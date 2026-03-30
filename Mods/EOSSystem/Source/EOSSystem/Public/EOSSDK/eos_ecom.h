// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_ecom.h — delegates to the real EOS SDK eos_ecom.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_ecom.h via the EOSSDK module include path.
#include <eos_ecom.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Ecom interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_ecom.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Ecom_QueryOwnership_t)(EOS_HEcom Handle, const EOS_Ecom_QueryOwnershipOptions* Options, void* ClientData, EOS_Ecom_OnQueryOwnershipCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Ecom_QueryEntitlements_t)(EOS_HEcom Handle, const EOS_Ecom_QueryEntitlementsOptions* Options, void* ClientData, EOS_Ecom_OnQueryEntitlementsCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Ecom_GetEntitlementsCount_t)(EOS_HEcom Handle, const EOS_Ecom_GetEntitlementsCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Ecom_CopyEntitlementByIndex_t)(EOS_HEcom Handle, const EOS_Ecom_CopyEntitlementByIndexOptions* Options, EOS_Ecom_Entitlement** OutEntitlement);
typedef void        (EOS_CALL *EOS_Ecom_Entitlement_Release_t)(EOS_Ecom_Entitlement* Entitlement);
