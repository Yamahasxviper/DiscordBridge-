// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/enforcer.ts
// (PreLogin/PostLogin hooks replace the external DS-API polling loop)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanEnforcer.generated.h"

// Forward declaration to avoid pulling GameFramework/OnlineReplStructs.h into every
// translation unit that includes this header.  The CSS Alpakit server distribution
// does not ship OnlineReplStructs.h through the standalone Engine dep; it is only
// reachable via FactoryGame's transitive Engine include paths.
struct FUniqueNetIdRepl;

class UBanDatabase;

DECLARE_LOG_CATEGORY_EXTERN(LogBanEnforcer, Log, All);

class UWorld;

/**
 * Tracks a player whose PlayerState or platform identity was not yet available
 * at PostLogin time.  Retried every 0.5 s up to MaxAttempts times (~20 s total).
 */
struct FPendingBanCheck
{
    TWeakObjectPtr<APlayerController> Player;
    int32 Attempts = 0;
    static constexpr int32 MaxAttempts = 40; // 40 * 0.5s = 20 s
};

/**
 * UBanEnforcer
 *
 * Enforces bans at player login.  Direct port of the Enforcer class in
 * Tools/BanSystem/src/enforcer.ts, adapted for a UE server-side mod:
 *
 *   Tools/BanSystem approach: external service polls the DS HTTPS API
 *                              every N seconds and kicks banned players.
 *
 *   UE mod approach:          hook PreLogin (fires before the player
 *                              joins) to reject the connection outright
 *                              with the ban reason.  No polling needed.
 *
 * Supports both Steam and EOS player IDs.  The compound UID format
 * ("STEAM:xxx" / "EOS:xxx") matches the database schema exactly.
 */
UCLASS()
class BANSYSTEM_API UBanEnforcer : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Kick any currently-connected player whose compound UID matches.
     *
     * Must be called from the game thread.  Iterates connected
     * PlayerControllers, resolves each player's platform UID (same format
     * as stored in UBanDatabase: "STEAM:xxx" / "EOS:xxx"), and kicks the
     * first matching player.  First tries AGameSession::KickPlayer, then
     * falls back to closing the UNetConnection directly to handle CSS
     * dedicated-server configurations where the session kick does not fully
     * disconnect the client.  Does NOT call PC->Destroy() — the connection
     * close triggers the standard UE cleanup path.
     *
     * If a player's FUniqueNetIdRepl is not yet populated (CSS DS async
     * identity), the raw player ID embedded in the UID is compared against the
     * PlayerState display name as a best-effort fallback.
     */
    static void KickConnectedPlayer(UWorld* World, const FString& Uid, const FString& Reason);

private:
    /**
     * FGameModeEvents::GameModePostLoginEvent hook — primary ban enforcement
     * path on CSS dedicated servers.
     *
     * CSS's AFGGameMode::Login() routes authentication through
     * UFGDedicatedServerGameModeComponentInterface::PreLogin rather than the
     * standard AGameModeBase::PreLogin, so GameModePreLoginEvent may never
     * fire.  GameModePostLoginEvent is broadcast by AGameModeBase::PostLogin
     * which CSS does call (SML relies on it).
     *
     * If the player's FUniqueNetIdRepl is already valid at PostLogin time
     * (the common case for Steam players on CSS DS), the ban check runs
     * immediately and synchronously.  Otherwise the player is queued for
     * identity polling every 0.5 s up to MaxAttempts ticks (~20 s) while
     * the async Server_RegisterControllerComponent RPC populates the identity.
     */
    void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

    /**
     * Timer callback fired every 0.5 s while there are pending ban checks.
     * For each queued player it checks whether their FUniqueNetIdRepl is now
     * valid.  If so, the ban lookup is performed and the player is kicked if
     * banned.  Players whose identity does not arrive within MaxAttempts ticks
     * (~20 s) are dropped from the queue with a warning.
     */
    void ProcessPendingBanChecks();

    /**
     * Performs the actual ban database lookup and kick for a player whose
     * platform identity has been confirmed valid.  Safe to call from the game
     * thread only.
     */
    void PerformBanCheckForPlayer(UWorld* World, APlayerController* PC, UBanDatabase* DB);

    FDelegateHandle PostLoginHandle;

    /** Players queued for PlayerState / identity polling (CSS async init). */
    TArray<FPendingBanCheck> PendingBanChecks;

    /** Looping 0.5 s timer that drives ProcessPendingBanChecks(). Active only
     *  while PendingBanChecks is non-empty. */
    FTimerHandle PollTimerHandle;

    /** Weak reference to the world the poll timer was registered on.
     *  Kept so Deinitialize() can clear the timer even if GetWorld() is
     *  already null at that point. */
    TWeakObjectPtr<UWorld> PollTimerWorld;
};
