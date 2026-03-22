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
 *   - Installs a hook on AFGGameMode::PostLogin to enforce bans
 *     the moment a player joins.
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
    /** Handle returned by the PostLogin after-hook; used to unsubscribe cleanly. */
    FDelegateHandle PostLoginHookHandle;

    /** Handle used to unsubscribe the world-init delegate for command registration. */
    FDelegateHandle WorldInitHandle;
};
