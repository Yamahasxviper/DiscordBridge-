// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSIdHelper.cpp
//
// Implements EOSId::GetProductUserId — exported from the EOSIdHelper DLL.
//
// All EOS-specific headers (OnlineIdEOSGS.h, EOSShared.h, eos_common.h) are
// confined to this translation unit.  This prevents the DLL-import symbols for
// UE::Online::GetProductUserId, EOS_ProductUserId_IsValid, and LexToString from
// being pulled into consumer .obj files, which would cause LNK2019 errors in
// modules such as BanSystem that depend on EOSIdHelper but should not need to
// directly link against OnlineServicesEOSGS, EOSShared, or EOSSDK.

#include "EOSIdHelper.h"
#include "Modules/ModuleManager.h"

#if WITH_EOS_SDK

// EOS SDK types: EOS_ProductUserId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
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
#include "eos_common.h"

// UE5 V2 — UE::Online::GetProductUserId(FAccountId) -> EOS_ProductUserId
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) -> FString
#include "EOSShared.h"

// V1 — IUniqueNetIdEOS interface (legacy OnlineSubsystemEOS path).
// Only compiled when WITH_EOS_SUBSYSTEM_V1=1 (controlled in EOSIdHelper.Build.cs).
#if WITH_EOS_SUBSYSTEM_V1
#include "OnlineSubsystemEOSTypesPublic.h"
#endif

#endif // WITH_EOS_SDK

// FUniqueNetIdRepl — needed for IsV1/IsV2/GetV1Unsafe/GetV2Unsafe
#include "GameFramework/OnlineReplStructs.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, EOSIdHelper)

namespace EOSId
{

bool GetProductUserId(const FUniqueNetIdRepl& UniqueId, FString& OutProductUserId)
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
