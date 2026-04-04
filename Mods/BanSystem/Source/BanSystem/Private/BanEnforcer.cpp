// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/enforcer.ts

#include "BanEnforcer.h"
#include "BanDatabase.h"

// Pull in the full FUniqueNetIdRepl definition that was forward-declared in the header.
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY(LogBanEnforcer);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Super::Initialize(Collection);

    // Primary enforcement hook — AGameModeBase::PostLogin broadcasts this event
    // and CSS (confirmed by SML) calls it on every player join.
    // Note: CSS routes PreLogin through UFGDedicatedServerGameModeComponentInterface
    // rather than AGameModeBase::PreLogin, so FGameModeEvents::GameModePreLoginEvent
    // does not fire on CSS dedicated servers and is not hooked here.
    // Use AddLambda + TWeakObjectPtr instead of AddUObject to avoid C2665 in
    // CSS 5.3's strict template matching while still handling object lifetime safely.
    TWeakObjectPtr<UBanEnforcer> WeakThis(this);
    PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddLambda(
        [WeakThis](AGameModeBase* GameMode, APlayerController* NewPlayer)
        {
            if (UBanEnforcer* Enforcer = WeakThis.Get())
                Enforcer->OnPostLogin(GameMode, NewPlayer);
        });

    UE_LOG(LogBanEnforcer, Log, TEXT("BanEnforcer: login enforcement active (PostLogin)"));
}

void UBanEnforcer::Deinitialize()
{
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    PostLoginHandle.Reset();

    // Cancel any in-flight identity poll and clear the queue.
    PendingBanChecks.Empty();

    // Use the stored world reference because GetGameInstance()->GetWorld() may
    // already return null by the time Deinitialize() is called on shutdown.
    if (UWorld* World = PollTimerWorld.Get())
        World->GetTimerManager().ClearTimer(PollTimerHandle);
    PollTimerWorld.Reset();

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PostLogin enforcement (primary path on CSS dedicated servers)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
    if (!GameMode || !NewPlayer) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB)
    {
        UE_LOG(LogBanEnforcer, Error,
            TEXT("BanEnforcer: OnPostLogin — UBanDatabase subsystem unavailable, skipping ban check"));
        return;
    }

    UWorld* World = GI->GetWorld();
    if (!World) return;

    // Attempt an immediate ban check when PlayerState and identity are ready.
    // On CSS dedicated servers APlayerState::UniqueId is typically set during
    // AGameModeBase::Login() so this fast path fires for most Steam players.
    if (NewPlayer->PlayerState)
    {
        const FUniqueNetIdRepl& NetIdAtLogin = NewPlayer->PlayerState->GetUniqueId();
        if (NetIdAtLogin.IsValid())
        {
            // Defer the kick one tick so CSS's own PostLogin code can finish
            // before the connection is closed.  Closing synchronously inside the
            // PostLogin event can crash if AFGGameMode::PostLogin() runs more
            // setup code after Super::PostLogin() returns.
            TWeakObjectPtr<UBanEnforcer> WeakThis(this);
            TWeakObjectPtr<APlayerController> WeakPC(NewPlayer);
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateLambda([WeakThis, WeakPC]()
                {
                    UBanEnforcer* Enforcer = WeakThis.Get();
                    APlayerController* PC  = WeakPC.Get();
                    if (!Enforcer || !IsValid(PC)) return;

                    UGameInstance* GI2 = Enforcer->GetGameInstance();
                    if (!GI2) return;
                    UWorld* W2 = GI2->GetWorld();
                    UBanDatabase* DB2 = GI2->GetSubsystem<UBanDatabase>();
                    if (W2 && DB2)
                        Enforcer->PerformBanCheckForPlayer(W2, PC, DB2);
                }));
            return;
        }
    }

    // PlayerState is null OR identity not yet available.
    // CSS may initialise PlayerState asynchronously after PostLogin fires.
    // Queue the player and poll every 0.5 s until both PlayerState and identity
    // are available.
    PendingBanChecks.Add({NewPlayer, 0});

    if (!World->GetTimerManager().IsTimerActive(PollTimerHandle))
    {
        PollTimerWorld = World;
        World->GetTimerManager().SetTimer(
            PollTimerHandle,
            FTimerDelegate::CreateUObject(this, &UBanEnforcer::ProcessPendingBanChecks),
            0.5f,
            /*bLoop=*/true);

        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: started identity-poll timer for pending ban checks"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Identity poll timer  (fires every 0.5 s while there are pending checks)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::ProcessPendingBanChecks()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UWorld* World = GI->GetWorld();
    if (!World) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    // Iterate backwards so we can safely RemoveAt while iterating.
    for (int32 i = PendingBanChecks.Num() - 1; i >= 0; --i)
    {
        FPendingBanCheck& Check = PendingBanChecks[i];
        APlayerController* PC = Check.Player.Get();

        // Player already disconnected.
        if (!IsValid(PC))
        {
            PendingBanChecks.RemoveAt(i);
            continue;
        }

        // PlayerState not yet available — CSS may initialise it asynchronously
        // after PostLogin.  Treat this the same as an unresolved identity: burn
        // one attempt and keep the player in the queue.
        if (!PC->PlayerState)
        {
            if (++Check.Attempts >= FPendingBanCheck::MaxAttempts)
            {
                UE_LOG(LogBanEnforcer, Warning,
                    TEXT("BanEnforcer: gave up waiting for PlayerState for player after %d attempts (~%.0f s)"),
                    FPendingBanCheck::MaxAttempts,
                    FPendingBanCheck::MaxAttempts * 0.5f);
                PendingBanChecks.RemoveAt(i);
            }
            // else: PlayerState not ready yet — keep queued, retry next tick.
            continue;
        }

        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        if (!NetId.IsValid())
        {
            if (++Check.Attempts >= FPendingBanCheck::MaxAttempts)
            {
                UE_LOG(LogBanEnforcer, Warning,
                    TEXT("BanEnforcer: gave up waiting for UniqueId for player '%s' after %d attempts (~%.0f s)"),
                    *PC->PlayerState->GetPlayerName(),
                    FPendingBanCheck::MaxAttempts,
                    FPendingBanCheck::MaxAttempts * 0.5f);
                PendingBanChecks.RemoveAt(i);
            }
            // else: identity not ready yet — keep queued, retry next tick.
            continue;
        }

        // Identity is now valid — run the ban check and remove from queue.
        PendingBanChecks.RemoveAt(i);
        PerformBanCheckForPlayer(World, PC, DB);
    }

    // Stop the poll timer when the queue is drained to avoid unnecessary work.
    if (PendingBanChecks.IsEmpty())
    {
        World->GetTimerManager().ClearTimer(PollTimerHandle);
        PollTimerWorld.Reset();
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: identity-poll timer stopped (queue empty)"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban lookup + kick for a player whose identity has been confirmed valid
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::PerformBanCheckForPlayer(UWorld* World, APlayerController* PC, UBanDatabase* DB)
{
    if (!World || !IsValid(PC) || !DB || !PC->PlayerState) return;

    const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
    if (!NetId.IsValid()) return;

    const FString Platform = NetId->GetType().ToString().ToUpper();
    // Normalize EOS PUIDs to lowercase so they match bans stored by chat commands
    // (which also lowercase EOS PUIDs via the /ban and /tempban resolution paths).
    const FString RawId    = (Platform == TEXT("EOS"))
        ? NetId->ToString().ToLower()
        : NetId->ToString();
    const FString Uid      = UBanDatabase::MakeUid(Platform, RawId);

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: checking ban status for player '%s' (%s: %s)"),
        *PC->PlayerState->GetPlayerName(), *Platform, *RawId);

    FBanEntry Entry;
    if (!DB->IsCurrentlyBanned(Uid, Entry))
    {
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: player '%s' (%s) is not banned — allowing join"),
            *PC->PlayerState->GetPlayerName(), *Uid);
        return;
    }

    const FString KickMsg = Entry.GetKickMessage();
    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: kicking banned player %s (%s) — %s"),
        *RawId, *Platform, *KickMsg);

    // Try the standard session kick first (sends a kick message to the client).
    AGameModeBase* GM = World->GetAuthGameMode();
    if (GM && GM->GameSession)
    {
        GM->GameSession->KickPlayer(PC, FText::FromString(KickMsg));
    }

    // Hard fallback: close the net connection directly.
    // CSS's AFGGameSession::KickPlayer may not fully disconnect the client in all
    // server configurations.  Closing UNetConnection guarantees disconnection.
    // NOTE: Do NOT call PC->Destroy() — the connection close triggers the standard
    // UE cleanup path.  Explicitly destroying the PC before that runs can cause
    // crashes and ghost players.
    if (IsValid(PC))
    {
        if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
        {
            Conn->Close();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Kick helper
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::KickConnectedPlayer(UWorld* World, const FString& Uid, const FString& Reason)
{
    if (!World) return;

    AGameModeBase* GM = World->GetAuthGameMode();
    if (!GM || !GM->GameSession) return;

    // Parse the platform and raw player ID from the compound UID so we can fall back
    // to a second matching strategy when GetUniqueId() is not yet populated.
    FString UidPlatform, UidRawId;
    UBanDatabase::ParseUid(Uid, UidPlatform, UidRawId);

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState) continue;

        // ── Primary match: compound UID via FUniqueNetIdRepl ─────────────────
        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        if (NetId.IsValid())
        {
            const FString Platform   = NetId->GetType().ToString().ToUpper();
            // Normalize EOS PUIDs to lowercase to match stored ban UIDs.
            const FString RawId      = (Platform == TEXT("EOS"))
                ? NetId->ToString().ToLower()
                : NetId->ToString();
            const FString PlayerUid  = UBanDatabase::MakeUid(Platform, RawId);

            if (PlayerUid != Uid) continue;
        }
        else
        {
            // ── Fallback: CSS DS may not have set UniqueId yet.  Match by the
            //    raw player ID embedded in the UID against the PlayerState name
            //    (Steam players' display names often equal their Steam64 ID on
            //    fresh connection, but this is best-effort only).
            // If neither strategy finds the player we log a warning.
            const FString PlayerName = PC->PlayerState->GetPlayerName();
            if (!PlayerName.Equals(UidRawId, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogBanEnforcer, Verbose,
                    TEXT("BanEnforcer: KickConnectedPlayer — player '%s' has no valid UniqueId yet, skipping (UID mismatch)"),
                    *PlayerName);
                continue;
            }

            UE_LOG(LogBanEnforcer, Warning,
                TEXT("BanEnforcer: KickConnectedPlayer — matched '%s' by name fallback (UniqueId not yet set)"),
                *PlayerName);
        }

        GM->GameSession->KickPlayer(PC, FText::FromString(Reason));

        // Hard fallback: close the net connection directly in case
        // CSS's AFGGameSession::KickPlayer does not fully disconnect the client.
        // NOTE: Do NOT call PC->Destroy() — the connection close triggers the
        // standard UE cleanup path.  Explicitly destroying the PC before that
        // cleanup runs can cause crashes or leave ghost players.
        if (IsValid(PC))
        {
            if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
            {
                Conn->Close();
            }
        }

        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: kicked connected player %s — %s"), *Uid, *Reason);
        return;
    }

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: KickConnectedPlayer — no connected player found for UID %s"), *Uid);
}
