// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_stats.h — EOS SDK Stats interface, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the Stats handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_StatsHandle;
typedef struct EOS_StatsHandle* EOS_HStats;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_Stat
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_STAT_API_LATEST 1

typedef struct EOS_Stats_Stat
{
    /** API version: must be EOS_STATS_STAT_API_LATEST */
    int32_t     ApiVersion;
    /** Name of the stat */
    const char* Name;
    /** Current integer value of the stat */
    int32_t     Value;
} EOS_Stats_Stat;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_IngestData
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_INGESTDATA_API_LATEST 1

typedef struct EOS_Stats_IngestData
{
    /** API version: must be EOS_STATS_INGESTDATA_API_LATEST */
    int32_t     ApiVersion;
    /** Name of the stat to ingest */
    const char* StatName;
    /** Amount to add to the stat */
    int32_t     IngestAmount;
} EOS_Stats_IngestData;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_IngestStatOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_INGESTSTAT_API_LATEST 3

typedef struct EOS_Stats_IngestStatOptions
{
    /** API version: must be EOS_STATS_INGESTSTAT_API_LATEST */
    int32_t                    ApiVersion;
    /** Product User ID of the local user ingesting the stats */
    EOS_ProductUserId          LocalUserId;
    /** Product User ID of the target user (may equal LocalUserId) */
    EOS_ProductUserId          TargetUserId;
    /** Array of stat ingest descriptors */
    const EOS_Stats_IngestData* Stats;
    /** Number of entries in Stats */
    uint32_t                   StatsCount;
} EOS_Stats_IngestStatOptions;

typedef struct EOS_Stats_IngestStatCompleteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
} EOS_Stats_IngestStatCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Stats_OnIngestStatCompleteCallback)(const EOS_Stats_IngestStatCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_QueryStatsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_QUERYSTATS_API_LATEST 3

/** Sentinel value meaning "no time restriction" */
#define EOS_STATS_TIME_UNDEFINED  ((int64_t)(-1))

typedef struct EOS_Stats_QueryStatsOptions
{
    /** API version: must be EOS_STATS_QUERYSTATS_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user making the query */
    EOS_ProductUserId LocalUserId;
    /** UTC Unix timestamp for start of aggregation window (-1 = no restriction) */
    int64_t           StartTime;
    /** UTC Unix timestamp for end of aggregation window (-1 = no restriction) */
    int64_t           EndTime;
    /** Optional array of stat name strings to filter (NULL = all stats) */
    const char**      StatNames;
    /** Number of entries in StatNames (0 if StatNames is NULL) */
    uint32_t          StatNamesCount;
    /** Product User ID of the target user to query */
    EOS_ProductUserId TargetUserId;
} EOS_Stats_QueryStatsOptions;

typedef struct EOS_Stats_OnQueryStatsCompleteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
} EOS_Stats_OnQueryStatsCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Stats_OnQueryStatsCompleteCallback)(const EOS_Stats_OnQueryStatsCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_GetStatCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_GETSTATCOUNT_API_LATEST 1

typedef struct EOS_Stats_GetStatCountOptions
{
    /** API version: must be EOS_STATS_GETSTATCOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the target user */
    EOS_ProductUserId TargetUserId;
} EOS_Stats_GetStatCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_CopyStatByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_COPYSTATBYINDEX_API_LATEST 1

typedef struct EOS_Stats_CopyStatByIndexOptions
{
    /** API version: must be EOS_STATS_COPYSTATBYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the target user */
    EOS_ProductUserId TargetUserId;
    /** Zero-based index of the stat to copy */
    uint32_t          StatIndex;
} EOS_Stats_CopyStatByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Stats_CopyStatByNameOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_STATS_COPYSTATBYNAME_API_LATEST 1

typedef struct EOS_Stats_CopyStatByNameOptions
{
    /** API version: must be EOS_STATS_COPYSTATBYNAME_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the target user */
    EOS_ProductUserId TargetUserId;
    /** Name of the stat to copy */
    const char*       Name;
} EOS_Stats_CopyStatByNameOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Stats interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Ingests (increments) one or more stats for the target user */
typedef void        (EOS_CALL *EOS_Stats_IngestStat_t)(EOS_HStats Handle, const EOS_Stats_IngestStatOptions* Options, void* ClientData, EOS_Stats_OnIngestStatCompleteCallback CompletionDelegate);

/** Queries stats for the target user */
typedef void        (EOS_CALL *EOS_Stats_QueryStats_t)(EOS_HStats Handle, const EOS_Stats_QueryStatsOptions* Options, void* ClientData, EOS_Stats_OnQueryStatsCompleteCallback CompletionDelegate);

/** Returns the number of cached stats for the target user */
typedef uint32_t    (EOS_CALL *EOS_Stats_GetStatsCount_t)(EOS_HStats Handle, const EOS_Stats_GetStatCountOptions* Options);

/** Copies a cached stat by index; caller must release with EOS_Stats_Stat_Release */
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByIndex_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByIndexOptions* Options, EOS_Stats_Stat** OutStat);

/** Copies a cached stat by name; caller must release with EOS_Stats_Stat_Release */
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByName_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByNameOptions* Options, EOS_Stats_Stat** OutStat);

/** Releases memory allocated by EOS_Stats_CopyStatByIndex or EOS_Stats_CopyStatByName */
typedef void        (EOS_CALL *EOS_Stats_Stat_Release_t)(EOS_Stats_Stat* Stat);

#ifdef __cplusplus
}
#endif
