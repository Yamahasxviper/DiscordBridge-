// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_achievements.h — delegates to the real EOS SDK eos_achievements.h, then
// adds _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_achievements.h>)
#  include <eos_achievements.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Achievements interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Achievements_QueryPlayerAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_QueryPlayerAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnQueryPlayerAchievementsCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Achievements_GetPlayerAchievementCount_t)(EOS_HAchievements Handle, const EOS_Achievements_GetPlayerAchievementCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Achievements_CopyPlayerAchievementByIndex_t)(EOS_HAchievements Handle, const EOS_Achievements_CopyPlayerAchievementByIndexOptions* Options, EOS_Achievements_PlayerAchievement** OutAchievement);
typedef void        (EOS_CALL *EOS_Achievements_PlayerAchievement_Release_t)(EOS_Achievements_PlayerAchievement* Achievement);
typedef void        (EOS_CALL *EOS_Achievements_UnlockAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_UnlockAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnUnlockAchievementsCompleteCallback CompletionDelegate);
