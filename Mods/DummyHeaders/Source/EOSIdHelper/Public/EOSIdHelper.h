// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSIdHelper.h  (module: EOSIdHelper, plugin: DummyHeaders)
//
// Provides EOSId::GetProductUserId — a helper that extracts a player's EOS
// Product User ID (PUID) from their FUniqueNetIdRepl for use in Satisfactory
// server mods.
//
// WHY THIS EXISTS
// ───────────────
// Satisfactory uses the UE5 "OnlineServicesEOSGS" stack: player identities are
// V2 FAccountId objects.  The older UE4-era "OnlineSubsystemEOS" plugin is
// explicitly disabled ("Enabled": false) in FactoryGame.uproject, so any module
// that lists it as a build dependency fails at link time with:
//
//   LNK1181: cannot open input file 'UnrealEditor-OnlineSubsystemEOS.lib'
//   LNK2019: unresolved external symbol __imp_EOS_ProductUserId_IsValid
//
// This header uses ONLY the V2 OnlineServicesEOSGS path that Satisfactory ships,
// so those linker errors cannot occur.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"   // FUniqueNetIdRepl, IsV2, GetV2Unsafe

#if WITH_EOS_SDK

// EOS SDK — EOS_ProductUserId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

// UE5 V2 helper — UE::Online::GetProductUserId(FAccountId) -> EOS_ProductUserId
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) -> FString
#include "EOSShared.h"

// NOTE: "OnlineSubsystemEOSTypesPublic.h" (IUniqueNetIdEOS) is intentionally
// NOT included here.  OnlineSubsystemEOS is disabled in FactoryGame.uproject;
// including it would reproduce the LNK1181 / LNK2019 linker errors.

#endif // WITH_EOS_SDK

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
 * Usage in any server-side mod:
 *
 *   FString PUID;
 *   if (EOSId::GetProductUserId(PlayerState->GetUniqueId(), PUID))
 *   {
 *       // PUID is the 32-char hex EOS Product User ID — use it for ban checks etc.
 *   }
 */
namespace EOSId
{

/**
 * Attempt to extract the EOS Product User ID from a player's UniqueNetId.
 *
 * Works for every player that authenticated through EOS (Epic-direct, or Steam
 * with a linked Epic account).  Returns false for offline/LAN players (Null
 * online service) and for Steam-only players with no linked Epic account.
 *
 * @param UniqueId          The player's replicated network identity.
 * @param OutProductUserId  Receives the 32-char lowercase hex PUID on success.
 * @return true if a valid PUID was extracted; false otherwise.
 */
inline bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId)
{
    OutProductUserId.Empty();

    if (!UniqueId.IsValid())
        return false;

#if WITH_EOS_SDK

    // Satisfactory uses the UE5 V2 online stack (OnlineServicesEOSGS).
    // Every player with an active EOS session holds a V2 FAccountId.
    if (UniqueId.IsV2())
    {
        const UE::Online::FAccountId& AccountId = UniqueId.GetV2Unsafe();
        if (!AccountId.IsValid())
            return false;

        // Resolve FAccountId → EOS_ProductUserId via the EOSGS registry.
        const EOS_ProductUserId ProductUserId = UE::Online::GetProductUserId(AccountId);
        if (!EOS_ProductUserId_IsValid(ProductUserId))
            return false;

        OutProductUserId = LexToString(ProductUserId);
        return !OutProductUserId.IsEmpty();
    }

    // V1 FUniqueNetId identities (Steam, Null, legacy EOS) do not carry an EOS
    // PUID accessible without the disabled OnlineSubsystemEOS plugin.
    // Satisfactory does not use V1 EOS identities — return false gracefully.

#endif // WITH_EOS_SDK

    return false;
}

} // namespace EOSId
