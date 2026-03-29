// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_ecom.h — EOS SDK Ecom (Epic Games Store commerce) interface, written
// from scratch using only public EOS SDK documentation
// (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the Ecom handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_EcomHandleDetails;
typedef struct EOS_EcomHandleDetails* EOS_HEcom;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EEcomOwnershipStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EEcomOwnershipStatus
{
    EOS_OS_NotOwned = 0,
    EOS_OS_Owned    = 1
} EOS_EEcomOwnershipStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_ItemOwnership
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_ITEMOWNERSHIP_API_LATEST 1

typedef struct EOS_Ecom_ItemOwnership
{
    /** API version: must be EOS_ECOM_ITEMOWNERSHIP_API_LATEST */
    int32_t                  ApiVersion;
    /** Catalog item ID */
    const char*              Id;
    /** Ownership status for this item */
    EOS_EEcomOwnershipStatus OwnershipStatus;
} EOS_Ecom_ItemOwnership;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_QueryOwnershipOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_QUERYOWNERSHIP_API_LATEST 2

typedef struct EOS_Ecom_QueryOwnershipOptions
{
    /** API version: must be EOS_ECOM_QUERYOWNERSHIP_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Array of catalog item ID strings to check ownership of */
    const char**      CatalogItemIds;
    /** Number of entries in CatalogItemIds */
    uint32_t          CatalogItemIdCount;
    /** Optional catalog namespace override (NULL = use deployment namespace) */
    const char*       CatalogNamespace;
} EOS_Ecom_QueryOwnershipOptions;

typedef struct EOS_Ecom_QueryOwnershipCallbackInfo
{
    EOS_EResult                  ResultCode;
    void*                        ClientData;
    EOS_EpicAccountId            LocalUserId;
    /** Array of ownership results (valid only during the callback) */
    const EOS_Ecom_ItemOwnership* ItemOwnership;
    /** Number of entries in ItemOwnership */
    uint32_t                     ItemOwnershipCount;
} EOS_Ecom_QueryOwnershipCallbackInfo;

typedef void (EOS_CALL *EOS_Ecom_OnQueryOwnershipCallback)(const EOS_Ecom_QueryOwnershipCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_QueryEntitlementsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_QUERYENTITLEMENTS_API_LATEST 2

typedef struct EOS_Ecom_QueryEntitlementsOptions
{
    /** API version: must be EOS_ECOM_QUERYENTITLEMENTS_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Array of entitlement name strings to filter (NULL = all entitlements) */
    const char**      EntitlementNames;
    /** Number of entries in EntitlementNames */
    uint32_t          EntitlementNameCount;
    /** EOS_TRUE to include already-redeemed entitlements */
    EOS_Bool          bIncludeRedeemed;
} EOS_Ecom_QueryEntitlementsOptions;

typedef struct EOS_Ecom_QueryEntitlementsCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
} EOS_Ecom_QueryEntitlementsCallbackInfo;

typedef void (EOS_CALL *EOS_Ecom_OnQueryEntitlementsCallback)(const EOS_Ecom_QueryEntitlementsCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_Entitlement
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_ENTITLEMENT_API_LATEST 2

typedef struct EOS_Ecom_Entitlement
{
    /** API version: must be EOS_ECOM_ENTITLEMENT_API_LATEST */
    int32_t     ApiVersion;
    /** Name of the entitlement */
    const char* EntitlementName;
    /** Unique ID for this entitlement grant */
    const char* EntitlementId;
    /** Catalog item ID this entitlement corresponds to */
    const char* CatalogItemId;
    /** Server-assigned index (used for pagination, may be -1) */
    int32_t     ServerIndex;
    /** EOS_TRUE if this entitlement has been redeemed */
    EOS_Bool    bRedeemed;
    /** UTC Unix timestamp when the entitlement expires (-1 = no expiry) */
    int64_t     EndTimestamp;
} EOS_Ecom_Entitlement;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_GetEntitlementsCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST 1

typedef struct EOS_Ecom_GetEntitlementsCountOptions
{
    /** API version: must be EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
} EOS_Ecom_GetEntitlementsCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Ecom_CopyEntitlementByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST 1

typedef struct EOS_Ecom_CopyEntitlementByIndexOptions
{
    /** API version: must be EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Zero-based index of the entitlement to copy */
    uint32_t          EntitlementIndex;
} EOS_Ecom_CopyEntitlementByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Ecom interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries ownership status for a set of catalog items */
typedef void        (EOS_CALL *EOS_Ecom_QueryOwnership_t)(EOS_HEcom Handle, const EOS_Ecom_QueryOwnershipOptions* Options, void* ClientData, EOS_Ecom_OnQueryOwnershipCallback CompletionDelegate);

/** Queries entitlements for the local user */
typedef void        (EOS_CALL *EOS_Ecom_QueryEntitlements_t)(EOS_HEcom Handle, const EOS_Ecom_QueryEntitlementsOptions* Options, void* ClientData, EOS_Ecom_OnQueryEntitlementsCallback CompletionDelegate);

/** Returns the number of cached entitlements */
typedef uint32_t    (EOS_CALL *EOS_Ecom_GetEntitlementsCount_t)(EOS_HEcom Handle, const EOS_Ecom_GetEntitlementsCountOptions* Options);

/** Copies a cached entitlement by index; release with EOS_Ecom_Entitlement_Release */
typedef EOS_EResult (EOS_CALL *EOS_Ecom_CopyEntitlementByIndex_t)(EOS_HEcom Handle, const EOS_Ecom_CopyEntitlementByIndexOptions* Options, EOS_Ecom_Entitlement** OutEntitlement);

/** Releases memory allocated by EOS_Ecom_CopyEntitlementByIndex */
typedef void        (EOS_CALL *EOS_Ecom_Entitlement_Release_t)(EOS_Ecom_Entitlement* Entitlement);

#ifdef __cplusplus
}
#endif
