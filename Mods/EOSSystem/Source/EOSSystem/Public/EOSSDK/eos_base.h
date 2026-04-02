// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_base.h — delegates to the real EOS SDK eos_base.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.  All EOS
// type definitions come from those canonical headers; this file is kept only
// so that relative includes from within the EOSSDK/ subdirectory continue
// to resolve correctly.
//
// NOTE: Do NOT add a named include guard matching the real SDK's guard here.
// Defining EOS_BASE_H before #include <eos_base.h> causes the real header's
// own guard check to see it already set and skip all content — leaving
// EOS_CALL, EOS_MEMORY_CALL, etc. undefined.  #pragma once is sufficient to
// prevent this wrapper from being processed twice.

#pragma once

// Delegate to the real EOS SDK's eos_base.h via the EOSSDK module include
// path.  Angle-bracket form is used so the compiler searches /I paths
// (finding Engine/Source/ThirdParty/EOSSDK/SDK/Include/eos_base.h) rather
// than the current directory (Public/EOSSDK/ is not an /I root).
#include <eos_base.h>

// Sentinel used by EOSSystem's EOSDirectSDK/eos_platform.h to prevent EOS_HPlatform
// from being re-defined after this header has been processed.
#ifndef EOS_HPlatform_DEFINED
#define EOS_HPlatform_DEFINED
#endif
