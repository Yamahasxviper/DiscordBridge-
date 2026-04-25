// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Engine/World.h"

/** Module entry point for the DiscordBridge plugin. */
class FDiscordBridgeModule : public IModuleInterface
{
public:
    // Begin IModuleInterface
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
    // End IModuleInterface

private:
    /** Handle for the FWorldDelegates::OnWorldInitializedActors registration.
     *  Used to register /verify, /discord, /ingamewhitelist SML commands. */
    FDelegateHandle WorldInitHandle;
};
