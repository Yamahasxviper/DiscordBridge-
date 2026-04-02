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
// Use explicit C-style casts so these constants work whether EOS_ELogLevel is
// a plain typedef-int, a C unscoped enum, or a C++ scoped enum class (which
// does not allow implicit integer comparisons or returns).
#  define EOS_LOG_Off          ((EOS_ELogLevel)0)
#  define EOS_LOG_Fatal        ((EOS_ELogLevel)100)
#  define EOS_LOG_Error        ((EOS_ELogLevel)200)
#  define EOS_LOG_Warning      ((EOS_ELogLevel)300)
#  define EOS_LOG_Info         ((EOS_ELogLevel)400)
#  define EOS_LOG_Verbose      ((EOS_ELogLevel)500)
#  define EOS_LOG_VeryVerbose  ((EOS_ELogLevel)600)
#endif // EOS_LOG_Off

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_ELogCategory value constants.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_LC_Core
// Use explicit C-style casts so these constants work whether EOS_ELogCategory
// is a plain typedef-int, a C unscoped enum, or a C++ scoped enum class.
#  define EOS_LC_Core                  ((EOS_ELogCategory)0)
#  define EOS_LC_Auth                  ((EOS_ELogCategory)1)
#  define EOS_LC_Friends               ((EOS_ELogCategory)2)
#  define EOS_LC_Presence              ((EOS_ELogCategory)3)
#  define EOS_LC_UserInfo              ((EOS_ELogCategory)4)
#  define EOS_LC_HttpSerialization     ((EOS_ELogCategory)5)
#  define EOS_LC_Ecom                  ((EOS_ELogCategory)6)
#  define EOS_LC_P2P                   ((EOS_ELogCategory)7)
#  define EOS_LC_Sessions              ((EOS_ELogCategory)8)
#  define EOS_LC_RateLimiter           ((EOS_ELogCategory)9)
#  define EOS_LC_PlayerDataStorage     ((EOS_ELogCategory)10)
#  define EOS_LC_Analytics             ((EOS_ELogCategory)11)
#  define EOS_LC_Messaging             ((EOS_ELogCategory)12)
#  define EOS_LC_Connect               ((EOS_ELogCategory)13)
#  define EOS_LC_Overlay               ((EOS_ELogCategory)14)
#  define EOS_LC_Achievements          ((EOS_ELogCategory)15)
#  define EOS_LC_Stats                 ((EOS_ELogCategory)16)
#  define EOS_LC_UI                    ((EOS_ELogCategory)17)
#  define EOS_LC_Lobby                 ((EOS_ELogCategory)18)
#  define EOS_LC_Leaderboards          ((EOS_ELogCategory)19)
#  define EOS_LC_Keychain              ((EOS_ELogCategory)20)
#  define EOS_LC_IntegratedPlatform    ((EOS_ELogCategory)21)
#  define EOS_LC_TitleStorage          ((EOS_ELogCategory)22)
#  define EOS_LC_Mods                  ((EOS_ELogCategory)23)
#  define EOS_LC_AntiCheat             ((EOS_ELogCategory)24)
#  define EOS_LC_Reports               ((EOS_ELogCategory)25)
#  define EOS_LC_Sanctions             ((EOS_ELogCategory)26)
#  define EOS_LC_ProgressionSnapshots  ((EOS_ELogCategory)27)
#  define EOS_LC_KWS                   ((EOS_ELogCategory)28)
#  define EOS_LC_AllCategories         ((EOS_ELogCategory)0x7fffffff)
#endif // EOS_LC_Core

// Convenience alias: code in EOSSystemSubsystem.cpp uses EOS_LC_ALL_CATEGORIES
// as a shorthand for the pass-all-categories sentinel.
#ifndef EOS_LC_ALL_CATEGORIES
#  define EOS_LC_ALL_CATEGORIES EOS_LC_AllCategories
#endif
