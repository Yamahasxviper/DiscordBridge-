// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_stats.h — delegates to the real EOS SDK eos_stats.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_stats.h via the EOSSDK module include path.
#include <eos_stats.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Stats interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_stats.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Stats_IngestStat_t)(EOS_HStats Handle, const EOS_Stats_IngestStatOptions* Options, void* ClientData, EOS_Stats_OnIngestStatCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Stats_QueryStats_t)(EOS_HStats Handle, const EOS_Stats_QueryStatsOptions* Options, void* ClientData, EOS_Stats_OnQueryStatsCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Stats_GetStatsCount_t)(EOS_HStats Handle, const EOS_Stats_GetStatCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByIndex_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByIndexOptions* Options, EOS_Stats_Stat** OutStat);
typedef EOS_EResult (EOS_CALL *EOS_Stats_CopyStatByName_t)(EOS_HStats Handle, const EOS_Stats_CopyStatByNameOptions* Options, EOS_Stats_Stat** OutStat);
typedef void        (EOS_CALL *EOS_Stats_Stat_Release_t)(EOS_Stats_Stat* Stat);
