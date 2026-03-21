// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

// Compatibility stub + server-side hook module for EOSShared.
//
// On dedicated-server Alpakit builds, CSS custom UnrealEngine omits the real
// EOSShared plugin. In the Editor, the CSS Engine's EOSShared.uplugin lists
// the EOSShared module but does not ship its source, causing IntelliSense
// errors ("Plugin EOSShared does not contain the EOSShared module").
// This stub satisfies the compile-time dependency for both Server and Editor
// targets while also installing a native hook (via SML's NativeHookManager)
// that suppresses UFGLocalPlayer::RequestPublicPlayerAddress on dedicated
// servers.
//
// See Private/EOSSharedModule.cpp for the hook implementation.

#include "Modules/ModuleManager.h"

/**
 * Server/Editor compatibility module for EOSShared.
 *
 * Loaded only on WindowsServer / LinuxServer / Editor targets (TargetDenyList:
 * [Game] in CSSCompatStubs.uplugin). During StartupModule it hooks the
 * private, non-virtual UFGLocalPlayer::RequestPublicPlayerAddress method and
 * calls Scope.Cancel() to prevent the ipify.org HTTP request that the game
 * fires unconditionally for the EOS service-account local player on every
 * server start-up.
 */
class FEOSSharedCompatModule : public IModuleInterface
{
public:
	// Begin IModuleInterface
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
	// End IModuleInterface

private:
	/** Handle returned by SUBSCRIBE_METHOD; used for clean unsubscription at shutdown. */
	FDelegateHandle PublicIpHookHandle;
};
