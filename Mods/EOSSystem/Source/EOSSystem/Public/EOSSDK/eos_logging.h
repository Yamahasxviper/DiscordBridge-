// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_logging.h — EOS SDK logging interface, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_base.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELogLevel
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELogLevel
{
    EOS_LOG_Off          = 0,
    EOS_LOG_Fatal        = 100,
    EOS_LOG_Error        = 200,
    EOS_LOG_Warning      = 300,
    EOS_LOG_Info         = 400,
    EOS_LOG_Verbose      = 500,
    EOS_LOG_VeryVerbose  = 600
} EOS_ELogLevel;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELogCategory
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELogCategory
{
    EOS_LC_Core                  = 0,
    EOS_LC_Auth                  = 1,
    EOS_LC_Friends               = 2,
    EOS_LC_Presence              = 3,
    EOS_LC_UserInfo              = 4,
    EOS_LC_HttpSerialization     = 5,
    EOS_LC_Ecom                  = 6,
    EOS_LC_P2P                   = 7,
    EOS_LC_Sessions              = 8,
    EOS_LC_RateLimiter           = 9,
    EOS_LC_PlayerDataStorage     = 10,
    EOS_LC_Analytics             = 11,
    EOS_LC_Messaging             = 12,
    EOS_LC_Connect               = 13,
    EOS_LC_Overlay               = 14,
    EOS_LC_Achievements          = 15,
    EOS_LC_Stats                 = 16,
    EOS_LC_UI                    = 17,
    EOS_LC_Lobby                 = 18,
    EOS_LC_Leaderboards          = 19,
    EOS_LC_Keychain              = 20,
    EOS_LC_IntegratedPlatform    = 21,
    EOS_LC_TitleStorage          = 22,
    EOS_LC_Mods                  = 23,
    EOS_LC_AntiCheat             = 24,
    EOS_LC_Sanctions             = 25,
    EOS_LC_Reports               = 26,
    EOS_LC_ProgressionSnapshot   = 27,
    EOS_LC_RTC                   = 28,
    EOS_LC_RTCAdmin              = 29,
    EOS_LC_KWS                   = 30,
    EOS_LC_AllCategories         = 0x7fffffff
} EOS_ELogCategory;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_LogMessage
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOGMESSAGE_API_LATEST 1

typedef struct EOS_LogMessage
{
    /** API version: must be EOS_LOGMESSAGE_API_LATEST */
    int32_t            ApiVersion;
    /** Log category */
    EOS_ELogCategory   Category;
    /** Human-readable UTF-8 log message */
    const char*        Message;
    /** Severity level */
    EOS_ELogLevel      Level;
} EOS_LogMessage;

// ─────────────────────────────────────────────────────────────────────────────
//  Callback typedef
// ─────────────────────────────────────────────────────────────────────────────
typedef void (EOS_CALL *EOS_LogMessageFunc)(const EOS_LogMessage* InLogMessage);

#ifdef __cplusplus
}
#endif
