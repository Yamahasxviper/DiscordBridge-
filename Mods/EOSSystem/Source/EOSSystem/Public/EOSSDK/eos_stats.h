// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_stats.h — delegates to the real EOS SDK eos_stats.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_stats.h>)
#  include <eos_stats.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Stats interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Stats_IngestStat_t)(EOS_HStats Handle, const EOS_Stats_IngestStatOptions* Options, void* ClientData, EOS_Stats_OnIngestStatCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Stats_QueryStats_t)(EOS_HStats Handle, const EOS_Stats_QueryStatsOptions* Options, void* ClientData, EOS_Stats_OnQueryStatsCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Stats_GetStatsCount_t)(EOS_HStats Handle, const EOS_Stats_GetStatCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByIndex_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByIndexOptions* Options, EOS_Stats_Stat** OutStat);
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByName_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByNameOptions* Options, EOS_Stats_Stat** OutStat);
typedef void        (EOS_CALL *EOS_Stats_Stat_Release_t)(EOS_Stats_Stat* Stat);
