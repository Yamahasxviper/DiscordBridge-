// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSBanSDK.h  (module: EOSIdHelper, plugin: DummyHeaders)
//
// Custom EOS SDK access layer for the BanSystem mod.
//
// Provides three categories of helpers:
//
//   1. STATELESS PUID HELPERS
//      Convert 32-char hex PUID strings to/from EOS_ProductUserId handles.
//      These call EOS_ProductUserId_FromString / EOS_ProductUserId_IsValid /
//      LexToString directly — no EOS platform handle required.
//
//   2. EOS PLATFORM HANDLE ACCESS
//      EOSBanSDK::GetPlatformHandle() returns the EOS_HPlatform handle that
//      Satisfactory's engine initialised at startup.  With this handle any
//      EOS C SDK function (e.g. EOS_Connect_*, EOS_UserInfo_*, EOS_Sanctions_*)
//      can be called directly.
//
//   3. CONNECT INTERFACE HELPER
//      EOSBanSDK::GetConnectInterface() returns the EOS_HConnect handle for
//      the active platform — the entry point for EOS identity operations.
//
// USAGE PATTERN IN BANSYSTEM
// ──────────────────────────
//   #include "EOSBanSDK.h"
//
//   // Convert a stored PUID string back to a handle:
//   EOS_ProductUserId PUIDHandle = EOSBanSDK::PUIDFromString(StoredPUID);
//   if (EOSBanSDK::IsValidHandle(PUIDHandle))
//   {
//       // Use PUIDHandle with any EOS C SDK call that needs EOS_ProductUserId:
//       EOS_HPlatform Platform = EOSBanSDK::GetPlatformHandle();
//       if (Platform)
//       {
//           EOS_HConnect ConnectHandle = EOS_Platform_GetConnectInterface(Platform);
//           // ... call EOS_Connect_* functions ...
//       }
//   }
//
// IMPORTANT
// ─────────
// Only compiled when WITH_EOS_SDK is defined (i.e. when the EOSSDK module
// is available).  All functions are inline — zero extra DLL overhead.

#pragma once

#include "CoreMinimal.h"

#if WITH_EOS_SDK

// ── EOS SDK headers ────────────────────────────────────────────────────────
// Include the platform-specific base header first if required by the EOS SDK
// packaging (some CSS engine builds require this before eos_common.h).
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#   include EOS_PLATFORM_BASE_FILE_NAME
#endif

#include "eos_common.h"   // EOS_ProductUserId, EOS_HPlatform, EOS_Bool, EOS_TRUE/FALSE
#include "eos_platform.h" // EOS_Platform_GetConnectInterface, EOS_Platform_GetUserInfoInterface

// EOSShared — provides LexToString(EOS_ProductUserId) → FString
#include "EOSShared.h"

// IEOSSDKManager — singleton manager that owns the EOS_HPlatform created by
// Satisfactory's engine.  Provided by the EOSShared plugin (already a
// dependency of this module via EOSIdHelper.Build.cs).
#include "IEOSSDKManager.h"
// IEOSPlatformHandle — wraps EOS_HPlatform, exposes GetHandle().
// Included by IEOSSDKManager.h in most EOSShared versions; included here
// explicitly to guarantee availability regardless of engine variant.
#include "IEOSPlatformHandle.h"

// ─────────────────────────────────────────────────────────────────────────────
//  EOSBanSDK namespace
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSBanSDK
 *
 * Custom EOS SDK wrapper designed for use with the BanSystem mod.
 *
 * All functions are inline and safe to call from any thread, subject to the
 * EOS SDK's own threading requirements for handle-based calls.
 *
 * PUID STRING FORMAT
 * ──────────────────
 * EOS Product User IDs are stored in BanSystem as 32-character lowercase
 * hexadecimal strings (e.g. "00020aed06f0a6958c3c067fb4b73d51").
 * PUIDFromString() / PUIDToString() convert between this form and the opaque
 * EOS_ProductUserId handle used by the EOS C SDK.
 */
namespace EOSBanSDK
{

// ─────────────────────────────────────────────────────────────────────────────
//  1. Stateless PUID helpers
//     These do NOT require the EOS platform to be initialised.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Convert a stored EOS Product User ID string to an EOS_ProductUserId handle.
 *
 * Wraps EOS_ProductUserId_FromString() which is a pure parse function — it
 * requires no EOS platform handle and works even before the EOS platform is
 * fully initialised.
 *
 * The returned handle should be validated with IsValidHandle() before use.
 * An empty or malformed string produces a null/invalid handle.
 *
 * @param PUIDStr  32-char lowercase hex PUID stored in BanSystem
 *                 (e.g. "00020aed06f0a6958c3c067fb4b73d51").
 * @return EOS_ProductUserId handle; may be invalid if the string is malformed.
 */
inline EOS_ProductUserId PUIDFromString(const FString& PUIDStr)
{
    if (PUIDStr.IsEmpty())
        return nullptr;

    // EOS_ProductUserId_FromString parses the 32-char hex string and returns
    // an opaque EOS_ProductUserId handle.  The function is stateless — it
    // does not call back into the EOS platform and has no side effects.
    return EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUIDStr.ToLower()));
}

/**
 * Convert an EOS_ProductUserId handle back to its 32-char hex string form.
 *
 * Uses LexToString() from EOSShared, which is the same method used throughout
 * the DummyHeaders EOSIdHelper module.
 *
 * @param PUID  EOS_ProductUserId handle to convert.
 * @return 32-char lowercase hex string, or an empty FString if PUID is invalid.
 */
inline FString PUIDToString(EOS_ProductUserId PUID)
{
    if (!EOS_ProductUserId_IsValid(PUID))
        return FString();

    return LexToString(PUID);
}

/**
 * Returns true if the supplied EOS_ProductUserId handle represents a valid
 * EOS Product User ID.
 *
 * Equivalent to (EOS_ProductUserId_IsValid(PUID) == EOS_TRUE).
 * Provided as a named boolean helper to match the BanSystem coding style.
 *
 * @param PUID  Handle to validate.
 */
inline bool IsValidHandle(EOS_ProductUserId PUID)
{
    return EOS_ProductUserId_IsValid(PUID) == EOS_TRUE;
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. EOS Platform Handle access
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HPlatform handle that Satisfactory's engine initialised.
 *
 * The handle is retrieved through IEOSSDKManager — the singleton manager in
 * the EOSShared plugin that owns all EOS platform instances created by the
 * engine.  On a dedicated Satisfactory server exactly one platform instance
 * is active at runtime.
 *
 * With this handle, any EOS C SDK API can be called:
 *
 *   EOS_HPlatform Platform = EOSBanSDK::GetPlatformHandle();
 *   if (Platform)
 *   {
 *       // Example: get the Connect interface
 *       EOS_HConnect Connect = EOS_Platform_GetConnectInterface(Platform);
 *       // Example: get the UserInfo interface
 *       EOS_HUserInfo UserInfo = EOS_Platform_GetUserInfoInterface(Platform);
 *   }
 *
 * @return EOS_HPlatform handle, or nullptr if:
 *           - IEOSSDKManager is not available (EOSShared not loaded), or
 *           - No EOS platform has been initialised yet (too early in startup).
 */
inline EOS_HPlatform GetPlatformHandle()
{
    IEOSSDKManager* Manager = IEOSSDKManager::Get();
    if (!Manager)
        return nullptr;

    // IEOSSDKManager tracks every EOS platform handle created by the engine.
    // On a Satisfactory dedicated server there is exactly one active platform.
    //
    // NOTE: The method to retrieve active platform handles from IEOSSDKManager
    // varies between UE5 EOSShared versions.  The implementation below uses
    // GetPlatformHandles() (UE5.3+).  If your CSS engine build exposes a
    // different method name (e.g. GetAllPlatforms(), GetManagedHandles()), replace
    // GetPlatformHandles() below.  Refer to:
    //   Engine/Plugins/Online/EOSShared/Source/EOSShared/Public/IEOSSDKManager.h
    TArray<TSharedRef<IEOSPlatformHandle>> Handles = Manager->GetPlatformHandles();
    if (Handles.IsEmpty())
        return nullptr;

    // IEOSPlatformHandle wraps the raw EOS_HPlatform.  Most EOSShared versions
    // expose it via a GetHandle() method; some expose an implicit conversion
    // operator.  Use GetHandle() as the explicit form.
    return Handles[0]->GetHandle();
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. Connect Interface helper
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HConnect interface for the active EOS platform.
 *
 * EOS_HConnect is the entry point for EOS identity operations including:
 *   • EOS_Connect_QueryExternalAccountMappings — map external IDs to PUIDs
 *   • EOS_Connect_GetProductUserIdMapping     — look up PUID from external ID
 *   • EOS_Connect_CreateDeviceId               — create a device ID credential
 *
 * Returns nullptr when GetPlatformHandle() returns nullptr (see above).
 */
inline EOS_HConnect GetConnectInterface()
{
    const EOS_HPlatform Platform = GetPlatformHandle();
    if (!Platform)
        return nullptr;

    return EOS_Platform_GetConnectInterface(Platform);
}

} // namespace EOSBanSDK

#endif // WITH_EOS_SDK
