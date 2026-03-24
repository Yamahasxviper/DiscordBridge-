// Copyright Yamahasxviper. All Rights Reserved.

#include "Modules/ModuleManager.h"

/**
 * FTicketSystemModule
 *
 * Minimal Unreal Engine module entry point for the TicketSystem Alpakit mod.
 * All runtime logic lives in UTicketSubsystem (a UGameInstanceSubsystem that
 * is created automatically by the GameInstance when the server starts).
 */
class FTicketSystemModule : public IModuleInterface
{
public:
	virtual void StartupModule() override {}
	virtual void ShutdownModule() override {}
};

IMPLEMENT_MODULE(FTicketSystemModule, TicketSystem)
