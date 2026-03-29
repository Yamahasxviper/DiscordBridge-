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
 *   - Registers admin chat commands via the SML command subsystem.
 *
 * Ban enforcement (PreLogin rejection and PostLogin kick with async EOS PUID
 * resolution and cross-platform ban propagation) is handled entirely by
 * UBanEnforcementSubsystem — a UGameInstanceSubsystem that initialises
 * automatically when the game instance starts.
 *
 * The ban storage subsystems (USteamBanSubsystem, UEOSBanSubsystem) are
 * UGameInstanceSubsystem instances and are created/destroyed automatically
 * by the Unreal subsystem framework — no extra setup is needed here.
 */
class BANSYSTEM_API FBanSystemModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    /** Handle used to unsubscribe the world-init delegate for command registration. */
    FDelegateHandle WorldInitHandle;
};
