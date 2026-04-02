// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_leaderboards.h — delegates to the real EOS SDK eos_leaderboards.h, then
// adds _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_leaderboards.h>)
#  include <eos_leaderboards.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Leaderboards interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardDefinitions_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardDefinitionsOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardRanks_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardRanksOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Leaderboards_QueryLeaderboardUserScores_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_QueryLeaderboardUserScoresOptions* Options, void* ClientData, EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Leaderboards_GetLeaderboardDefinitionCount_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_GetLeaderboardDefinitionCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Leaderboards_CopyLeaderboardDefinitionByIndex_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_CopyLeaderboardDefinitionByIndexOptions* Options, EOS_Leaderboards_Definition** OutLeaderboardDefinition);
typedef uint32_t    (EOS_CALL *EOS_Leaderboards_GetLeaderboardRecordCount_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_GetLeaderboardRecordCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Leaderboards_CopyLeaderboardRecordByIndex_t)(EOS_HLeaderboards Handle, const EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions* Options, EOS_Leaderboards_LeaderboardRecord** OutLeaderboardRecord);
typedef void        (EOS_CALL *EOS_Leaderboards_LeaderboardDefinition_Release_t)(EOS_Leaderboards_Definition* LeaderboardDefinition);
typedef void        (EOS_CALL *EOS_Leaderboards_LeaderboardRecord_Release_t)(EOS_Leaderboards_LeaderboardRecord* LeaderboardRecord);
