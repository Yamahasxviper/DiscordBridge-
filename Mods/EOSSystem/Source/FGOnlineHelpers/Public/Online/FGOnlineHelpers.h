// Copyright Yamahasxviper. All Rights Reserved.
//
// FGOnlineHelpers.h  (module: FGOnlineHelpers, plugin: EOSSystem)
//
// Provides header stubs for CSS FactoryGame/EOS types that are not present
// in Satisfactory's custom UE build headers.  Listed as a dependency by all
// Alpakit C++ mods so that UBT can resolve engine headers at mod compile time.
//
// Also provides EOSId::GetProductUserId — the authoritative EOS Product User
// ID extraction helper for Satisfactory server mods.
//
// IMPLEMENTATION NOTE
// ────────────────────
// GetProductUserId is a non-inline exported function (FGONLINEHELPERS_API).
// The implementation lives in FGOnlineHelpers.cpp, which is the only
// translation unit that includes OnlineIdEOSGS.h and EOSShared.h.
// This prevents the DLL-import symbols for UE::Online::GetProductUserId,
// EOS_ProductUserId_IsValid, and LexToString from being pulled into consumer
// .obj files and causing LNK2019 errors (e.g. in BanSystem).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"  // FUniqueNetIdRepl

// ─────────────────────────────────────────────────────────────────────────────
//  EOSId  —  CSS FactoryGame helper namespace for EOS identity extraction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSId helpers
 *
 * Extract an EOS Product User ID (PUID) from a player's FUniqueNetIdRepl,
 * regardless of whether it wraps a V1 FUniqueNetId or a V2 FAccountId.
 *
 * Supported scenarios in CSS Satisfactory:
 *
 *   V2 — FAccountId  (OnlineServicesEOSGS)
 *       Players whose primary authentication goes through EOS/Epic Online
 *       Services hold a V2 FAccountId.  UE::Online::GetProductUserId()
 *       extracts the EOS_ProductUserId from the registry.
 *
 *   V1 — FUniqueNetId of type "EOS"  (OnlineSubsystemEOS)
 *       Legacy path.  Compiled only when WITH_EOS_SUBSYSTEM_V1=1.
 *       The id is cast to IUniqueNetIdEOS to retrieve the ProductUserId.
 *
 *   V1 — FUniqueNetId of any other type (e.g. "Steam", "Null")
 *       No EOS PUID is embedded — GetProductUserId returns false.
 */
namespace EOSId
{

/**
 * Attempt to extract an EOS Product User ID from a FUniqueNetIdRepl.
 *
 * Handles both V1 (FUniqueNetId) and V2 (FAccountId) representations.
 * Safe to call for every player regardless of how they connected.
 *
 * @param UniqueId          The player's replicated network identity.
 * @param OutProductUserId  Receives the 32-char lowercase hex PUID on success.
 * @return true if a valid PUID was extracted; false otherwise.
 */
FGONLINEHELPERS_API bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId);

}  // namespace EOSId
