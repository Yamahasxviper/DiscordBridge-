// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_achievements.h — delegates to the real EOS SDK eos_achievements.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_achievements.h via the EOSSDK module include path.
#include <eos_achievements.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Achievements interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_achievements.h declares the functions but does not
//  provide these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Achievements_QueryPlayerAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_QueryPlayerAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnQueryPlayerAchievementsCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Achievements_GetPlayerAchievementCount_t)(EOS_HAchievements Handle, const EOS_Achievements_GetPlayerAchievementCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Achievements_CopyPlayerAchievementByIndex_t)(EOS_HAchievements Handle, const EOS_Achievements_CopyPlayerAchievementByIndexOptions* Options, EOS_Achievements_PlayerAchievement** OutAchievement);
typedef void        (EOS_CALL *EOS_Achievements_PlayerAchievement_Release_t)(EOS_Achievements_PlayerAchievement* Achievement);
typedef void        (EOS_CALL *EOS_Achievements_UnlockAchievements_t)(EOS_HAchievements Handle, const EOS_Achievements_UnlockAchievementsOptions* Options, void* ClientData, EOS_Achievements_OnUnlockAchievementsCompleteCallback CompletionDelegate);
