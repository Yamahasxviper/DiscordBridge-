// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sdk.h — Master include for the EOS C SDK delegation wrapper headers.
//
// All sub-headers delegate to the real engine EOSSDK headers via angle-bracket
// includes (e.g. <eos_auth.h>).  EOSSystem.Build.cs lists "EOSSDK" as a public
// dependency so UBT adds the engine's ThirdParty EOSSDK include path.
//
// Delegating to the canonical engine headers prevents C2371/C2011/C3431 type
// redefinition conflicts when BanSystem includes both EOSSystem and EOSDirectSDK
// (which also pulls in the real EOSSDK headers via EOSShared) in the same TU.
//
// eos_platform.h is the sole exception: the CSS engine ships eos_types.h but
// omits eos_platform.h, so that file remains a hand-written wrapper around
// <eos_types.h> and provides the missing function-pointer typedefs and
// C-linkage function declarations.
//
// Include this single header to pull in all EOS SDK type definitions
// used by the EOSSystem mod.

#pragma once

// Foundation
#include "eos_base.h"
#include "eos_common.h"
#include "eos_logging.h"
#include "eos_init.h"
#include "eos_platform.h"

// Authentication / identity
#include "eos_auth.h"
#include "eos_connect.h"
#include "eos_userinfo.h"

// Social
#include "eos_friends.h"
#include "eos_presence.h"

// Multiplayer
#include "eos_sessions.h"
#include "eos_lobby.h"

// Security
#include "eos_anticheat.h"
#include "eos_sanctions.h"

// Progression
#include "eos_stats.h"
#include "eos_achievements.h"
#include "eos_leaderboards.h"

// Storage
#include "eos_storage.h"

// Analytics / reporting
#include "eos_metrics.h"
#include "eos_reports.h"

// Commerce
#include "eos_ecom.h"

// Voice
#include "eos_rtc.h"
