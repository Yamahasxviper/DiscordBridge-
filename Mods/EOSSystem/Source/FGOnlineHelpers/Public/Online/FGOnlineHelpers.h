// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"  // FUniqueNetIdRepl, IsV1/IsV2, GetV1Unsafe, GetV2Unsafe

#if WITH_EOS_SDK

// EOS SDK — EOS_ProductUserId, EOS_EpicAccountId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

// OnlineServicesEOSGS — free function UE::Online::GetProductUserId(FAccountId)
// for the V2 (FAccountId / UE::Online::Online Services) path.
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) → FString
#include "EOSShared.h"

// V1 — OnlineSubsystemEOS — IUniqueNetIdEOS interface for the legacy EOS path.
// Guarded by WITH_EOS_SUBSYSTEM_V1 because OnlineSubsystemEOS is currently
// "Enabled": false in FactoryGame.uproject.  Including it without the module
// dep produces LNK1181.  Flip WITH_EOS_SUBSYSTEM_V1 to 1 in
// FGOnlineHelpers.Build.cs to re-enable when the plugin becomes available.
#if WITH_EOS_SUBSYSTEM_V1
#include "OnlineSubsystemEOSTypesPublic.h"
#endif

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

#if WITH_EOS_SUBSYSTEM_V1
    if (UniqueId.IsV1())
    {
        // ── V1 path: FUniqueNetId of EOS type (OnlineSubsystemEOS) ──────────
        // Only attempt the cast when the type name confirms this is an EOS ID;
        // a Steam or Null FUniqueNetId must NOT be cast to IUniqueNetIdEOS.
        static const FName EosTypeName(TEXT("EOS"));
        const FUniqueNetIdPtr& Ptr = UniqueId.GetV1Unsafe();
        if (!Ptr.IsValid())
            return false;

        if (Ptr->GetType() != EosTypeName)
            return false;  // Not an EOS V1 id (e.g. Steam, Null) — no PUID

        // IUniqueNetIdEOS is the shared interface for all EOS V1 net-ids.
        // The static_cast is valid here because we already confirmed the type.
        const IUniqueNetIdEOS* EosId = static_cast<const IUniqueNetIdEOS*>(Ptr.Get());
        if (!EosId)
            return false;

        const EOS_ProductUserId ProductUserId = EosId->GetProductUserId();
        if (!EOS_ProductUserId_IsValid(ProductUserId))
            return false;

        OutProductUserId = LexToString(ProductUserId);
        return !OutProductUserId.IsEmpty();
    }
#endif  // WITH_EOS_SUBSYSTEM_V1

#endif  // WITH_EOS_SDK

    return false;
}

}  // namespace EOSId
