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
// NOTE: Do NOT add a named include guard matching the real SDK's guard here.
// Defining EOS_COMMON_H before #include <eos_common.h> causes the real
// header's own guard check to see it already set and skip all content —
// leaving EOS_Success, EOS_EResult, and all other constants undefined.
// #pragma once is sufficient to prevent this wrapper from being processed twice.

#pragma once

// Delegate to the real EOS SDK's eos_common.h via the EOSSDK module include
// path.  Angle-bracket form finds Engine/Source/ThirdParty/EOSSDK/SDK/Include/
// eos_common.h rather than this directory (Public/EOSSDK/ is not an /I root).
#include <eos_common.h>
