// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSDirectSDK.cpp
//
// Implements the module stub and the non-inline functions from EOSDirectSDK.h.
//
// The platform-handle registry here is the engine-agnostic alternative to
// IEOSSDKManager, which is absent from some CSS UE5.3.2 builds.
// UEOSSystemSubsystem calls RegisterPlatformHandle() after EOS_Platform_Create
// and UnregisterPlatformHandle() before EOS_Platform_Release.

#include "EOSDirectSDK.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, EOSDirectSDK)

// ─────────────────────────────────────────────────────────────────────────────
//  Module-static EOS platform handle registry
//
//  Written once by UEOSSystemSubsystem::Initialize() and cleared by
//  UEOSSystemSubsystem::Deinitialize().  Thread safety mirrors the EOS C SDK
//  contract: platform creation/destruction must occur on the game thread.
// ─────────────────────────────────────────────────────────────────────────────
static EOS_HPlatform GEOSDirectSDKPlatform = nullptr;

namespace EOSDirectSDK
{

EOS_HPlatform GetPlatformHandle()
{
    return GEOSDirectSDKPlatform;
}

void RegisterPlatformHandle(EOS_HPlatform Platform)
{
    GEOSDirectSDKPlatform = Platform;
}

void UnregisterPlatformHandle()
{
    GEOSDirectSDKPlatform = nullptr;
}

} // namespace EOSDirectSDK
