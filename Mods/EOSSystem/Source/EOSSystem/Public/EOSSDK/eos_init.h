// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_init.h — delegates to the real EOS SDK eos_init.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.  All
// EOS_InitializeOptions / EOS_Initialize_ThreadAffinity type definitions
// come from the canonical engine header, preventing C2371 redefinition
// conflicts when BanSystem includes both EOSSystem and EOSDirectSDK headers.
//
// INCLUDE GUARD
// ─────────────
// EOS_INIT_H matches the named guard used by the engine's ThirdParty EOSSDK
// eos_init.h.  Whichever file is processed first sets the guard and prevents
// the other from re-defining the same types.

#pragma once

#ifndef EOS_INIT_H
#define EOS_INIT_H

// Delegate to the real EOS SDK's eos_init.h via the EOSSDK module include path.
#include <eos_init.h>

#endif // EOS_INIT_H
