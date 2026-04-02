// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sanctions.h — delegates to the real EOS SDK eos_sanctions.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_sanctions.h>)
#  include <eos_sanctions.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Sanctions interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Sanctions_QueryActivePlayerSanctions_t)(EOS_HSanctions Handle, const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options, void* ClientData, EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate);
typedef int32_t     (EOS_CALL *EOS_Sanctions_GetPlayerSanctionCount_t)(EOS_HSanctions Handle, const EOS_Sanctions_GetPlayerSanctionCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Sanctions_CopyPlayerSanctionByIndex_t)(EOS_HSanctions Handle, const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options, EOS_Sanctions_PlayerSanction** OutSanction);
typedef void        (EOS_CALL *EOS_Sanctions_PlayerSanction_Release_t)(EOS_Sanctions_PlayerSanction* Sanction);
