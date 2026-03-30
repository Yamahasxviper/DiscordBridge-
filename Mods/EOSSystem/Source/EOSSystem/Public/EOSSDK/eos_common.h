// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_common.h — delegates to the real EOS SDK eos_common.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.  All EOS
// type definitions (EOS_EResult, EOS_EpicAccountId, EOS_ProductUserId,
// EOS_ELoginStatus, EOS_EExternalAccountType, etc.) come from the canonical
// engine header.  This prevents C2371/C2011 redefinition conflicts when
// BanSystem includes both EOSSystem and EOSDirectSDK (which also pulls in
// the real EOSSDK eos_common.h via EOSShared) in the same translation unit.
//
// INCLUDE GUARD
// ─────────────
// EOS_COMMON_H matches the named guard used by the engine's ThirdParty EOSSDK
// eos_common.h.  Whichever file is processed first sets the guard and prevents
// the other from re-defining the same types.

#pragma once

#ifndef EOS_COMMON_H
#define EOS_COMMON_H

// Delegate to the real EOS SDK's eos_common.h via the EOSSDK module include
// path.  Angle-bracket form finds Engine/Source/ThirdParty/EOSSDK/SDK/Include/
// eos_common.h rather than this directory (Public/EOSSDK/ is not an /I root).
#include <eos_common.h>

#endif // EOS_COMMON_H
