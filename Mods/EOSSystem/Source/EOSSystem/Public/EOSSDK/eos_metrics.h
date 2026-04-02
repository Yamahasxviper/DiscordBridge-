// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_metrics.h — delegates to the real EOS SDK eos_metrics.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_metrics.h>)
#  include <eos_metrics.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Metrics interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef EOS_EResult (EOS_CALL *EOS_Metrics_BeginPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_BeginPlayerSessionOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Metrics_EndPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_EndPlayerSessionOptions* Options);
