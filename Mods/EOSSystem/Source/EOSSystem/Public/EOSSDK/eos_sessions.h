// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sessions.h — delegates to the real EOS SDK eos_sessions.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C2011 conflicts for EOS_EAttributeType and
// EOS_EOnlineComparisonOp (which the CSS engine places in eos_common.h)
// when BanSystem includes both EOSSystem and EOSDirectSDK headers.

#pragma once

// Delegate to the real EOS SDK's eos_sessions.h via the EOSSDK module include path.
#include <eos_sessions.h>
