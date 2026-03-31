// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSDirectSDK.h  (module: EOSDirectSDK, plugin: EOSSystem)
//
// ─────────────────────────────────────────────────────────────────────────────
// DEDICATED DIRECT EOS C SDK ACCESS MODULE FOR BANSYSTEM
// ─────────────────────────────────────────────────────────────────────────────
//
// This header is the sole public surface of the EOSDirectSDK module.  It gives
// any Satisfactory server mod that lists "EOSDirectSDK" in its Build.cs the
// ability to:
//
//   1. PUID STRING  ←→  EOS_ProductUserId HANDLE CONVERSION
//      Stateless helpers — no EOS platform required.
//        EOSDirectSDK::PUIDFromString(StoredPUID)
//        EOSDirectSDK::PUIDToString(Handle)
//        EOSDirectSDK::IsValidHandle(Handle)
//
//   2. EOS PLATFORM HANDLE
//      Returns the EOS_HPlatform created and managed by UEOSSystemSubsystem.
//      With this handle any EOS C SDK function can be called:
//        EOSDirectSDK::GetPlatformHandle()   →  EOS_HPlatform
//
//   3. CONNECT INTERFACE SHORTCUT
//      Direct access to EOS_HConnect for identity operations:
//        EOSDirectSDK::GetConnectInterface() →  EOS_HConnect
//
//   4. PLATFORM REGISTRATION (called by UEOSSystemSubsystem only)
//      The platform handle is registered once after EOS_Platform_Create and
//      cleared before EOS_Platform_Release.  Callers other than
//      UEOSSystemSubsystem must NOT call these:
//        EOSDirectSDK::RegisterPlatformHandle(Handle)
//        EOSDirectSDK::UnregisterPlatformHandle()
//
// ENGINE-AGNOSTIC DESIGN
// ───────────────────────
// Earlier revisions attempted to retrieve the engine's EOS platform via the
// UE EOSShared plugin (IEOSSDKManager::Get()).  That API is absent or
// structurally different in many CSS UE5.3.2 builds, causing recurring C2039
// build errors regardless of which SFINAE tier was used.
//
// This revision takes a fully engine-agnostic approach:
//   • UEOSSystemSubsystem creates its own EOS_HPlatform via FEOSSDKLoader
//     (dynamic DLL loader, no dependency on any CSS engine EOS abstractions).
//   • After creation it calls EOSDirectSDK::RegisterPlatformHandle(handle) to
//     store the handle in a module-static variable defined in EOSDirectSDK.cpp.
//   • GetPlatformHandle() reads that static — zero dependency on IEOSSDKManager,
//     IEOSPlatformHandle, EOSShared, or any other engine-variant type.
//
// SEPARATION FROM EOSIdHelper
// ────────────────────────────
// EOSIdHelper extracts EOS PUIDs from UE's OnlineServices abstraction layer
// (FUniqueNetIdRepl → PUID string).  EOSDirectSDK is conceptually different:
// it skips the UE layer entirely and speaks directly to the EOS C SDK using
// the registered EOS_HPlatform handle.
//
// USAGE IN BANSYSTEM
// ───────────────────
//   #include "EOSDirectSDK.h"
//
//   // 1. Convert a stored PUID string to a handle (no platform needed):
//   EOS_ProductUserId PUIDHandle = EOSDirectSDK::PUIDFromString(StoredPUID);
//   if (EOSDirectSDK::IsValidHandle(PUIDHandle))
//   {
//       // 2. Get the registered EOS_HPlatform to call any EOS C SDK function:
//       EOS_HPlatform Platform = EOSDirectSDK::GetPlatformHandle();
//       if (Platform)
//       {
//           // 3. Example: use the Connect interface
//           EOS_HConnect Connect = EOS_Platform_GetConnectInterface(Platform);
//           // ... call EOS_Connect_* functions ...
//
//           // 4. Example: use the UserInfo interface
//           EOS_HUserInfo UserInfo = EOS_Platform_GetUserInfoInterface(Platform);
//           // ... call EOS_UserInfo_* functions ...
//
//           // 5. Example: use the Sanctions interface
//           EOS_HSanctions Sanctions = EOS_Platform_GetSanctionsInterface(Platform);
//           // ... call EOS_Sanctions_* functions ...
//       }
//   }
//
// AVAILABILITY
// ─────────────
// PUID conversion helpers and platform accessors are guarded by #if WITH_EOS_SDK.
// On builds where the EOSSDK module is absent the header is a no-op — no
// compilation errors.  PUID helpers are inline; platform functions are
// non-inline (defined in EOSDirectSDK.cpp) to keep the static registry
// isolated from consumer translation units.

#pragma once

#include "CoreMinimal.h"

#if WITH_EOS_SDK

// ── EOS SDK headers ────────────────────────────────────────────────────────
// Some CSS engine packaging requires the platform-specific base header before
// eos_common.h.  The EOS_PLATFORM_BASE_FILE_NAME macro handles this.
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#   include EOS_PLATFORM_BASE_FILE_NAME
#endif

// The CSS UE5.3.2 engine ships its own eos_base.h (at
// Engine/Source/ThirdParty/EOSSDK/SDK/Include/eos_base.h) which requires
// EOS_MEMORY_CALL, EOS_CALL, and EOS_USE_DLLEXPORT to be pre-defined.
// When EOS_PLATFORM_BASE_FILE_NAME is not set (common in server/editor
// builds) these macros are missing, producing C1189.  Define them here as
// a safe fallback — each definition is guarded so the engine's own
// eos_platform_prereqs.h wins if it was already included.
#ifndef EOS_CALL
#  if defined(_WIN32)
#    define EOS_CALL __cdecl
#  else
#    define EOS_CALL
#  endif
#endif
#ifndef EOS_MEMORY_CALL
#  define EOS_MEMORY_CALL EOS_CALL
#endif
#ifndef EOS_USE_DLLEXPORT
#  define EOS_USE_DLLEXPORT 0
#endif

// Core EOS C SDK types: EOS_ProductUserId, EOS_HPlatform, EOS_Bool,
// EOS_TRUE, EOS_FALSE, EOS_ProductUserId_FromString,
// EOS_ProductUserId_IsValid, EOS_ProductUserId_ToString
#include "eos_common.h"

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EResult value constants — fallback macros
//
//  The CSS UE5.3.2 engine may ship eos_common.h with EOS_EResult defined as a
//  C++ scoped enum class (enum class EOS_EResult : int32_t).  Scoped enum
//  values are NOT injected into the enclosing namespace, so bare names such as
//  EOS_Success are undeclared identifiers (C2065) unless provided as macros.
//
//  Each constant is guarded by #ifndef so the real SDK definition wins when
//  present (e.g. on builds where EOS_EResult is a plain C enum and the values
//  are already exposed as macro aliases).  Explicit C-style casts ensure these
//  macros work whether EOS_EResult is a typedef-int, C enum, or C++ scoped
//  enum class.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_Success
#  define EOS_Success                  ((EOS_EResult)0)
#  define EOS_NoConnection             ((EOS_EResult)1)
#  define EOS_InvalidCredentials       ((EOS_EResult)2)
#  define EOS_InvalidUser              ((EOS_EResult)3)
#  define EOS_InvalidAuth              ((EOS_EResult)4)
#  define EOS_AccessDenied             ((EOS_EResult)5)
#  define EOS_MissingPermissions       ((EOS_EResult)6)
#  define EOS_TokenNotAccount          ((EOS_EResult)7)
#  define EOS_TooManyRequests          ((EOS_EResult)8)
#  define EOS_AlreadyPending           ((EOS_EResult)9)
#  define EOS_InvalidParameters        ((EOS_EResult)10)
#  define EOS_InvalidRequest           ((EOS_EResult)11)
#  define EOS_UnrecognizedResponse     ((EOS_EResult)12)
#  define EOS_IncompatibleVersion      ((EOS_EResult)13)
#  define EOS_NotConfigured            ((EOS_EResult)14)
#  define EOS_AlreadyConfigured        ((EOS_EResult)15)
#  define EOS_NotImplemented           ((EOS_EResult)16)
#  define EOS_Canceled                 ((EOS_EResult)17)
#  define EOS_NotFound                 ((EOS_EResult)18)
#  define EOS_OperationWillRetry       ((EOS_EResult)19)
#  define EOS_NoChange                 ((EOS_EResult)20)
#  define EOS_VersionMismatch          ((EOS_EResult)21)
#  define EOS_LimitExceeded            ((EOS_EResult)22)
#  define EOS_Duplicate                ((EOS_EResult)23)
#  define EOS_MissingParameters        ((EOS_EResult)24)
#  define EOS_InvalidSandboxId         ((EOS_EResult)25)
#  define EOS_TimedOut                 ((EOS_EResult)26)
#  define EOS_PartialResult            ((EOS_EResult)27)
#  define EOS_Missing_Role             ((EOS_EResult)28)
#  define EOS_Missing_Feature          ((EOS_EResult)29)
#  define EOS_Invalid_Sandbox          ((EOS_EResult)30)
#  define EOS_Invalid_Deployment       ((EOS_EResult)31)
#  define EOS_Invalid_Product          ((EOS_EResult)32)
#  define EOS_Invalid_ProductUserID    ((EOS_EResult)33)
#  define EOS_ServiceFailure           ((EOS_EResult)34)
#  define EOS_CacheDirectoryMissing    ((EOS_EResult)35)
#  define EOS_CacheDirectoryInvalid    ((EOS_EResult)36)
#  define EOS_InvalidState             ((EOS_EResult)37)
#  define EOS_RequestInProgress        ((EOS_EResult)38)
#  define EOS_ApplicationSuspended     ((EOS_EResult)39)
#  define EOS_NetworkChanged           ((EOS_EResult)40)
#endif // EOS_Success

// EOS Platform interface accessors:
//   EOS_Platform_GetConnectInterface(platform)    →  EOS_HConnect
//   EOS_Platform_GetUserInfoInterface(platform)   →  EOS_HUserInfo
//   EOS_Platform_GetSanctionsInterface(platform)  →  EOS_HSanctions
//   (and all other EOS_Platform_Get*Interface functions)
//
// eos_platform.h lives alongside this header in EOSDirectSDK/Public/.  That
// stub provides a complete, ABI-compatible replacement for the real EOS SDK
// eos_platform.h, which is absent from some CSS engine EOSSDK distributions.
// If a future CSS engine build does ship eos_platform.h inside the EOSSDK
// plugin, the include guard in our stub (EOS_Platform_H — the same guard the
// real SDK uses) prevents any redeclaration conflicts.
#include "eos_platform.h"

// NOTE: EOSShared.h and IEOSSDKManager.h are intentionally NOT included here.
// The CSS UE5.3.2 engine ships varying versions of IEOSSDKManager — some
// builds omit the header entirely, others have different method signatures.
// Depending on either header caused recurring C2039 build failures.
// GetPlatformHandle() now uses a module-static registry (EOSDirectSDK.cpp)
// populated by UEOSSystemSubsystem, which is fully engine-agnostic.

// ─────────────────────────────────────────────────────────────────────────────
//  EOSDirectSDK namespace
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSDirectSDK
 *
 * Dedicated namespace providing direct EOS C SDK access for BanSystem and
 * other Satisfactory server mods.
 *
 * PUID conversion helpers (PUIDFromString, PUIDToString, IsValidHandle) are
 * inline — they call EOS C SDK functions directly and require no platform.
 *
 * Platform functions (GetPlatformHandle, RegisterPlatformHandle,
 * UnregisterPlatformHandle, GetConnectInterface) are non-inline; they are
 * defined in EOSDirectSDK.cpp and exported from the EOSDirectSDK DLL.
 *
 * Thread safety: PUID helpers are safe on any thread.  Platform functions
 * must be called on the game thread.
 *
 * PUID STRING FORMAT
 * ──────────────────
 * BanSystem stores EOS Product User IDs as 32-character lowercase hex strings,
 * e.g. "00020aed06f0a6958c3c067fb4b73d51".  PUIDFromString() / PUIDToString()
 * convert between this storage form and the opaque EOS_ProductUserId handle
 * required by every EOS C SDK function.
 */
namespace EOSDirectSDK
{

// ─────────────────────────────────────────────────────────────────────────────
//  1. Stateless PUID helpers — do NOT require the EOS platform to be running
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Convert a stored 32-char hex PUID string to an EOS_ProductUserId handle.
 *
 * Calls EOS_ProductUserId_FromString() — a pure parse function that needs no
 * EOS_HPlatform and has no side effects.  The returned handle should be
 * validated with IsValidHandle() before passing to any EOS C SDK function.
 *
 * @param PUIDStr  32-char lowercase hex string (e.g. "00020aed06f0a6958c3c067fb4b73d51").
 * @return Parsed EOS_ProductUserId handle; invalid (null) if PUIDStr is empty
 *         or malformed.
 */
inline EOS_ProductUserId PUIDFromString(const FString& PUIDStr)
{
    if (PUIDStr.IsEmpty())
        return nullptr;

    return EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUIDStr.ToLower()));
}

/**
 * Convert an EOS_ProductUserId handle to its 32-char lowercase hex string.
 *
 * Uses EOS_ProductUserId_ToString() directly — no EOSShared dependency.
 *
 * @param PUID  Handle to stringify.
 * @return 32-char lowercase hex string, or empty FString when PUID is invalid.
 */
inline FString PUIDToString(EOS_ProductUserId PUID)
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

/**
 * Returns true when the EOS C SDK considers the handle to be a valid,
 * well-formed EOS Product User ID.
 *
 * Named boolean wrapper around EOS_ProductUserId_IsValid() == EOS_TRUE.
 *
 * @param PUID  Handle to validate.
 */
inline bool IsValidHandle(EOS_ProductUserId PUID)
{
    return EOS_ProductUserId_IsValid(PUID) == EOS_TRUE;
}

// ─────────────────────────────────────────────────────────────────────────────
//  2. EOS Platform Handle
//
//  The handle is registered by UEOSSystemSubsystem after it calls
//  EOS_Platform_Create() via FEOSSDKLoader.  This approach is fully
//  engine-agnostic — it requires no IEOSSDKManager, IEOSPlatformHandle, or
//  any other CSS engine-variant header.
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HPlatform handle registered by UEOSSystemSubsystem.
 *
 * The handle is valid once UEOSSystemSubsystem::Initialize() has completed
 * successfully.  It becomes null again after UEOSSystemSubsystem::Deinitialize().
 *
 * With the returned handle ANY EOS C SDK function that takes an EOS_HPlatform
 * can be called:
 *
 *   EOS_HPlatform Platform = EOSDirectSDK::GetPlatformHandle();
 *   if (Platform)
 *   {
 *       EOS_HConnect   Connect   = EOS_Platform_GetConnectInterface(Platform);
 *       EOS_HUserInfo  UserInfo  = EOS_Platform_GetUserInfoInterface(Platform);
 *       EOS_HSanctions Sanctions = EOS_Platform_GetSanctionsInterface(Platform);
 *       // ... call any EOS_<Interface>_* function
 *   }
 *
 * @return EOS_HPlatform handle owned by UEOSSystemSubsystem.  Returns nullptr
 *         when UEOSSystemSubsystem has not yet created the platform or has
 *         already released it.
 */
EOSDIRECTSDK_API EOS_HPlatform GetPlatformHandle();

/**
 * Register the EOS_HPlatform handle created by UEOSSystemSubsystem.
 *
 * ONLY UEOSSystemSubsystem::Initialize() should call this.  Must be called
 * immediately after EOS_Platform_Create() succeeds.
 *
 * @param Platform  The handle returned by EOS_Platform_Create().
 */
EOSDIRECTSDK_API void RegisterPlatformHandle(EOS_HPlatform Platform);

/**
 * Clear the registered EOS_HPlatform handle.
 *
 * ONLY UEOSSystemSubsystem::Deinitialize() should call this.  Must be called
 * before EOS_Platform_Release().
 */
EOSDIRECTSDK_API void UnregisterPlatformHandle();

// ─────────────────────────────────────────────────────────────────────────────
//  3. EOS Interface shortcuts
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HConnect interface for the registered EOS platform.
 *
 * EOS_HConnect is the entry point for EOS identity operations:
 *   • EOS_Connect_QueryExternalAccountMappings  — map external IDs to PUIDs
 *   • EOS_Connect_GetProductUserIdMapping       — reverse-map PUID → external ID
 *   • EOS_Connect_CreateDeviceId               — create a device credential
 *   • EOS_Connect_Login / EOS_Connect_LinkAccount
 *
 * Returns nullptr when GetPlatformHandle() returns nullptr.
 */
inline EOS_HConnect GetConnectInterface()
{
    const EOS_HPlatform Platform = GetPlatformHandle();
    if (!Platform)
        return nullptr;

    return EOS_Platform_GetConnectInterface(Platform);
}

} // namespace EOSDirectSDK

#endif // WITH_EOS_SDK
