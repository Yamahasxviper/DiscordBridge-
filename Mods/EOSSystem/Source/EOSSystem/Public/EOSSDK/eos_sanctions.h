// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sanctions.h — EOS SDK Sanctions interface, written from scratch using
// only public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sanctions_PlayerSanction
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SANCTIONS_PLAYERSANCTION_API_LATEST 1

typedef struct EOS_Sanctions_PlayerSanction
{
    /** API version: must be EOS_SANCTIONS_PLAYERSANCTION_API_LATEST */
    int32_t     ApiVersion;
    /** ISO 8601 UTC timestamp string when the sanction was placed */
    const char* TimePlaced;
    /** Action string describing the sanction type (e.g., "SUSPENSION") */
    const char* Action;
    /** Unique reference ID for this sanction */
    const char* ReferenceId;
} EOS_Sanctions_PlayerSanction;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sanctions_QueryActivePlayerSanctionsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SANCTIONS_QUERYACTIVEPLAYERSANCTIONS_API_LATEST 2

typedef struct EOS_Sanctions_QueryActivePlayerSanctionsOptions
{
    /** API version: must be EOS_SANCTIONS_QUERYACTIVEPLAYERSANCTIONS_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the player to query sanctions for */
    EOS_ProductUserId TargetUserId;
    /** Product User ID of the local user making the request */
    EOS_ProductUserId LocalUserId;
} EOS_Sanctions_QueryActivePlayerSanctionsOptions;

typedef struct EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId TargetUserId;
    EOS_ProductUserId LocalUserId;
} EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo;

typedef void (EOS_CALL *EOS_Sanctions_OnQueryActivePlayerSanctionsCallback)(const EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sanctions_GetPlayerSanctionCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SANCTIONS_GETPLAYERSANCTIONCOUNT_API_LATEST 1

typedef struct EOS_Sanctions_GetPlayerSanctionCountOptions
{
    /** API version: must be EOS_SANCTIONS_GETPLAYERSANCTIONCOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the player */
    EOS_ProductUserId TargetUserId;
} EOS_Sanctions_GetPlayerSanctionCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sanctions_CopyPlayerSanctionByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SANCTIONS_COPYPLAYERSANCTIONBYINDEX_API_LATEST 1

typedef struct EOS_Sanctions_CopyPlayerSanctionByIndexOptions
{
    /** API version: must be EOS_SANCTIONS_COPYPLAYERSANCTIONBYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the player */
    EOS_ProductUserId TargetUserId;
    /** Zero-based index of the sanction to copy */
    uint32_t          SanctionIndex;
} EOS_Sanctions_CopyPlayerSanctionByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Sanctions interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries active sanctions for a given player */
typedef void        (EOS_CALL *EOS_Sanctions_QueryActivePlayerSanctions_t)(EOS_HSanctions Handle, const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options, void* ClientData, EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate);

/** Returns the number of cached sanctions for a player */
typedef uint32_t    (EOS_CALL *EOS_Sanctions_GetPlayerSanctionCount_t)(EOS_HSanctions Handle, const EOS_Sanctions_GetPlayerSanctionCountOptions* Options);

/** Copies a sanction entry by index; caller must release with EOS_Sanctions_PlayerSanction_Release */
typedef EOS_EResult (EOS_CALL *EOS_Sanctions_CopyPlayerSanctionByIndex_t)(EOS_HSanctions Handle, const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options, EOS_Sanctions_PlayerSanction** OutSanction);

/** Releases memory allocated by EOS_Sanctions_CopyPlayerSanctionByIndex */
typedef void        (EOS_CALL *EOS_Sanctions_PlayerSanction_Release_t)(EOS_Sanctions_PlayerSanction* Sanction);

#ifdef __cplusplus
}
#endif
