// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FBanSystemModule
 *
 * Main entry-point for the BanSystem Alpakit mod.
 *
 * On startup:
 *   - Installs a hook on AGameModeBase::PreLogin to reject banned players
 *     before their connection is established (primary enforcement).
 *   - Installs a hook on AGameModeBase::PostLogin as a secondary fallback
 *     to kick any banned player whose IDs could not be resolved at PreLogin.
 *   - Registers admin chat commands via the SML command subsystem.
 *
 * The two ban subsystems (USteamBanSubsystem, UEOSBanSubsystem) are
 * UGameInstanceSubsystem instances and are created/destroyed automatically
 * by Unreal's subsystem framework — no extra setup is needed here.
 */
class BANSYSTEM_API FBanSystemModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    /**
     * Handle returned by the PreLogin ban-check hook.
     * Fires before a player's connection is accepted; sets ErrorMessage to
     * reject the connection if the player is banned.
     */
    FDelegateHandle PreLoginHookHandle;

    /** Handle returned by the PostLogin after-hook; used to unsubscribe cleanly. */
    FDelegateHandle PostLoginHookHandle;

    /** Handle used to unsubscribe the world-init delegate for command registration. */
    FDelegateHandle WorldInitHandle;
};
