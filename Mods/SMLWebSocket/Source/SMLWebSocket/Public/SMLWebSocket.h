// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"
#include "Logging/LogMacros.h"

/** Dedicated log category for SMLWebSocket – used in place of LogTemp so that
 *  all WebSocket messages can be captured by the DiscordBot file logger. */
DECLARE_LOG_CATEGORY_EXTERN(LogSMLWebSocket, Log, All);

/** Custom WebSocket module providing SSL/OpenSSL-backed WebSocket client support for Satisfactory mods. */
class FSMLWebSocketModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};
