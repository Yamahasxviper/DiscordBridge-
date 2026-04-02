// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSBanSDK.h  (module: EOSIdHelper, plugin: EOSSystem)
//
// COMPATIBILITY FORWARDER
// ────────────────────────
// Direct EOS C SDK access has been moved to its own dedicated module:
//   EOSDirectSDK  (Mods/EOSSystem/Source/EOSDirectSDK/)
//
// This header is kept so that any existing code using EOSBanSDK.h / the
// EOSBanSDK:: namespace continues to compile without modification.
// It includes EOSDirectSDK.h and creates a namespace alias so that
// EOSBanSDK:: and EOSDirectSDK:: are interchangeable.
//
// NEW CODE SHOULD:
//   1. Add "EOSDirectSDK" to PublicDependencyModuleNames in the mod's Build.cs
//   2. #include "EOSDirectSDK.h" directly
//   3. Use EOSDirectSDK:: namespace helpers
//
// EXISTING CODE (BanIdResolver, BanDiscordSubsystem, BanCommands) continues
// to work unchanged via the EOSBanSDK namespace alias defined below.

#pragma once

// Include the authoritative dedicated module header.
// All helper implementations live there.
#include "EOSDirectSDK.h"

#if WITH_EOS_SDK

// ─────────────────────────────────────────────────────────────────────────────
//  EOSBanSDK  —  backward-compatible alias for EOSDirectSDK
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSBanSDK is a namespace alias for EOSDirectSDK.
 *
 * Code written against the original EOSBanSDK:: namespace continues to
 * compile and behave identically.  No migration is required for existing
 * callers (BanIdResolver, BanDiscordSubsystem, BanCommands).
 *
 * Example — both of these are equivalent:
 *   EOS_ProductUserId H = EOSBanSDK::PUIDFromString(Str);
 *   EOS_ProductUserId H = EOSDirectSDK::PUIDFromString(Str);
 */
namespace EOSBanSDK = EOSDirectSDK;

#endif // WITH_EOS_SDK

