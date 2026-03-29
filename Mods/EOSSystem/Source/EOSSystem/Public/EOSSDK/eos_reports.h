// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_reports.h — EOS SDK Reports interface, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EPlayerReportsCategory
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EPlayerReportsCategory
{
    EOS_PRC_Invalid           = 0,
    EOS_PRC_Cheating          = 1,
    EOS_PRC_Exploiting        = 2,
    EOS_PRC_OffensiveProfile  = 3,
    EOS_PRC_VerbalAbuse       = 4,
    EOS_PRC_Scamming          = 5,
    EOS_PRC_Spamming          = 6,
    EOS_PRC_Other             = 7
} EOS_EPlayerReportsCategory;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Reports_SendPlayerBehaviorReportOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_REPORTS_SENDPLAYERBEHAVIORREPORT_API_LATEST 1

typedef struct EOS_Reports_SendPlayerBehaviorReportOptions
{
    /** API version: must be EOS_REPORTS_SENDPLAYERBEHAVIORREPORT_API_LATEST */
    int32_t                    ApiVersion;
    /** Product User ID of the player submitting the report */
    EOS_ProductUserId          ReporterUserId;
    /** Product User ID of the player being reported */
    EOS_ProductUserId          ReportedUserId;
    /** Category of the report */
    EOS_EPlayerReportsCategory Category;
    /** Optional free-form message from the reporter (may be NULL) */
    const char*                Message;
    /** Optional context string for game-specific metadata (may be NULL) */
    const char*                Context;
} EOS_Reports_SendPlayerBehaviorReportOptions;

typedef struct EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
} EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo;

typedef void (EOS_CALL *EOS_Reports_OnSendPlayerBehaviorReportCompleteCallback)(const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  Reports interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Submits a player behaviour report */
typedef void (EOS_CALL *EOS_Reports_SendPlayerBehaviorReport_t)(EOS_HReports Handle, const EOS_Reports_SendPlayerBehaviorReportOptions* Options, void* ClientData, EOS_Reports_OnSendPlayerBehaviorReportCompleteCallback CompletionDelegate);

#ifdef __cplusplus
}
#endif
