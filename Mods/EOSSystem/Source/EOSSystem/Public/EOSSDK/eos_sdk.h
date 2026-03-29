// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sdk.h — Master include for the self-written EOS C SDK headers.
// Written from scratch using only public EOS SDK documentation
// (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.
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
