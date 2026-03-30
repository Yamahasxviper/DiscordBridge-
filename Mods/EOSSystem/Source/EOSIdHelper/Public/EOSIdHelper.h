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

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"   // FUniqueNetIdRepl, IsV1/IsV2, GetV1/V2Unsafe

#if WITH_EOS_SDK

// EOS SDK — EOS_ProductUserId, EOS_EpicAccountId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

// UE5 V2 — UE::Online::GetProductUserId(FAccountId) -> EOS_ProductUserId
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) -> FString
#include "EOSShared.h"

// V1 — IUniqueNetIdEOS interface for the legacy OnlineSubsystemEOS path.
// Only compiled when WITH_EOS_SUBSYSTEM_V1=1 (controlled in EOSIdHelper.Build.cs).
// OnlineSubsystemEOS is disabled in FactoryGame.uproject; keeping this guarded
// lets the V1 code exist and compile without producing LNK1181.
#if WITH_EOS_SUBSYSTEM_V1
#include "OnlineSubsystemEOSTypesPublic.h"
#endif

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
 * Both the V2 (current) and V1 (legacy) paths are implemented below.
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
inline bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId)
{
    OutProductUserId.Empty();

    if (!UniqueId.IsValid())
        return false;

#if WITH_EOS_SDK

    // ── V2 path: FAccountId (OnlineServicesEOSGS) ────────────────────────────
    // Every Satisfactory player with an active EOS session holds a V2 FAccountId.
    if (UniqueId.IsV2())
    {
        const UE::Online::FAccountId& AccountId = UniqueId.GetV2Unsafe();
        if (!AccountId.IsValid())
            return false;

        // Resolve FAccountId -> EOS_ProductUserId via the EOSGS registry.
        const EOS_ProductUserId ProductUserId = UE::Online::GetProductUserId(AccountId);
        if (!EOS_ProductUserId_IsValid(ProductUserId))
            return false;

        OutProductUserId = LexToString(ProductUserId);
        return !OutProductUserId.IsEmpty();
    }

#if WITH_EOS_SUBSYSTEM_V1
    // ── V1 path: FUniqueNetId of EOS type (OnlineSubsystemEOS) ──────────────
    // Only compiled when OnlineSubsystemEOS is enabled and WITH_EOS_SUBSYSTEM_V1=1.
    // Guards against static_cast to IUniqueNetIdEOS on non-EOS IDs (Steam, Null).
    if (UniqueId.IsV1())
    {
        static const FName EosTypeName(TEXT("EOS"));
        const FUniqueNetIdPtr& Ptr = UniqueId.GetV1Unsafe();
        if (!Ptr.IsValid())
            return false;

        if (Ptr->GetType() != EosTypeName)
            return false;  // Not a V1 EOS id (e.g. Steam, Null) — no PUID

        // IUniqueNetIdEOS is the shared interface for all V1 EOS net-ids.
        // The static_cast is safe here because GetType() == "EOS" confirms the type.
        const IUniqueNetIdEOS* EosId = static_cast<const IUniqueNetIdEOS*>(Ptr.Get());
        if (!EosId)
            return false;

        const EOS_ProductUserId ProductUserId = EosId->GetProductUserId();
        if (!EOS_ProductUserId_IsValid(ProductUserId))
            return false;

        OutProductUserId = LexToString(ProductUserId);
        return !OutProductUserId.IsEmpty();
    }
#endif // WITH_EOS_SUBSYSTEM_V1

#endif // WITH_EOS_SDK

    return false;
}

} // namespace EOSId

