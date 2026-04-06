// Copyright Yamahasxviper. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * FServerWhitelistModule
 *
 * Minimal Unreal Engine module entry point for the ServerWhitelist Alpakit mod.
 * All runtime logic lives in UServerWhitelistSubsystem (a UGameInstanceSubsystem
 * that is created automatically by the GameInstance when the server starts).
 */
class FServerWhitelistModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FServerWhitelistModule, ServerWhitelist)
