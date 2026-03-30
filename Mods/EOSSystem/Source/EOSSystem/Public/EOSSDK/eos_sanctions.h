// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sanctions.h — delegates to the real EOS SDK eos_sanctions.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_sanctions.h via the EOSSDK module include path.
#include <eos_sanctions.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Sanctions interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_sanctions.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Sanctions_QueryActivePlayerSanctions_t)(EOS_HSanctions Handle, const EOS_Sanctions_QueryActivePlayerSanctionsOptions* Options, void* ClientData, EOS_Sanctions_OnQueryActivePlayerSanctionsCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_Sanctions_GetPlayerSanctionCount_t)(EOS_HSanctions Handle, const EOS_Sanctions_GetPlayerSanctionCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Sanctions_CopyPlayerSanctionByIndex_t)(EOS_HSanctions Handle, const EOS_Sanctions_CopyPlayerSanctionByIndexOptions* Options, EOS_Sanctions_PlayerSanction** OutSanction);
typedef void        (EOS_CALL *EOS_Sanctions_PlayerSanction_Release_t)(EOS_Sanctions_PlayerSanction* Sanction);
