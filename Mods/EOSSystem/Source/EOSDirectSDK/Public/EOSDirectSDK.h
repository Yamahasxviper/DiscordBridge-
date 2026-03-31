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
//   2. ENGINE-OWNED EOS PLATFORM HANDLE
//      Retrieves the EOS_HPlatform that Satisfactory's engine initialised.
//      With this any EOS C SDK function can be called:
//        EOSDirectSDK::GetPlatformHandle()   →  EOS_HPlatform
//
//   3. CONNECT INTERFACE SHORTCUT
//      Direct access to EOS_HConnect for identity operations:
//        EOSDirectSDK::GetConnectInterface() →  EOS_HConnect
//
// SEPARATION FROM EOSIdHelper
// ────────────────────────────
// EOSIdHelper extracts EOS PUIDs from UE's OnlineServices abstraction layer
// (FUniqueNetIdRepl → PUID string).  EOSDirectSDK is conceptually different:
// it skips the UE layer entirely and speaks directly to the EOS C SDK using
// the engine-owned EOS_HPlatform handle.
//
// USAGE IN BANSYSTEM
// ───────────────────
//   #include "EOSDirectSDK.h"
//
//   // 1. Convert a stored PUID string to a handle (no platform needed):
//   EOS_ProductUserId PUIDHandle = EOSDirectSDK::PUIDFromString(StoredPUID);
//   if (EOSDirectSDK::IsValidHandle(PUIDHandle))
//   {
//       // 2. Get the engine's EOS_HPlatform to call any EOS C SDK function:
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
// All functions are guarded by #if WITH_EOS_SDK.  On builds where the EOSSDK
// module is absent the header is a no-op — no compilation errors.  All
// functions are inline — zero additional DLL calls.

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

// EOSShared helpers:
//   LexToString(EOS_ProductUserId)  →  FString (32-char lowercase hex)
//   IEOSSDKManager::Get()          →  singleton owning all engine EOS platforms
//   IEOSPlatformHandle             →  thin wrapper around EOS_HPlatform
#include "EOSShared.h"
#include "IEOSSDKManager.h"
// IEOSPlatformHandle is defined within IEOSSDKManager.h in the CSS UE5.3.2
// engine build.  A separate IEOSPlatformHandle.h is not distributed, so we
// do NOT include it explicitly here.

// C++17 type traits — needed for the compile-time API-detection helper below.
#include <type_traits>
#include <utility>

// ─────────────────────────────────────────────────────────────────────────────
//  Compile-time IEOSSDKManager API detection
//
//  Satisfactory migrated from UE4 → UE5, so the CSS (Coffee Stain Studios)
//  engine may carry older UE5 code alongside UE5.3.2 code.  Three API tiers
//  are known across all UE4→UE5 EOSShared variants:
//
//  TIER 1 — CSS UE5.3.2 (CSS-specific, absent from vanilla UE5):
//      GetPlatformHandles()  →  TArray<TSharedRef<IEOSPlatformHandle>>
//
//  TIER 2 — Vanilla UE 5.2 / UE5.3 (standard, absent from CSS):
//      GetActivePlatforms()  →  TArray<IEOSPlatformHandlePtr>  (TSharedPtr)
//
//  TIER 3 — Early UE5 / UE4-era hybrid (no enumeration method at all):
//      CreatePlatform(GetDefaultPlatformConfigName())  →  IEOSPlatformHandlePtr
//      CreatePlatform() is idempotent — it returns the existing handle when a
//      platform for that config name was already created, so calling it here
//      is safe and has no side-effects on a running game.
//
//  GetPlatformHandle() below uses C++17 if-constexpr + these traits to pick
//  the correct tier.  The discarded branches are never compiled, so none of
//  them trigger C2039 on an engine that lacks that method.
// ─────────────────────────────────────────────────────────────────────────────
namespace EOSSDKManagerDetail
{
    // Tier 1: CSS UE5.3.2 — GetPlatformHandles() exists.
    template <class T, class = void>
    struct THasGetPlatformHandles : std::false_type {};
    template <class T>
    struct THasGetPlatformHandles<T,
        std::void_t<decltype(std::declval<T&>().GetPlatformHandles())>>
        : std::true_type {};

    // Tier 2: Vanilla UE5.2+ — GetActivePlatforms() exists.
    template <class T, class = void>
    struct THasGetActivePlatforms : std::false_type {};
    template <class T>
    struct THasGetActivePlatforms<T,
        std::void_t<decltype(std::declval<T&>().GetActivePlatforms())>>
        : std::true_type {};

    // Tier 3: Early UE5 / UE4-era hybrid — CreatePlatform(FString) exists.
    // (CreatePlatform is present in every known UE5 and CSS variant as the
    //  lowest-common-denominator platform-creation API.)
    template <class T, class = void>
    struct THasCreatePlatformByName : std::false_type {};
    template <class T>
    struct THasCreatePlatformByName<T,
        std::void_t<decltype(std::declval<T&>().CreatePlatform(std::declval<const FString&>()))>>
        : std::true_type {};
} // namespace EOSSDKManagerDetail

// ─────────────────────────────────────────────────────────────────────────────
//  EOSDirectSDK namespace
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSDirectSDK
 *
 * Dedicated namespace providing direct EOS C SDK access for BanSystem and
 * other Satisfactory server mods.
 *
 * All functions are inline.  Thread safety follows the EOS C SDK's own rules:
 * the stateless PUID helpers are safe on any thread; handle-based calls
 * (GetPlatformHandle, GetConnectInterface) must be made on the game thread
 * or under the same thread model that the engine uses for EOS operations.
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
 * Uses LexToString() from EOSShared — the canonical UE5 way to render an
 * EOS_ProductUserId as a string.
 *
 * @param PUID  Handle to stringify.
 * @return 32-char lowercase hex string, or empty FString when PUID is invalid.
 */
inline FString PUIDToString(EOS_ProductUserId PUID)
{
    if (!EOS_ProductUserId_IsValid(PUID))
        return FString();

    return LexToString(PUID);
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
//  2. Engine EOS Platform Handle
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HPlatform handle that Satisfactory's engine initialised.
 *
 * The engine creates exactly one EOS platform instance at startup via the
 * EOSShared plugin.  IEOSSDKManager is the engine-internal registry that owns
 * this instance.  With the returned handle ANY EOS C SDK function that takes
 * an EOS_HPlatform can be called:
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
 * @return EOS_HPlatform handle owned by the engine.  Returns nullptr when:
 *           • EOSShared / IEOSSDKManager is not loaded, or
 *           • The EOS platform has not been initialised yet (call too early in
 *             the startup sequence — typically safe from PostDefault onwards).
 *
 * NOTE ON ENGINE-VARIANT API (three tiers — do NOT hardcode a single call)
 * ─────────────────────────────────────────────────────────────────────────
 * Satisfactory migrated UE4 → UE5, so the CSS engine may carry code from
 * multiple EOSShared generations.  Three tiers are detected at compile time
 * via if-constexpr (see EOSSDKManagerDetail above):
 *
 *   Tier 1  CSS UE5.3.2:   GetPlatformHandles()  → TArray<TSharedRef<…>>
 *   Tier 2  Vanilla UE5.2+: GetActivePlatforms() → TArray<IEOSPlatformHandlePtr>
 *   Tier 3  Early UE5/UE4: CreatePlatform(name)  → IEOSPlatformHandlePtr
 *
 * Only the matching tier is compiled; discarded branches never cause C2039.
 */
inline EOS_HPlatform GetPlatformHandle()
{
    IEOSSDKManager* Manager = IEOSSDKManager::Get();
    if (!Manager)
        return nullptr;

    // Compile-time dispatch across three known IEOSSDKManager API generations.
    // Only the branch that exists in this engine's IEOSSDKManager.h compiles.
    if constexpr (EOSSDKManagerDetail::THasGetPlatformHandles<IEOSSDKManager>::value)
    {
        // Tier 1 — CSS UE5.3.2: GetPlatformHandles() → TArray<TSharedRef<IEOSPlatformHandle>>
        // TSharedRef is never null; IsEmpty() guard alone is sufficient.
        const auto Handles = Manager->GetPlatformHandles();
        if (Handles.IsEmpty())
            return nullptr;
        return static_cast<EOS_HPlatform>(*Handles[0]);
    }
    else if constexpr (EOSSDKManagerDetail::THasGetActivePlatforms<IEOSSDKManager>::value)
    {
        // Tier 2 — Vanilla UE5.2+: GetActivePlatforms() → TArray<IEOSPlatformHandlePtr>
        // IEOSPlatformHandlePtr is TSharedPtr — validate before dereferencing.
        const auto Handles = Manager->GetActivePlatforms();
        if (Handles.IsEmpty() || !Handles[0].IsValid())
            return nullptr;
        return static_cast<EOS_HPlatform>(*Handles[0]);
    }
    else if constexpr (EOSSDKManagerDetail::THasCreatePlatformByName<IEOSSDKManager>::value)
    {
        // Tier 3 — Early UE5 / UE4-era hybrid: no list method available.
        // CreatePlatform() is idempotent — returns the existing handle when
        // the platform for that config name was already created by the engine.
        const auto Handle = Manager->CreatePlatform(Manager->GetDefaultPlatformConfigName());
        if (!Handle.IsValid())
            return nullptr;
        return static_cast<EOS_HPlatform>(*Handle);
    }
    else
    {
        // No known IEOSSDKManager enumeration API found.  Return nullptr so
        // callers degrade gracefully rather than crashing.
        return nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  3. EOS Interface shortcuts
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the EOS_HConnect interface for the engine's active EOS platform.
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
