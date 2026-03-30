// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sdk.h — Master include for EOSSystem's EOS C SDK delegation headers.
//
// Trimmed to the subset needed by the ban system:
//   • EOS Platform init / tick (eos_platform.h)
//   • EOS Connect interface — PUID lookup, Steam64↔PUID mapping (eos_connect.h)
//
// All other EOS subsystems (sessions, metrics, anti-cheat, sanctions, stats,
// achievements, leaderboards, storage, friends, presence, ecom, lobby, RTC,
// reports, userinfo) have been removed — they are not used by the ban system.

#pragma once

// Foundation
#include "eos_base.h"
#include "eos_common.h"
#include "eos_logging.h"
#include "eos_init.h"
#include "eos_platform.h"

// Identity / PUID lookup (required for cross-platform Steam↔EOS ban matching)
#include "eos_connect.h"
