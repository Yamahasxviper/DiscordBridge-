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
// NOTE: Do NOT add a named include guard matching the real SDK's guard here.
// Defining EOS_INIT_H before #include <eos_init.h> causes the real header's
// own guard check to see it already set and skip all content — leaving
// EOS_InitializeOptions and related types undefined.  #pragma once is
// sufficient to prevent this wrapper from being processed twice.

#pragma once

// Delegate to the real EOS SDK's eos_init.h via the EOSSDK module include path.
#include <eos_init.h>
