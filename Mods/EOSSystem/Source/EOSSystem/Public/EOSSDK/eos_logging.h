// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_logging.h — delegates to the real EOS SDK eos_logging.h, with fallback
// definitions for the CSS UE5.3.2 engine which may ship a minimal EOSSDK that
// omits EOS_ELogCategory and EOS_ELogLevel enum value constants.
//
// See eos_common.h for the rationale behind the #ifndef-per-value fallback
// pattern (handles cases where the real SDK defines types-only, without values).

#pragma once

#if defined(__has_include) && __has_include(<eos_logging.h>)
#  include <eos_logging.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_ELogLevel value constants.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_LOG_Off
#  define EOS_LOG_Off          0
#  define EOS_LOG_Fatal        100
#  define EOS_LOG_Error        200
#  define EOS_LOG_Warning      300
#  define EOS_LOG_Info         400
#  define EOS_LOG_Verbose      500
#  define EOS_LOG_VeryVerbose  600
#endif // EOS_LOG_Off

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_ELogCategory value constants.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_LC_Core
#  define EOS_LC_Core                  0
#  define EOS_LC_Auth                  1
#  define EOS_LC_Friends               2
#  define EOS_LC_Presence              3
#  define EOS_LC_UserInfo              4
#  define EOS_LC_HttpSerialization     5
#  define EOS_LC_Ecom                  6
#  define EOS_LC_P2P                   7
#  define EOS_LC_Sessions              8
#  define EOS_LC_RateLimiter           9
#  define EOS_LC_PlayerDataStorage     10
#  define EOS_LC_Analytics             11
#  define EOS_LC_Messaging             12
#  define EOS_LC_Connect               13
#  define EOS_LC_Overlay               14
#  define EOS_LC_Achievements          15
#  define EOS_LC_Stats                 16
#  define EOS_LC_UI                    17
#  define EOS_LC_Lobby                 18
#  define EOS_LC_Leaderboards          19
#  define EOS_LC_Keychain              20
#  define EOS_LC_IntegratedPlatform    21
#  define EOS_LC_TitleStorage          22
#  define EOS_LC_Mods                  23
#  define EOS_LC_AntiCheat             24
#  define EOS_LC_Reports               25
#  define EOS_LC_Sanctions             26
#  define EOS_LC_ProgressionSnapshots  27
#  define EOS_LC_KWS                   28
#  define EOS_LC_AllCategories         0x7fffffff
#endif // EOS_LC_Core

// Convenience alias: code in EOSSystemSubsystem.cpp uses EOS_LC_ALL_CATEGORIES
// as a shorthand for the pass-all-categories sentinel.
#ifndef EOS_LC_ALL_CATEGORIES
#  define EOS_LC_ALL_CATEGORIES EOS_LC_AllCategories
#endif
