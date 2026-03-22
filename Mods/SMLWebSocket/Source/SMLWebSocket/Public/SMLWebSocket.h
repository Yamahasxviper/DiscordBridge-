// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

/** Custom WebSocket module providing SSL/OpenSSL-backed WebSocket client support for Satisfactory mods. */
class FSMLWebSocketModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface
};
