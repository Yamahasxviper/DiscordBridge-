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

    // Best-effort PreLogin hook — may not fire on CSS dedicated servers because
    // AFGGameMode::Login() routes through UFGDedicatedServerGameModeComponentInterface::PreLogin
    // rather than the standard AGameModeBase::PreLogin path.  Kept for completeness.
    PreLoginHandle = FGameModeEvents::GameModePreLoginEvent.AddUObject(
        this, &UBanEnforcer::OnPreLogin);

    // Primary enforcement hook — AGameModeBase::PostLogin broadcasts this event
    // and CSS (confirmed by SML) calls it on every player join.  Any banned player
    // that was not caught at PreLogin is kicked here immediately.
    PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
        this, &UBanEnforcer::OnPostLogin);

    UE_LOG(LogBanEnforcer, Log, TEXT("BanEnforcer: login enforcement active (PreLogin + PostLogin)"));
}

void UBanEnforcer::Deinitialize()
{
    FGameModeEvents::GameModePreLoginEvent.Remove(PreLoginHandle);
    PreLoginHandle.Reset();

    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    PostLoginHandle.Reset();

    // Cancel any in-flight identity poll and clear the queue.
    PendingBanChecks.Empty();
    if (UGameInstance* GI = GetGameInstance())
    {
        if (UWorld* World = GI->GetWorld())
            World->GetTimerManager().ClearTimer(PollTimerHandle);
    }

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enforcement
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::OnPreLogin(AGameModeBase* /*GameMode*/,
                               const FUniqueNetIdRepl& UniqueId,
                               const FString& /*Options*/,
                               FString& ErrorMessage)
{
    // If another system already rejected this login, don't overwrite.
    if (!ErrorMessage.IsEmpty()) return;

    if (!UniqueId.IsValid()) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;
    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    // Build the compound UID.  The FUniqueNetId type name is "STEAM" or "EOS"
    // on the CSS dedicated server; we upper-case it to match the stored format.
    const FString Platform = UniqueId->GetType().ToString().ToUpper();
    const FString RawId    = UniqueId->ToString();
    const FString Uid      = UBanDatabase::MakeUid(Platform, RawId);

    FBanEntry Entry;
    if (DB->IsCurrentlyBanned(Uid, Entry))
    {
        ErrorMessage = Entry.GetKickMessage();
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: rejected %s (%s) at PreLogin — %s"),
            *RawId, *Platform, *ErrorMessage);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  PostLogin enforcement (primary path on CSS dedicated servers)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
    if (!GameMode || !NewPlayer) return;

    if (!NewPlayer->PlayerState)
    {
        UE_LOG(LogBanEnforcer, Warning,
            TEXT("BanEnforcer: OnPostLogin — PlayerState is null, skipping ban check"));
        return;
    }

    UBanDatabase* DB = nullptr;
    if (UGameInstance* GI = GetGameInstance())
        DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB)
    {
        UE_LOG(LogBanEnforcer, Error,
            TEXT("BanEnforcer: OnPostLogin — UBanDatabase subsystem unavailable, skipping ban check"));
        return;
    }

    UWorld* World = GameMode->GetWorld();
    if (!World) return;

    // CSS dedicated servers populate FUniqueNetIdRepl asynchronously after
    // PostLogin via a Server_RegisterControllerComponent RPC.  The identity is
    // therefore often not yet valid at this point.  Instead of checking once
    // and bailing out, queue the player for polling every 0.5 s so we catch the
    // identity as soon as it arrives (up to FPendingBanCheck::MaxAttempts ticks,
    // i.e. ~10 s).
    PendingBanChecks.Add({NewPlayer, 0});

    if (!World->GetTimerManager().IsTimerActive(PollTimerHandle))
    {
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
        if (!IsValid(PC) || !PC->PlayerState)
        {
            PendingBanChecks.RemoveAt(i);
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
    const FString RawId    = NetId->ToString();
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

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState) continue;

        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        if (!NetId.IsValid()) continue;

        const FString Platform   = NetId->GetType().ToString().ToUpper();
        const FString RawId      = NetId->ToString();
        const FString PlayerUid  = UBanDatabase::MakeUid(Platform, RawId);

        if (PlayerUid == Uid)
        {
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
    }
}
