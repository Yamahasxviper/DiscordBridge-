// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_leaderboards.h — EOS SDK Leaderboards interface, written from scratch
// using only public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the Leaderboards handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_LeaderboardsHandleDetails;
typedef struct EOS_LeaderboardsHandleDetails* EOS_HLeaderboards;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELeaderboardAggregation
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELeaderboardAggregation
{
    EOS_LA_Min    = 0,
    EOS_LA_Max    = 1,
    EOS_LA_Sum    = 2,
    EOS_LA_Latest = 3
} EOS_ELeaderboardAggregation;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_Definition
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_DEFINITION_API_LATEST 1

typedef struct EOS_Leaderboards_Definition
{
    /** API version: must be EOS_LEADERBOARDS_DEFINITION_API_LATEST */
    int32_t                    ApiVersion;
    /** Unique leaderboard ID */
    const char*                LeaderboardId;
    /** Name of the stat this leaderboard is based on */
    const char*                StatName;
    /** Aggregation mode */
    EOS_ELeaderboardAggregation Aggregation;
    /** UTC Unix start time for the leaderboard window (-1 = no restriction) */
    int64_t                    StartTime;
    /** UTC Unix end time for the leaderboard window (-1 = no restriction) */
    int64_t                    EndTime;
} EOS_Leaderboards_Definition;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_LeaderboardRecord
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_LEADERBOARDRECORD_API_LATEST 2

typedef struct EOS_Leaderboards_LeaderboardRecord
{
    /** API version: must be EOS_LEADERBOARDS_LEADERBOARDRECORD_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the player for this record */
    EOS_ProductUserId UserId;
    /** Rank of this record in the leaderboard (1-based) */
    uint32_t          Rank;
    /** Score value */
    int32_t           Score;
    /** Display name of the player (may be NULL) */
    const char*       UserDisplayName;
} EOS_Leaderboards_LeaderboardRecord;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_QueryLeaderboardDefinitionsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_QUERYLEADERBOARDDEFINITIONS_API_LATEST 2

typedef struct EOS_Leaderboards_QueryLeaderboardDefinitionsOptions
{
    /** API version: must be EOS_LEADERBOARDS_QUERYLEADERBOARDDEFINITIONS_API_LATEST */
    int32_t                   ApiVersion;
    /** UTC Unix start time filter (-1 = no restriction) */
    int64_t                   StartTime;
    /** UTC Unix end time filter (-1 = no restriction) */
    int64_t                   EndTime;
    /** Optional local user ID for entitlement-gated leaderboards (NULL = none) */
    const EOS_ProductUserId*  LocalUserId;
} EOS_Leaderboards_QueryLeaderboardDefinitionsOptions;

typedef struct EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
} EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallback)(const EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_QueryLeaderboardRanksOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST 2

typedef struct EOS_Leaderboards_QueryLeaderboardRanksOptions
{
    /** API version: must be EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST */
    int32_t                  ApiVersion;
    /** Leaderboard ID to query */
    const char*              LeaderboardId;
    /** Optional local user ID (NULL = none) */
    const EOS_ProductUserId* LocalUserId;
} EOS_Leaderboards_QueryLeaderboardRanksOptions;

typedef struct EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LeaderboardId;
} EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallback)(const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_QueryLeaderboardUserScoresOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_QUERYLEADERBOARDUSERSCORES_API_LATEST 2

typedef struct EOS_Leaderboards_QueryLeaderboardUserScoresOptions
{
    /** API version: must be EOS_LEADERBOARDS_QUERYLEADERBOARDUSERSCORES_API_LATEST */
    int32_t                   ApiVersion;
    /** Array of Product User IDs to query scores for */
    const EOS_ProductUserId*  UserIds;
    /** Number of entries in UserIds */
    uint32_t                  UserIdsCount;
    /** Name of the stat to aggregate */
    const char*               StatName;
    /** Aggregation type */
    EOS_ELeaderboardAggregation Aggregation;
    /** UTC Unix start time (-1 = no restriction) */
    int64_t                   StartTime;
    /** UTC Unix end time (-1 = no restriction) */
    int64_t                   EndTime;
    /** Optional local user ID (NULL = none) */
    const EOS_ProductUserId*  LocalUserId;
} EOS_Leaderboards_QueryLeaderboardUserScoresOptions;

typedef struct EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
} EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallback)(const EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_GetLeaderboardDefinitionCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_GETLEADERBOARDDEFINITIONCOUNT_API_LATEST 1

typedef struct EOS_Leaderboards_GetLeaderboardDefinitionCountOptions
{
    /** API version: must be EOS_LEADERBOARDS_GETLEADERBOARDDEFINITIONCOUNT_API_LATEST */
    int32_t ApiVersion;
} EOS_Leaderboards_GetLeaderboardDefinitionCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_COPYLEADERBOARDDEFINITIONBYINDEX_API_LATEST 1

typedef struct EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions
{
    /** API version: must be EOS_LEADERBOARDS_COPYLEADERBOARDDEFINITIONBYINDEX_API_LATEST */
    int32_t  ApiVersion;
    /** Zero-based index of the definition to copy */
    uint32_t LeaderboardIndex;
} EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_GetLeaderboardRecordCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST 1

typedef struct EOS_Leaderboards_GetLeaderboardRecordCountOptions
{
    /** API version: must be EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST */
    int32_t ApiVersion;
} EOS_Leaderboards_GetLeaderboardRecordCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST 1

typedef struct EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions
{
    /** API version: must be EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST */
    int32_t  ApiVersion;
    /** Zero-based index of the record to copy */
    uint32_t LeaderboardRecordIndex;
} EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Leaderboards interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries leaderboard definitions */
typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardDefinitions_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardDefinitionsOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallback CompletionDelegate);

/** Queries top-ranked scores for a leaderboard */
typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardRanks_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardRanksOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallback CompletionDelegate);

/** Queries scores for a specific set of users */
typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardUserScores_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardUserScoresOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallback CompletionDelegate);

/** Returns the number of cached leaderboard definitions */
typedef uint32_t    (EOS_CALL *EOS_Leaderboards_GetLeaderboardDefinitionCount_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_GetLeaderboardDefinitionCountOptions* Options);

/** Copies a cached leaderboard definition by index; release with EOS_Leaderboards_LeaderboardDefinition_Release */
typedef EOS_EResult (EOS_CALL *EOS_Leaderboards_CopyLeaderboardDefinitionByIndex_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions* Options, EOS_Leaderboards_Definition** OutLeaderboardDefinition);

/** Returns the number of cached leaderboard records */
typedef uint32_t    (EOS_CALL *EOS_Leaderboards_GetLeaderboardRecordCount_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_GetLeaderboardRecordCountOptions* Options);

/** Copies a cached leaderboard record by index; release with EOS_Leaderboards_LeaderboardRecord_Release */
typedef EOS_EResult (EOS_CALL *EOS_Leaderboards_CopyLeaderboardRecordByIndex_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions* Options, EOS_Leaderboards_LeaderboardRecord** OutLeaderboardRecord);

/** Releases a leaderboard definition allocated by CopyLeaderboardDefinitionByIndex */
typedef void        (EOS_CALL *EOS_Leaderboards_LeaderboardDefinition_Release_t)(EOS_Leaderboards_Definition* LeaderboardDefinition);

/** Releases a leaderboard record allocated by CopyLeaderboardRecordByIndex */
typedef void        (EOS_CALL *EOS_Leaderboards_LeaderboardRecord_Release_t)(EOS_Leaderboards_LeaderboardRecord* LeaderboardRecord);

#ifdef __cplusplus
}
#endif
