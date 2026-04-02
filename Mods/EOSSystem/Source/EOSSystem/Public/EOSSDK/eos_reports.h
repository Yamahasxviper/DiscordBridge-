// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_reports.h — delegates to the real EOS SDK eos_reports.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_reports.h>)
#  include <eos_reports.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Reports interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void (EOS_CALL *EOS_Reports_SendPlayerBehaviorReport_t)(EOS_HReports Handle, const EOS_Reports_SendPlayerBehaviorReportOptions* Options, void* ClientData, EOS_Reports_OnSendPlayerBehaviorReportCompleteCallback CompletionDelegate);
