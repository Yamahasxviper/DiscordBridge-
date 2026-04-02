// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sdk.h — Master include for EOSSystem's EOS C SDK delegation headers.
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
// Trimmed to the subset needed by the ban system:
//   • EOS Platform init / tick (eos_platform.h)
//   • EOS Connect interface — PUID lookup, Steam64↔PUID mapping (eos_connect.h)
//   • Auth, UserInfo, Friends, Presence, Sessions — used by FEOSSDKLoader
//   • AntiCheatServer, Sanctions, Stats, Achievements, Leaderboards
//   • PlayerDataStorage, TitleStorage, Metrics, Reports, Ecom

#pragma once

// Foundation
#include "eos_base.h"
#include "eos_common.h"
#include "eos_logging.h"
#include "eos_init.h"
#include "eos_platform.h"

// Identity / PUID lookup (required for cross-platform Steam↔EOS ban matching)
#include "eos_connect.h"

// Additional EOS subsystems used by FEOSSDKLoader
#include "eos_auth.h"
#include "eos_userinfo.h"
#include "eos_friends.h"
#include "eos_presence.h"
#include "eos_sessions.h"
#include "eos_anticheat.h"
#include "eos_sanctions.h"
#include "eos_stats.h"
#include "eos_achievements.h"
#include "eos_leaderboards.h"
#include "eos_storage.h"
#include "eos_metrics.h"
#include "eos_reports.h"
#include "eos_ecom.h"
