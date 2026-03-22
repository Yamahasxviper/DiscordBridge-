// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "Logging/LogMacros.h"
#include "Modules/ModuleManager.h"

DECLARE_LOG_CATEGORY_EXTERN(LogDiscordBridge, Log, All);

/** Module entry point for the DiscordBridge plugin. */
class FDiscordBridgeModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};
