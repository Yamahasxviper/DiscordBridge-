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
// INCLUDE GUARD
// ─────────────
// EOS_BASE_H matches the named guard used by the engine's ThirdParty EOSSDK
// eos_base.h.  Whichever file is processed first sets the guard and prevents
// the other from re-defining the same types.

#pragma once

#ifndef EOS_BASE_H
#define EOS_BASE_H

// Delegate to the real EOS SDK's eos_base.h via the EOSSDK module include
// path.  Angle-bracket form is used so the compiler searches /I paths
// (finding Engine/Source/ThirdParty/EOSSDK/SDK/Include/eos_base.h) rather
// than the current directory (Public/EOSSDK/ is not an /I root).
#include <eos_base.h>

// Sentinel used by DummyHeaders' eos_platform.h to prevent EOS_HPlatform
// from being re-defined after this header has been processed.
#ifndef EOS_HPlatform_DEFINED
#define EOS_HPlatform_DEFINED
#endif

#endif // EOS_BASE_H
