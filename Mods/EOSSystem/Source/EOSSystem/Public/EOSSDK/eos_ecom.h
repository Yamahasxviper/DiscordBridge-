// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_ecom.h — delegates to the real EOS SDK eos_ecom.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_ecom.h>)
#  include <eos_ecom.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Ecom interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Ecom_QueryOwnership_t)(EOS_HEcom Handle, const EOS_Ecom_QueryOwnershipOptions* Options, void* ClientData, EOS_Ecom_OnQueryOwnershipCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Ecom_QueryEntitlements_t)(EOS_HEcom Handle, const EOS_Ecom_QueryEntitlementsOptions* Options, void* ClientData, EOS_Ecom_OnQueryEntitlementsCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Ecom_GetEntitlementsCount_t)(EOS_HEcom Handle, const EOS_Ecom_GetEntitlementsCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Ecom_CopyEntitlementByIndex_t)(EOS_HEcom Handle, const EOS_Ecom_CopyEntitlementByIndexOptions* Options, EOS_Ecom_Entitlement** OutEntitlement);
typedef void        (EOS_CALL *EOS_Ecom_Entitlement_Release_t)(EOS_Ecom_Entitlement* Entitlement);
