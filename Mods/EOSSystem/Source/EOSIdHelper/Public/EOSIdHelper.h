// Copyright Yamahasxviper. All Rights Reserved.
//
// EOSIdHelper.h  (module: EOSIdHelper, plugin: EOSSystem)
//
// Minimal module header.
//
// NOTE ON EOSId::GetProductUserId
// ────────────────────────────────
// This module previously provided EOSId::GetProductUserId as a duplicate of
// the identical helper in FGOnlineHelpers.  That duplication has been removed
// to avoid an ODR conflict between namespace EOSId (this module) and the same
// namespace defined in FGOnlineHelpers.
//
// Use FGOnlineHelpers (module: FGOnlineHelpers, plugin: EOSSystem) for
// EOSId::GetProductUserId.  FGOnlineHelpers is the standard module that every
// Alpakit C++ mod in this project already lists as a dependency.
//
// EOSBanSDK.h is still provided by this module as a convenience forwarder
// to EOSDirectSDK.h.  New code should depend on "EOSDirectSDK" directly and
// include "EOSDirectSDK.h" instead.

#pragma once

#include "CoreMinimal.h"

