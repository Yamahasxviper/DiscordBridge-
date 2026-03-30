// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_metrics.h — delegates to the real EOS SDK eos_metrics.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_metrics.h via the EOSSDK module include path.
#include <eos_metrics.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Metrics interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_metrics.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef EOS_EResult (EOS_CALL *EOS_Metrics_BeginPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_BeginPlayerSessionOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Metrics_EndPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_EndPlayerSessionOptions* Options);
