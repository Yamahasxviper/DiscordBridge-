// Copyright Yamahasxviper. All Rights Reserved.
//
// FGOnlineHelpers.h  (module: FGOnlineHelpers, plugin: EOSSystem)
//
// Provides header stubs for CSS FactoryGame/EOS types that are not present
// in Satisfactory's custom UE build headers.  Listed as a dependency by all
// Alpakit C++ mods so that UBT can resolve engine headers at mod compile time.
//
// Also provides the authoritative player-identity helpers for Satisfactory
// server mods:
//
//   EOSId::GetProductUserId   — extract EOS Product User ID (PUID)
//   EOSId::GetSteam64Id       — extract Steam 64-bit ID
//   EOSId::IsValidSteam64Id   — validate a Steam64 ID string
//   EOSId::IsValidEOSProductUserId — validate an EOS PUID string
//
// IMPLEMENTATION NOTE
// ────────────────────
// GetProductUserId and GetSteam64Id are non-inline exported functions
// (FGONLINEHELPERS_API).  Their implementations live in FGOnlineHelpers.cpp,
// which is the only translation unit that includes OnlineIdEOSGS.h and
// EOSShared.h.  This prevents the DLL-import symbols for
// UE::Online::GetProductUserId, EOS_ProductUserId_IsValid, and LexToString
// from being pulled into consumer .obj files and causing LNK2019 errors
// (e.g. in BanSystem).
//
// IsValidSteam64Id and IsValidEOSProductUserId are pure string validators
// with no platform or engine dependencies — they are inline so callers get
// zero-overhead validation without a DLL call.

#pragma once

#include "CoreMinimal.h"

// Forward-declare FUniqueNetIdRepl so consumers of this header can compile
// without requiring GameFramework/OnlineReplStructs.h on their include path.
// The CSS Alpakit server distribution does not ship OnlineReplStructs.h as
// part of the standalone Engine module headers; it is available through
// FactoryGame's include paths.  Callers that create or inspect
// FUniqueNetIdRepl values must include GameFramework/OnlineReplStructs.h
// themselves (all current consumers already get it via FactoryGame or Engine).
struct FUniqueNetIdRepl;

// ─────────────────────────────────────────────────────────────────────────────
//  EOSId  —  CSS FactoryGame helper namespace for EOS identity extraction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSId helpers
 *
 * Centralised player-identity utilities for Satisfactory server mods.
 * Every Alpakit C++ mod in this project lists FGOnlineHelpers as a
 * dependency, so all helpers below are available without additional
 * module dependencies.
 *
 * Supported identity scenarios in CSS Satisfactory:
 *
 *   EOS / Epic Online Services
 *   ──────────────────────────
 *   V2 — FAccountId  (OnlineServicesEOSGS)
 *       Players whose primary authentication goes through EOS hold a V2
 *       FAccountId.  UE::Online::GetProductUserId() extracts the
 *       EOS_ProductUserId from the registry.
 *
 *   V1 — FUniqueNetId of type "EOS"  (OnlineSubsystemEOS)
 *       Legacy path.  Compiled only when WITH_EOS_SUBSYSTEM_V1=1.
 *       The id is cast to IUniqueNetIdEOS to retrieve the ProductUserId.
 *
 *   Steam
 *   ─────
 *   V1 — FUniqueNetId of type "Steam"
 *       When a player authenticates through Steam, FUniqueNetIdRepl::GetType()
 *       returns "Steam" and ToString() returns the 17-digit Steam64 decimal
 *       string (e.g. "76561198000000000").
 *
 *   V1 — FUniqueNetId of any other type (e.g. "Null")
 *       No platform identity is embedded — both Get* helpers return false.
 */
namespace EOSId
{

// ─────────────────────────────────────────────────────────────────────────────
//  EOS Product User ID helpers
// ─────────────────────────────────────────────────────────────────────────────

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

/**
 * Returns true when Id looks like a valid EOS Product User ID:
 * exactly 32 lowercase hexadecimal characters, no hyphens or spaces.
 *
 * Example valid PUID: "00020aed06f0a6958c3c067fb4b73d51"
 *
 * This is a pure string validation — no EOS platform or engine subsystem
 * is required.  Call this before persisting or comparing PUID strings.
 *
 * @param Id  String to validate.
 * @return true if Id is a 32-char lowercase hex string.
 */
inline bool IsValidEOSProductUserId(const FString& Id)
{
    if (Id.Len() != 32) return false;
    const FString Lower = Id.ToLower();
    for (TCHAR C : Lower)
    {
        const bool bDigit = (C >= TEXT('0') && C <= TEXT('9'));
        const bool bHex   = (C >= TEXT('a') && C <= TEXT('f'));
        if (!bDigit && !bHex) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Steam 64-bit ID helpers
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Attempt to extract a Steam 64-bit ID from a FUniqueNetIdRepl.
 *
 * Returns true when the identity was created by the UE5 Steam online
 * services plugin (GetType() == "Steam") and ToString() yields a valid
 * 17-digit Steam64 decimal string.
 *
 * Returns false for EOS, Null, and any other identity type.
 *
 * @param UniqueId      The player's replicated network identity.
 * @param OutSteam64Id  Receives the 17-digit Steam64 decimal string on success.
 * @return true if a valid Steam64 ID was extracted; false otherwise.
 */
FGONLINEHELPERS_API bool GetSteam64Id(const FUniqueNetIdRepl& UniqueId, FString& OutSteam64Id);

/**
 * Returns true when Id looks like a valid Steam 64-bit ID:
 * exactly 17 decimal digits, starting with "7656119".
 *
 * Example valid Steam64 ID: "76561198000000000"
 *
 * This is a pure string validation — no platform or engine subsystem is
 * required.  Call this before persisting or comparing Steam64 ID strings.
 *
 * @param Id  String to validate.
 * @return true if Id is a 17-digit decimal string starting with "7656119".
 */
inline bool IsValidSteam64Id(const FString& Id)
{
    if (Id.Len() != 17) return false;
    for (TCHAR C : Id)
        if (C < TEXT('0') || C > TEXT('9')) return false;
    // Steam64 IDs live in the range [76561193005069312, 76561202255233023].
    // A quick prefix check covers the overwhelming majority of real IDs.
    return Id.StartsWith(TEXT("7656119"));
}

}  // namespace EOSId
