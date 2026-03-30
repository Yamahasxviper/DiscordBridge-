// Copyright Yamahasxviper. All Rights Reserved.

#include "BanEnforcementSubsystem.h"
#include "BanIdResolver.h"
#include "BanDiscordSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
// EOSSystem — cross-platform PUID↔Steam64 lookup
#include "EOSConnectSubsystem.h"
#include "EOSTypes.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanEnforcement, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  Lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // Subscribe to global GameMode events for PreLogin/PostLogin enforcement.
    // Using AddUObject ensures automatic cleanup if this subsystem is destroyed.
    PreLoginHandle = FGameModeEvents::GameModePreLoginEvent.AddUObject(
        this, &UBanEnforcementSubsystem::OnPreLogin);

    PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
        this, &UBanEnforcementSubsystem::OnPostLogin);

    // Subscribe to GameMode logout events for session cleanup.
    LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(
        this, &UBanEnforcementSubsystem::OnLogout);

    // Subscribe to EOSSystem's PUID events for async join-time enforcement and
    // cross-platform ban propagation.  EOSSystem is optional — if not installed
    // the subsystem still works for Steam-only ban enforcement.
    if (UEOSConnectSubsystem* EOS = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>())
    {
        EOS->OnPlayerPUIDRegistered.AddDynamic(this, &UBanEnforcementSubsystem::OnPUIDRegistered);
        EOS->OnPUIDLookupComplete.AddDynamic(this,   &UBanEnforcementSubsystem::OnPUIDLookupDone);
        EOS->OnReverseLookupComplete.AddDynamic(this, &UBanEnforcementSubsystem::OnReverseLookupDone);

        UE_LOG(LogBanEnforcement, Log,
            TEXT("BanEnforcementSubsystem: Subscribed to EOSSystem events — "
                 "async PUID lookup and cross-platform propagation enabled."));
    }
    else
    {
        UE_LOG(LogBanEnforcement, Log,
            TEXT("BanEnforcementSubsystem: EOSSystem not available — "
                 "async PUID lookups and cross-platform ban propagation disabled."));
    }

    // Subscribe to ban-issued events so that a newly-banned player who is
    // currently connected gets kicked immediately rather than being allowed
    // to keep playing until they disconnect and reconnect.
    if (USteamBanSubsystem* SteamBans = GetGameInstance()->GetSubsystem<USteamBanSubsystem>())
    {
        SteamBans->OnPlayerBanned.AddDynamic(this, &UBanEnforcementSubsystem::OnSteamPlayerBanned);
    }
    if (UEOSBanSubsystem* EOSBans = GetGameInstance()->GetSubsystem<UEOSBanSubsystem>())
    {
        EOSBans->OnPlayerBanned.AddDynamic(this, &UBanEnforcementSubsystem::OnEOSPlayerBanned);
    }

    UE_LOG(LogBanEnforcement, Log,
        TEXT("BanEnforcementSubsystem: Initialized. "
             "PreLogin + PostLogin + Logout ban enforcement active."));
}

void UBanEnforcementSubsystem::Deinitialize()
{
    // Unsubscribe from global GameMode events.
    FGameModeEvents::GameModePreLoginEvent.Remove(PreLoginHandle);
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
    PreLoginHandle.Reset();
    PostLoginHandle.Reset();
    LogoutHandle.Reset();

    // Unsubscribe from EOSSystem and ban-issued events.
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UEOSConnectSubsystem* EOS = GI->GetSubsystem<UEOSConnectSubsystem>())
        {
            EOS->OnPlayerPUIDRegistered.RemoveDynamic(this, &UBanEnforcementSubsystem::OnPUIDRegistered);
            EOS->OnPUIDLookupComplete.RemoveDynamic(this,   &UBanEnforcementSubsystem::OnPUIDLookupDone);
            EOS->OnReverseLookupComplete.RemoveDynamic(this, &UBanEnforcementSubsystem::OnReverseLookupDone);
        }

        if (USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>())
        {
            SteamBans->OnPlayerBanned.RemoveDynamic(this, &UBanEnforcementSubsystem::OnSteamPlayerBanned);
        }
        if (UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>())
        {
            EOSBans->OnPlayerBanned.RemoveDynamic(this, &UBanEnforcementSubsystem::OnEOSPlayerBanned);
        }
    }

    PendingBySteam64.Empty();
    PendingEOSPropagation.Empty();
    PendingSteamPropagation.Empty();
    PendingEOSUnban.Empty();
    PendingSteamUnban.Empty();

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PreLogin enforcement (rejects connection before PlayerController is created)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnPreLogin(AGameModeBase*          GameMode,
                                           const FUniqueNetIdRepl& UniqueId,
                                           FString&                ErrorMessage)
{
    // Preserve an existing error message set by another mod.
    if (!ErrorMessage.IsEmpty() || !GameMode) return;

    UGameInstance* GI = GameMode->GetGameInstance();
    if (!GI) return;

    const FResolvedBanId ResolvedId = FBanIdResolver::Resolve(UniqueId);
    if (!ResolvedId.IsValid())
    {
        UE_LOG(LogBanEnforcement, Verbose,
            TEXT("PreLogin: no ban-relevant platform ID for type '%s' — skipping."),
            *FBanIdResolver::GetIdTypeName(UniqueId));
        return;
    }

    // ── Steam ban check ────────────────────────────────────────────────────
    if (ResolvedId.HasSteamId())
    {
        USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
        if (SteamBans)
        {
            FString Reason;
            if (SteamBans->IsPlayerBanned(ResolvedId.Steam64Id, Reason))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("PreLogin: rejecting Steam-banned player %s — Reason: %s"),
                    *ResolvedId.Steam64Id, *Reason);
                ErrorMessage = BuildKickMessage(false, Reason);
                PostBanKickDiscordNotification(ResolvedId.Steam64Id, Reason);
                return;
            }
        }
    }

    // ── EOS ban check ──────────────────────────────────────────────────────
    if (ResolvedId.HasEOSPuid())
    {
        UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
        if (EOSBans)
        {
            FString Reason;
            if (EOSBans->IsPlayerBanned(ResolvedId.EOSProductUserId, Reason))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("PreLogin: rejecting EOS-banned player %s — Reason: %s"),
                    *ResolvedId.EOSProductUserId, *Reason);
                ErrorMessage = BuildKickMessage(true, Reason);
                PostBanKickDiscordNotification(ResolvedId.EOSProductUserId, Reason);
                return;
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PostLogin enforcement + async PUID lookup trigger
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnPostLogin(AGameModeBase*    GameMode,
                                            APlayerController* NewPlayer)
{
    if (!GameMode || !NewPlayer || !NewPlayer->PlayerState) return;

    UGameInstance* GI = GameMode->GetGameInstance();
    if (!GI) return;

    const FUniqueNetIdRepl UniqueId = NewPlayer->PlayerState->GetUniqueId();
    if (!UniqueId.IsValid()) return;

    const FResolvedBanId ResolvedId = FBanIdResolver::Resolve(UniqueId);
    if (!ResolvedId.IsValid())
    {
        UE_LOG(LogBanEnforcement, Verbose,
            TEXT("PostLogin: no ban-relevant platform ID for type '%s' — skipping."),
            *FBanIdResolver::GetIdTypeName(UniqueId));
        return;
    }

    // ── Steam ban check ────────────────────────────────────────────────────
    if (ResolvedId.HasSteamId())
    {
        USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
        if (SteamBans)
        {
            FString Reason;
            if (SteamBans->IsPlayerBanned(ResolvedId.Steam64Id, Reason))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("PostLogin: kicking Steam-banned player %s — Reason: %s"),
                    *ResolvedId.Steam64Id, *Reason);
                KickBannedPlayer(NewPlayer, ResolvedId.Steam64Id, BuildKickMessage(false, Reason));
                PostBanKickDiscordNotification(ResolvedId.Steam64Id, Reason);
                return;
            }
        }
    }

    // ── EOS ban check ──────────────────────────────────────────────────────
    if (ResolvedId.HasEOSPuid())
    {
        UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
        if (EOSBans)
        {
            FString Reason;
            if (EOSBans->IsPlayerBanned(ResolvedId.EOSProductUserId, Reason))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("PostLogin: kicking EOS-banned player %s — Reason: %s"),
                    *ResolvedId.EOSProductUserId, *Reason);
                KickBannedPlayer(NewPlayer, ResolvedId.EOSProductUserId, BuildKickMessage(true, Reason));
                PostBanKickDiscordNotification(ResolvedId.EOSProductUserId, Reason);
                return;
            }
        }

        // PUID is present and player is not banned.
        return;
    }

    // ── Async EOS PUID lookup for Steam-only players ───────────────────────
    // If this player has a Steam ID but no EOS PUID has been resolved yet,
    // try the EOSSystem sync cache first (free, no network call).  If not
    // cached, trigger an async Steam64→PUID lookup.  When the result arrives
    // via OnPUIDLookupDone, the EOS ban list will be checked automatically.
    if (ResolvedId.HasSteamId() && !ResolvedId.HasEOSPuid())
    {
        UEOSConnectSubsystem* EOS = GI->GetSubsystem<UEOSConnectSubsystem>();
        if (!EOS) return;

        // Fast path: check the sync cache first.
        FString CachedPUID;
        if (FBanIdResolver::TryGetCachedPUIDFromSteam64(GI, ResolvedId.Steam64Id, CachedPUID))
        {
            UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
            if (EOSBans)
            {
                FString Reason;
                if (EOSBans->IsPlayerBanned(CachedPUID, Reason))
                {
                    UE_LOG(LogBanEnforcement, Log,
                        TEXT("PostLogin: kicking EOS-banned Steam player %s (cached PUID %s) — Reason: %s"),
                        *ResolvedId.Steam64Id, *CachedPUID, *Reason);
                    KickBannedPlayer(NewPlayer, CachedPUID, BuildKickMessage(true, Reason));
                    PostBanKickDiscordNotification(CachedPUID, Reason);
                    return;
                }
            }
            // Cached PUID is not banned.
        }
        else
        {
            // No cache hit — add to pending map and trigger async lookup.
            // OnPUIDLookupDone will handle the result.
            FPendingEOSCheck& Pending = PendingBySteam64.FindOrAdd(ResolvedId.Steam64Id);
            Pending.Controller = NewPlayer;
            Pending.Steam64Id  = ResolvedId.Steam64Id;

            EOS->LookupPUIDBySteam64(ResolvedId.Steam64Id);

            UE_LOG(LogBanEnforcement, Verbose,
                TEXT("PostLogin: async EOS PUID lookup triggered for Steam player %s — "
                     "will check EOS ban list when result arrives."),
                *ResolvedId.Steam64Id);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PUID registered — catch late EOS PUID registrations after PostLogin
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnPUIDRegistered(const FString& PUID, APlayerController* Controller)
{
    if (PUID.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();

    // ── Fast path: use the PlayerController supplied by EOSConnectSubsystem ──
    // HandlePostLogin now extracts the PUID via UE::Online::GetProductUserId()
    // and passes the matching PC directly, so we can skip the world scan.
    APlayerController* PC = Controller;

    // ── Fallback: world scan (manual RegisterPlayerPUID calls, PC == nullptr) ─
    // This path is a safety net for calls that don't originate from PostLogin.
    // It relies on FBanIdResolver::Resolve() succeeding (V2 FAccountId players).
    if (!PC)
    {
        UWorld* World = GI->GetWorld();
        if (!World) return;

        for (TPlayerControllerIterator<APlayerController>::ServerAll It(World); It; ++It)
        {
            APlayerController* Candidate = *It;
            if (!Candidate || !Candidate->PlayerState) continue;

            FResolvedBanId Ids = FBanIdResolver::Resolve(Candidate->PlayerState->GetUniqueId());
            if (Ids.EOSProductUserId == PUID)
            {
                PC = Candidate;
                break;
            }
        }
    }

    if (!PC) return;  // Player not (yet) in world — ban will be caught by PostLogin

    // ── EOS ban check ──────────────────────────────────────────────────────
    if (EOSBans)
    {
        FString Reason;
        if (EOSBans->IsPlayerBanned(PUID, Reason))
        {
            UE_LOG(LogBanEnforcement, Log,
                TEXT("OnPUIDRegistered: kicking EOS-banned player (PUID %s)."), *PUID);
            KickBannedPlayer(PC, PUID, BuildKickMessage(true, Reason));
            PostBanKickDiscordNotification(PUID, Reason);
            return;
        }
    }

    // Player is not banned.
}

// ─────────────────────────────────────────────────────────────────────────────
//  Async PUID lookup completed
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnPUIDLookupDone(const FString& ExternalAccountId,
                                                  const FString& PUID)
{
    if (PUID.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();

    // ── Join-time enforcement ──────────────────────────────────────────────
    // Check whether we have a pending ban check waiting on this Steam64 ID's PUID.
    if (FPendingEOSCheck* Pending = PendingBySteam64.Find(ExternalAccountId))
    {
        APlayerController* PC = Pending->Controller.Get();
        PendingBySteam64.Remove(ExternalAccountId);

        if (PC)
        {
            UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
            if (EOSBans)
            {
                FString Reason;
                if (EOSBans->IsPlayerBanned(PUID, Reason))
                {
                    UE_LOG(LogBanEnforcement, Log,
                        TEXT("OnPUIDLookupDone: kicking EOS-banned player "
                             "(PUID %s, Steam64 %s) — Reason: %s"),
                        *PUID, *ExternalAccountId, *Reason);
                    KickBannedPlayer(PC, PUID, BuildKickMessage(true, Reason));
                    PostBanKickDiscordNotification(PUID, Reason);
                }
                else
                {
                    // Player is not banned.
                    UE_LOG(LogBanEnforcement, Verbose,
                        TEXT("OnPUIDLookupDone: PUID %s confirmed not banned."), *PUID);
                }
            }
        }
    }

    // ── Cross-platform ban propagation ─────────────────────────────────────
    // Check whether a Steam ban was issued for this Steam64 ID and we should
    // now apply the complementary EOS ban.
    if (FPropagationEntry* Entry = PendingEOSPropagation.Find(ExternalAccountId))
    {
        const FPropagationEntry Prop = *Entry;
        PendingEOSPropagation.Remove(ExternalAccountId);

        UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
        if (EOSBans)
        {
            if (EOSBans->BanPlayer(PUID, Prop.Reason, Prop.DurationMinutes, Prop.BannedBy))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("OnPUIDLookupDone: cross-platform EOS ban applied — "
                         "PUID %s (from Steam64 %s)."),
                    *PUID, *ExternalAccountId);
            }
            else
            {
                UE_LOG(LogBanEnforcement, Warning,
                    TEXT("OnPUIDLookupDone: failed to apply cross-platform EOS ban for PUID %s."),
                    *PUID);
            }
        }
    }

    // ── Cross-platform unban propagation ───────────────────────────────────
    // Check whether a Steam unban was issued for this Steam64 ID and we should
    // now remove the complementary EOS ban.
    if (PendingEOSUnban.Contains(ExternalAccountId))
    {
        PendingEOSUnban.Remove(ExternalAccountId);

        UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
        if (EOSBans)
        {
            if (EOSBans->UnbanPlayer(PUID))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("OnPUIDLookupDone: cross-platform EOS unban applied — "
                         "PUID %s (from Steam64 %s)."),
                    *PUID, *ExternalAccountId);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Async reverse lookup completed (PUID → external accounts)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnReverseLookupDone(const FString&          PUID,
                                                     const FString&          ExternalAccountId,
                                                     EEOSExternalAccountType AccountType)
{
    // We only need Steam accounts for cross-platform Steam ban propagation.
    if (AccountType != EEOSExternalAccountType::Steam || ExternalAccountId.IsEmpty()) return;

    UGameInstance* GI = GetGameInstance();

    if (FPropagationEntry* Entry = PendingSteamPropagation.Find(PUID))
    {
        const FPropagationEntry Prop = *Entry;
        PendingSteamPropagation.Remove(PUID);

        if (!USteamBanSubsystem::IsValidSteam64Id(ExternalAccountId)) return;

        USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
        if (SteamBans)
        {
            if (SteamBans->BanPlayer(ExternalAccountId, Prop.Reason, Prop.DurationMinutes, Prop.BannedBy))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("OnReverseLookupDone: cross-platform Steam ban applied — "
                         "Steam64 %s (from PUID %s)."),
                    *ExternalAccountId, *PUID);
            }
            else
            {
                UE_LOG(LogBanEnforcement, Warning,
                    TEXT("OnReverseLookupDone: failed to apply cross-platform Steam ban for %s."),
                    *ExternalAccountId);
            }
        }
    }

    // ── Cross-platform unban propagation ───────────────────────────────────
    // Check whether an EOS unban was issued for this PUID and we should now
    // remove the complementary Steam ban.
    if (PendingSteamUnban.Contains(PUID))
    {
        PendingSteamUnban.Remove(PUID);

        if (!USteamBanSubsystem::IsValidSteam64Id(ExternalAccountId)) return;

        USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
        if (SteamBans)
        {
            if (SteamBans->UnbanPlayer(ExternalAccountId))
            {
                UE_LOG(LogBanEnforcement, Log,
                    TEXT("OnReverseLookupDone: cross-platform Steam unban applied — "
                         "Steam64 %s (from PUID %s)."),
                    *ExternalAccountId, *PUID);
            }
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Logout — clean up EOS session registrations on player disconnect
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnLogout(AGameModeBase* GameMode, AController* Controller)
{
    APlayerController* PC = Cast<APlayerController>(Controller);
    if (!PC) return;

    // Remove any pending ban checks for this PC (player disconnected before
    // the async PUID lookup finished).
    for (auto It = PendingBySteam64.CreateIterator(); It; ++It)
    {
        if (It.Value().Controller.Get() == PC)
        {
            It.RemoveCurrent();
            break;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban-issued callbacks — kick currently-connected banned players immediately
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::OnSteamPlayerBanned(const FString& Steam64Id,
                                                     const FBanEntry& Entry)
{
    if (Steam64Id.IsEmpty()) return;

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return;

    for (TPlayerControllerIterator<APlayerController>::ServerAll It(World); It; ++It)
    {
        APlayerController* PC = *It;
        if (!PC || !PC->PlayerState) continue;

        FResolvedBanId Ids = FBanIdResolver::Resolve(PC->PlayerState->GetUniqueId());
        if (Ids.Steam64Id == Steam64Id)
        {
            UE_LOG(LogBanEnforcement, Log,
                TEXT("OnSteamPlayerBanned: kicking currently-connected Steam-banned player %s — Reason: %s"),
                *Steam64Id, *Entry.Reason);
            KickBannedPlayer(PC, Steam64Id, BuildKickMessage(false, Entry.Reason));
            PostBanKickDiscordNotification(Steam64Id, Entry.Reason);
            return;
        }
    }
}

void UBanEnforcementSubsystem::OnEOSPlayerBanned(const FString& EOSProductUserId,
                                                   const FBanEntry& Entry)
{
    if (EOSProductUserId.IsEmpty()) return;

    UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
    if (!World) return;

    for (TPlayerControllerIterator<APlayerController>::ServerAll It(World); It; ++It)
    {
        APlayerController* PC = *It;
        if (!PC || !PC->PlayerState) continue;

        FResolvedBanId Ids = FBanIdResolver::Resolve(PC->PlayerState->GetUniqueId());
        if (Ids.EOSProductUserId == EOSProductUserId)
        {
            UE_LOG(LogBanEnforcement, Log,
                TEXT("OnEOSPlayerBanned: kicking currently-connected EOS-banned player %s — Reason: %s"),
                *EOSProductUserId, *Entry.Reason);
            KickBannedPlayer(PC, EOSProductUserId, BuildKickMessage(true, Entry.Reason));
            PostBanKickDiscordNotification(EOSProductUserId, Entry.Reason);
            return;
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cross-platform propagation — public API
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::PropagateToEOSAsync(const FString& Steam64Id,
                                                     const FString& Reason,
                                                     int32          DurationMinutes,
                                                     const FString& BannedBy)
{
    UGameInstance* GI = GetGameInstance();
    UEOSConnectSubsystem* EOS = GI ? GI->GetSubsystem<UEOSConnectSubsystem>() : nullptr;
    if (!EOS) return;

    // Fast path: check the sync cache first.
    FString CachedPUID;
    if (FBanIdResolver::TryGetCachedPUIDFromSteam64(GI, Steam64Id, CachedPUID))
    {
        UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
        if (EOSBans)
        {
            EOSBans->BanPlayer(CachedPUID, Reason, DurationMinutes, BannedBy);
            UE_LOG(LogBanEnforcement, Log,
                TEXT("PropagateToEOSAsync: cache hit — EOS ban applied for PUID %s (Steam64 %s)."),
                *CachedPUID, *Steam64Id);
        }
        return;
    }

    // No cache hit — schedule the propagation and trigger an async EOS lookup.
    FPropagationEntry& Entry = PendingEOSPropagation.FindOrAdd(Steam64Id);
    Entry.Reason          = Reason;
    Entry.DurationMinutes = DurationMinutes;
    Entry.BannedBy        = BannedBy;

    EOS->LookupPUIDBySteam64(Steam64Id);

    UE_LOG(LogBanEnforcement, Log,
        TEXT("PropagateToEOSAsync: async PUID lookup triggered for Steam64 %s."), *Steam64Id);
}

void UBanEnforcementSubsystem::PropagateToSteamAsync(const FString& PUID,
                                                       const FString& Reason,
                                                       int32          DurationMinutes,
                                                       const FString& BannedBy)
{
    UGameInstance* GI = GetGameInstance();
    UEOSConnectSubsystem* EOS = GI ? GI->GetSubsystem<UEOSConnectSubsystem>() : nullptr;
    if (!EOS) return;

    // Fast path: check the sync cache first.
    FString CachedSteam64;
    if (FBanIdResolver::TryGetCachedSteam64FromPUID(GI, PUID, CachedSteam64))
    {
        USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
        if (SteamBans)
        {
            SteamBans->BanPlayer(CachedSteam64, Reason, DurationMinutes, BannedBy);
            UE_LOG(LogBanEnforcement, Log,
                TEXT("PropagateToSteamAsync: cache hit — Steam ban applied for %s (PUID %s)."),
                *CachedSteam64, *PUID);
        }
        return;
    }

    // No cache hit — schedule the propagation and trigger an async reverse lookup.
    FPropagationEntry& Entry = PendingSteamPropagation.FindOrAdd(PUID);
    Entry.Reason          = Reason;
    Entry.DurationMinutes = DurationMinutes;
    Entry.BannedBy        = BannedBy;

    EOS->QueryExternalAccountsForPUID(PUID);

    UE_LOG(LogBanEnforcement, Log,
        TEXT("PropagateToSteamAsync: async reverse lookup triggered for PUID %s."), *PUID);
}

void UBanEnforcementSubsystem::PropagateUnbanToEOSAsync(const FString& Steam64Id)
{
    UGameInstance* GI = GetGameInstance();
    UEOSConnectSubsystem* EOS = GI ? GI->GetSubsystem<UEOSConnectSubsystem>() : nullptr;
    if (!EOS) return;

    // Fast path: check the sync cache first.
    FString CachedPUID;
    if (FBanIdResolver::TryGetCachedPUIDFromSteam64(GI, Steam64Id, CachedPUID))
    {
        UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
        if (EOSBans)
        {
            EOSBans->UnbanPlayer(CachedPUID);
            UE_LOG(LogBanEnforcement, Log,
                TEXT("PropagateUnbanToEOSAsync: cache hit — EOS unban applied for PUID %s (Steam64 %s)."),
                *CachedPUID, *Steam64Id);
        }
        return;
    }

    // No cache hit — schedule the unban and trigger an async EOS lookup.
    PendingEOSUnban.Add(Steam64Id);
    EOS->LookupPUIDBySteam64(Steam64Id);

    UE_LOG(LogBanEnforcement, Log,
        TEXT("PropagateUnbanToEOSAsync: async PUID lookup triggered for Steam64 %s."), *Steam64Id);
}

void UBanEnforcementSubsystem::PropagateUnbanToSteamAsync(const FString& PUID)
{
    UGameInstance* GI = GetGameInstance();
    UEOSConnectSubsystem* EOS = GI ? GI->GetSubsystem<UEOSConnectSubsystem>() : nullptr;
    if (!EOS) return;

    // Fast path: check the sync cache first.
    FString CachedSteam64;
    if (FBanIdResolver::TryGetCachedSteam64FromPUID(GI, PUID, CachedSteam64))
    {
        USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
        if (SteamBans)
        {
            SteamBans->UnbanPlayer(CachedSteam64);
            UE_LOG(LogBanEnforcement, Log,
                TEXT("PropagateUnbanToSteamAsync: cache hit — Steam unban applied for %s (PUID %s)."),
                *CachedSteam64, *PUID);
        }
        return;
    }

    // No cache hit — schedule the unban and trigger an async reverse lookup.
    PendingSteamUnban.Add(PUID);
    EOS->QueryExternalAccountsForPUID(PUID);

    UE_LOG(LogBanEnforcement, Log,
        TEXT("PropagateUnbanToSteamAsync: async reverse lookup triggered for PUID %s."), *PUID);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcementSubsystem::KickBannedPlayer(APlayerController* Controller,
                                                  const FString&     PlayerId,
                                                  const FString&     KickMessage)
{
    if (!Controller) return;

    UWorld* World = Controller->GetWorld();
    if (!World) return;

    AGameModeBase* GM = World->GetAuthGameMode();
    if (!GM || !GM->GameSession) return;

    GM->GameSession->KickPlayer(Controller, FText::FromString(KickMessage));
}

FString UBanEnforcementSubsystem::BuildKickMessage(bool bIsEOSBan,
                                                     const FString& Reason) const
{
    // Check for a custom kick reason override in BanDiscordSubsystem config.
    UGameInstance* GI = GetGameInstance();
    UBanDiscordSubsystem* BanDiscord =
        GI ? GI->GetSubsystem<UBanDiscordSubsystem>() : nullptr;
    if (BanDiscord)
    {
        const FString& Override = bIsEOSBan
            ? BanDiscord->GetConfig().EOSBanKickReason
            : BanDiscord->GetConfig().SteamBanKickReason;
        if (!Override.IsEmpty())
            return Override;
    }

    return FString::Printf(TEXT("[%s Ban] %s"),
        bIsEOSBan ? TEXT("EOS") : TEXT("Steam"), *Reason);
}

void UBanEnforcementSubsystem::PostBanKickDiscordNotification(const FString& PlayerId,
                                                               const FString& Reason) const
{
    UGameInstance* GI = GetGameInstance();
    UBanDiscordSubsystem* BanDiscord =
        GI ? GI->GetSubsystem<UBanDiscordSubsystem>() : nullptr;
    if (!BanDiscord) return;

    const FString& NotifyFmt = BanDiscord->GetConfig().BanKickDiscordMessage;
    const FString& ChannelId = BanDiscord->GetConfig().DiscordChannelId;
    if (NotifyFmt.IsEmpty() || ChannelId.IsEmpty()) return;

    FString Notice = NotifyFmt;
    Notice = Notice.Replace(TEXT("%PlayerId%"), *PlayerId);
    Notice = Notice.Replace(TEXT("%Reason%"),   *Reason);
    BanDiscord->SendDiscordChannelMessage(ChannelId, Notice);
}
