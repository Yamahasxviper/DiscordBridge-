// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSIdHelper.h  (module: EOSIdHelper, plugin: EOSSystem)
//
// Provides EOSId::GetProductUserId — a helper that extracts a player's EOS
// Product User ID (PUID) from their FUniqueNetIdRepl for use in Satisfactory
// server mods.
//
// TWO SUPPORTED PATHS
// ───────────────────
// V2 — FAccountId (OnlineServicesEOSGS)
//   The path Satisfactory ships.  Always compiled.
//
// V1 — FUniqueNetId of EOS type (OnlineSubsystemEOS / IUniqueNetIdEOS)
//   The legacy UE4-era path.  Compiled only when WITH_EOS_SUBSYSTEM_V1=1.
//   OnlineSubsystemEOS is currently "Enabled": false in FactoryGame.uproject,
//   which means its .lib is never produced and referencing it causes:
//       LNK1181: cannot open input file 'UnrealEditor-OnlineSubsystemEOS.lib'
//   To re-enable: set OnlineSubsystemEOS enabled in FactoryGame.uproject, then
//   set WITH_EOS_SUBSYSTEM_V1=1 in EOSIdHelper.Build.cs.
//
// IMPLEMENTATION NOTE
// ────────────────────
// GetProductUserId is a non-inline exported function (EOSIDHELPER_API).
// The implementation lives in EOSIdHelper.cpp, which is the only translation
// unit that includes OnlineIdEOSGS.h and EOSShared.h.  This prevents the
// DLL-import symbols for UE::Online::GetProductUserId, EOS_ProductUserId_IsValid,
// and LexToString from being pulled into every caller's .obj file and causing
// unresolved-external linker errors (LNK2019) in consumer modules (e.g. BanSystem).

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"   // FUniqueNetIdRepl

// ─────────────────────────────────────────────────────────────────────────────
//  EOSId namespace
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSId
 *
 * Helpers for extracting the EOS Product User ID (PUID) from a Satisfactory
 * player's network identity.
 *
 * Satisfactory authenticates all players (Epic-direct and Steam players with a
 * linked Epic account) through the UE5 OnlineServicesEOSGS stack.  Every such
 * player has a 32-character lower-hex PUID, e.g.:
 *
 *   "00020aed06f0a6958c3c067fb4b73d51"
 *
 * Both the V2 (current) and V1 (legacy) paths are implemented in EOSIdHelper.cpp.
 * The V1 path is guarded by WITH_EOS_SUBSYSTEM_V1 (see EOSIdHelper.Build.cs).
 *
 * Usage in any server-side mod:
 *
 *   FString PUID;
 *   if (EOSId::GetProductUserId(PlayerState->GetUniqueId(), PUID))
 *   {
 *       // PUID is the 32-char hex EOS Product User ID
 *   }
 */
namespace EOSId
{

/**
 * Attempt to extract the EOS Product User ID from a player's UniqueNetId.
 *
 * Tries the V2 path (OnlineServicesEOSGS) first; falls through to the V1
 * path (OnlineSubsystemEOS / IUniqueNetIdEOS) when WITH_EOS_SUBSYSTEM_V1=1.
 *
 * @param UniqueId          The player's replicated network identity.
 * @param OutProductUserId  Receives the 32-char lowercase hex PUID on success.
 * @return true if a valid PUID was extracted; false otherwise.
 */
EOSIDHELPER_API bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId);

} // namespace EOSId

