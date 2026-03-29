// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"  // FUniqueNetIdRepl, IsV1/IsV2, GetV1Unsafe, GetV2Unsafe

#if WITH_EOS_SDK

// EOS SDK — EOS_ProductUserId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

// OnlineServicesEOSGS — free function UE::Online::GetProductUserId(FAccountId)
// for the V2 (FAccountId / UE::Online::Online Services) path.
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) → FString
#include "EOSShared.h"

// NOTE: OnlineSubsystemEOS (IUniqueNetIdEOS) is intentionally omitted.
// Satisfactory uses the UE5 V2 OnlineServicesEOSGS path exclusively.
// OnlineSubsystemEOS is disabled ("Enabled": false) in FactoryGame.uproject,
// so including OnlineSubsystemEOSTypesPublic.h would produce LNK1181.

#endif // WITH_EOS_SDK

// ─────────────────────────────────────────────────────────────────────────────
//  EOSId  —  CSS FactoryGame helper namespace for EOS identity extraction
// ─────────────────────────────────────────────────────────────────────────────

/**
 * EOSId helpers
 *
 * Thin wrappers that extract an EOS Product User ID (PUID) from a
 * FUniqueNetIdRepl, regardless of whether it wraps a legacy V1 FUniqueNetId
 * or a modern V2 FAccountId from the OnlineServices layer.
 *
 * Supported scenarios in CSS Satisfactory:
 *
 *   V2 — FAccountId  (OnlineServicesEOSGS)
 *       Players whose primary authentication goes through EOS/Epic Online
 *       Services hold a V2 FAccountId.  UE::Online::GetProductUserId()
 *       extracts the EOS_ProductUserId from the registry.
 *       Satisfactory uses this path exclusively.
 *
 *   V1 — FUniqueNetId of any type (e.g. "Steam", "Null", legacy "EOS")
 *       Not used in Satisfactory.  OnlineSubsystemEOS is disabled in
 *       FactoryGame.uproject, so no IUniqueNetIdEOS cast is attempted.
 */
namespace EOSId
{

/**
 * Attempt to extract an EOS Product User ID from a FUniqueNetIdRepl.
 *
 * Handles both V1 (FUniqueNetId) and V2 (FAccountId) representations.
 * Safe to call for every player regardless of how they connected.
 *
 * @param UniqueId        The player's replicated network identity.
 * @param OutProductUserId  Receives the 32-char lowercase hex PUID on success.
 * @return true if a valid PUID was extracted; false otherwise.
 */
inline bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId)
{
    OutProductUserId.Empty();

    if (!UniqueId.IsValid())
        return false;

#if WITH_EOS_SDK

    if (UniqueId.IsV2())
    {
        // ── V2 path: FAccountId from OnlineServicesEOS/EOSGS ────────────────
        // UE::Online::GetProductUserId resolves the FAccountId handle through
        // the registry and returns the underlying EOS_ProductUserId.
        const UE::Online::FAccountId& AccountId = UniqueId.GetV2Unsafe();
        if (!AccountId.IsValid())
            return false;

        const EOS_ProductUserId ProductUserId = UE::Online::GetProductUserId(AccountId);
        if (!EOS_ProductUserId_IsValid(ProductUserId))
            return false;

        OutProductUserId = LexToString(ProductUserId);
        return !OutProductUserId.IsEmpty();
    }

    // V1 FUniqueNetId paths (Steam, Null, legacy EOS) do not carry an EOS PUID
    // accessible without OnlineSubsystemEOS.  Satisfactory does not use the V1
    // EOS path, so no further extraction is attempted.

#endif  // WITH_EOS_SDK

    return false;
}

}  // namespace EOSId
