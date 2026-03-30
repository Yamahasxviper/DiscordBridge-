// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_reports.h — delegates to the real EOS SDK eos_reports.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_reports.h via the EOSSDK module include path.
#include <eos_reports.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Reports interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_reports.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void (EOS_CALL *EOS_Reports_SendPlayerBehaviorReport_t)(EOS_HReports Handle, const EOS_Reports_SendPlayerBehaviorReportOptions* Options, void* ClientData, EOS_Reports_OnSendPlayerBehaviorReportCompleteCallback CompletionDelegate);
