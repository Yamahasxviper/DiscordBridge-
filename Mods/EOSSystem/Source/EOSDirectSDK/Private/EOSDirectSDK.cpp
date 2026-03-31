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

#if WITH_EOS_SDK

EOS_ProductUserId PUIDFromString(const FString& PUIDStr)
{
    if (PUIDStr.IsEmpty())
        return nullptr;

    return EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUIDStr.ToLower()));
}

FString PUIDToString(EOS_ProductUserId PUID)
{
    if (EOS_ProductUserId_IsValid(PUID) != EOS_TRUE)
        return FString();

    // EOS_PRODUCTUSERID_MAX_LENGTH is 32 hex chars; 64 is a safe buffer.
    char Buf[64] = {};
    int32_t Len  = static_cast<int32_t>(sizeof(Buf));
    if (EOS_ProductUserId_ToString(PUID, Buf, &Len) == EOS_Success)
        return UTF8_TO_TCHAR(Buf);

    return FString();
}

bool IsValidHandle(EOS_ProductUserId PUID)
{
    return EOS_ProductUserId_IsValid(PUID) == EOS_TRUE;
}

#endif // WITH_EOS_SDK

} // namespace EOSDirectSDK
