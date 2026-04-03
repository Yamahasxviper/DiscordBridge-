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

    // PlayerState may not be set yet on the very first frame; guard defensively.
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

    const FUniqueNetIdRepl& NetId = NewPlayer->PlayerState->GetUniqueId();
    if (!NetId.IsValid())
    {
        UE_LOG(LogBanEnforcer, Warning,
            TEXT("BanEnforcer: OnPostLogin — UniqueId not yet valid for '%s', skipping ban check"),
            *NewPlayer->PlayerState->GetPlayerName());
        return;
    }

    const FString Platform = NetId->GetType().ToString().ToUpper();
    const FString RawId    = NetId->ToString();
    const FString Uid      = UBanDatabase::MakeUid(Platform, RawId);

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: checking ban status for player '%s' (%s: %s)"),
        *NewPlayer->PlayerState->GetPlayerName(), *Platform, *RawId);

    FBanEntry Entry;
    if (!DB->IsCurrentlyBanned(Uid, Entry)) return;

    const FString KickMsg = Entry.GetKickMessage();
    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: scheduling deferred kick for banned player %s (%s) — %s"),
        *RawId, *Platform, *KickMsg);

    // Defer the kick to the next tick so that CSS's AFGGameMode::PostLogin and
    // UFGGameModeDSComponent::PostLogin have fully completed before we disconnect
    // the player.  Kicking synchronously inside the GameModePostLoginEvent callback
    // can corrupt the dedicated-server login state.
    UWorld* World = GameMode->GetWorld();
    if (!World) return;

    TWeakObjectPtr<UWorld>            WeakWorld(World);
    TWeakObjectPtr<APlayerController> WeakPlayer(NewPlayer);
    FString KickMsgCopy = KickMsg;

    World->GetTimerManager().SetTimerForNextTick([WeakWorld, WeakPlayer, KickMsgCopy]()
    {
        UWorld* W  = WeakWorld.Get();
        APlayerController* PC = WeakPlayer.Get();
        if (!W || !IsValid(PC)) return;  // world gone or player already disconnected

        // Try the standard session kick first (sends a "kicked" message to the client).
        AGameModeBase* GM = W->GetAuthGameMode();
        if (GM && GM->GameSession)
        {
            GM->GameSession->KickPlayer(PC, FText::FromString(KickMsgCopy));
        }

        // Hard fallback: close the net connection directly.
        // CSS's AFGGameSession::KickPlayer may not fully disconnect the client in all
        // server configurations.  Closing UNetConnection guarantees disconnection.
        // NOTE: Do NOT call PC->Destroy() here — the connection close triggers the
        // standard UE cleanup path that removes the PC from the game.  Explicitly
        // destroying the PC before that cleanup runs causes crashes and ghost players.
        if (IsValid(PC))
        {
            if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
            {
                Conn->Close();
            }
        }

        UE_LOG(LogBanEnforcer, Log, TEXT("BanEnforcer: deferred kick executed for banned player"));
    });
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
