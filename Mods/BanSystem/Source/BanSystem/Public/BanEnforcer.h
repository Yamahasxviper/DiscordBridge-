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

DECLARE_LOG_CATEGORY_EXTERN(LogBanEnforcer, Log, All);

class UWorld;

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
     * falls back to closing the UNetConnection and destroying the PC directly
     * to handle CSS dedicated-server configurations where the session kick
     * does not fully disconnect the client.
     */
    static void KickConnectedPlayer(UWorld* World, const FString& Uid, const FString& Reason);

private:
    /**
     * FGameModeEvents::GameModePreLoginEvent best-effort hook.
     * Fires from AGameModeBase::PreLogin in standard UE.
     * CSS may or may not broadcast this depending on the AFGGameMode::Login
     * override; the PostLogin hook below is the primary enforcement path.
     */
    void OnPreLogin(AGameModeBase* GameMode,
                    const FUniqueNetIdRepl& UniqueId,
                    const FString& Options,
                    FString& ErrorMessage);

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
     * The kick is deferred to the next tick so that AFGGameMode::PostLogin
     * and UFGGameModeDSComponent::PostLogin have fully completed before the
     * player is disconnected.  Kicking inside the PostLogin callback
     * synchronously can disrupt CSS's dedicated-server login state.
     */
    void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

    FDelegateHandle PreLoginHandle;
    FDelegateHandle PostLoginHandle;
};
