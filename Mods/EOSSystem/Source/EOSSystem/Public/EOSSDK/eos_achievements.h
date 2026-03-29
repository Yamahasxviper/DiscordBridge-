// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_achievements.h — EOS SDK Achievements interface, written from scratch
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
//  Forward declare the Achievements handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_AchievementsHandleDetails;
typedef struct EOS_AchievementsHandleDetails* EOS_HAchievements;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Achievements_PlayerAchievement
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ACHIEVEMENTS_PLAYERACHIEVEMENT_API_LATEST 2

typedef struct EOS_Achievements_PlayerAchievement
{
    /** API version: must be EOS_ACHIEVEMENTS_PLAYERACHIEVEMENT_API_LATEST */
    int32_t     ApiVersion;
    /** Achievement definition ID */
    const char* AchievementId;
    /** Progress toward unlocking (0.0 – 1.0) */
    double      Progress;
    /** UTC Unix timestamp when the achievement was unlocked (-1 = not unlocked) */
    int64_t     UnlockTime;
} EOS_Achievements_PlayerAchievement;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Achievements_UnlockAchievementsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST 1

typedef struct EOS_Achievements_UnlockAchievementsOptions
{
    /** API version: must be EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the user unlocking achievements */
    EOS_ProductUserId UserId;
    /** Array of achievement ID strings to unlock */
    const char**      AchievementIds;
    /** Number of entries in AchievementIds */
    uint32_t          AchievementsCount;
} EOS_Achievements_UnlockAchievementsOptions;

typedef struct EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId UserId;
    /** Number of achievements that were unlocked */
    uint32_t          AchievementsCount;
} EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Achievements_OnUnlockAchievementsCompleteCallback)(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Achievements_QueryPlayerAchievementsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST 2

typedef struct EOS_Achievements_QueryPlayerAchievementsOptions
{
    /** API version: must be EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user making the request */
    EOS_ProductUserId LocalUserId;
    /** Product User ID of the player whose achievements to query */
    EOS_ProductUserId TargetUserId;
} EOS_Achievements_QueryPlayerAchievementsOptions;

typedef struct EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ProductUserId TargetUserId;
} EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Achievements_OnQueryPlayerAchievementsCompleteCallback)(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Achievements_GetPlayerAchievementCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST 1

typedef struct EOS_Achievements_GetPlayerAchievementCountOptions
{
    /** API version: must be EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the target user */
    EOS_ProductUserId UserId;
} EOS_Achievements_GetPlayerAchievementCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Achievements_CopyPlayerAchievementByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST 2

typedef struct EOS_Achievements_CopyPlayerAchievementByIndexOptions
{
    /** API version: must be EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user making the request */
    EOS_ProductUserId LocalUserId;
    /** Product User ID of the target user */
    EOS_ProductUserId TargetUserId;
    /** Zero-based index of the achievement to copy */
    uint32_t          AchievementIndex;
} EOS_Achievements_CopyPlayerAchievementByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Achievements interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries player achievement data from the backend */
typedef void        (EOS_CALL *EOS_Achievements_QueryPlayerAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_QueryPlayerAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnQueryPlayerAchievementsCompleteCallback CompletionDelegate);

/** Returns the number of cached player achievements */
typedef uint32_t    (EOS_CALL *EOS_Achievements_GetPlayerAchievementCount_t)(EOS_HAchievements Handle, const EOS_Achievements_GetPlayerAchievementCountOptions* Options);

/** Copies a cached player achievement by index; caller must release with EOS_Achievements_PlayerAchievement_Release */
typedef EOS_EResult (EOS_CALL *EOS_Achievements_CopyPlayerAchievementByIndex_t)(EOS_HAchievements Handle, const EOS_Achievements_CopyPlayerAchievementByIndexOptions* Options, EOS_Achievements_PlayerAchievement** OutAchievement);

/** Releases memory allocated by EOS_Achievements_CopyPlayerAchievementByIndex */
typedef void        (EOS_CALL *EOS_Achievements_PlayerAchievement_Release_t)(EOS_Achievements_PlayerAchievement* Achievement);

/** Unlocks one or more achievements for the specified user */
typedef void        (EOS_CALL *EOS_Achievements_UnlockAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_UnlockAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnUnlockAchievementsCompleteCallback CompletionDelegate);

#ifdef __cplusplus
}
#endif
