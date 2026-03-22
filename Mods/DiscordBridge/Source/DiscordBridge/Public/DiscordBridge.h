// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** Module entry point for the DiscordBridge plugin. */
class FDiscordBridgeModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};
