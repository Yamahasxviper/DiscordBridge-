// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_metrics.h — EOS SDK Metrics interface, written from scratch using only
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
//  EOS_EUserControllerType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EUserControllerType
{
    EOS_UCT_Unknown         = 0,
    EOS_UCT_MouseKeyboard   = 1,
    EOS_UCT_GamepadControl  = 2,
    EOS_UCT_TouchScreen     = 3
} EOS_EUserControllerType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Metrics_BeginPlayerSessionOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_METRICS_BEGINPLAYERSESSION_API_LATEST 3

typedef struct EOS_Metrics_BeginPlayerSessionOptions
{
    /** API version: must be EOS_METRICS_BEGINPLAYERSESSION_API_LATEST */
    int32_t                  ApiVersion;
    /** Product User ID of the local player */
    EOS_ProductUserId        LocalUserId;
    /** Epic Account ID of the player (may be NULL for non-Epic accounts) */
    EOS_EpicAccountId        AccountId;
    /** UTF-8 display name of the player */
    const char*              DisplayName;
    /** Input controller type */
    EOS_EUserControllerType  ControllerType;
    /** Optional server IP address string for server-side reporting (may be NULL) */
    const char*              ServerIp;
    /** Optional game session ID string (may be NULL) */
    const char*              GameSessionId;
} EOS_Metrics_BeginPlayerSessionOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Metrics_EndPlayerSessionOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_METRICS_ENDPLAYERSESSION_API_LATEST 2

typedef struct EOS_Metrics_EndPlayerSessionOptions
{
    /** API version: must be EOS_METRICS_ENDPLAYERSESSION_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local player ending the session */
    EOS_ProductUserId LocalUserId;
    /** Epic Account ID of the player (may be NULL for non-Epic accounts) */
    EOS_EpicAccountId AccountId;
} EOS_Metrics_EndPlayerSessionOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Metrics interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Begins a metrics player session (synchronous — returns EOS_EResult directly) */
typedef EOS_EResult (EOS_CALL *EOS_Metrics_BeginPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_BeginPlayerSessionOptions* Options);

/** Ends a metrics player session (synchronous — returns EOS_EResult directly) */
typedef EOS_EResult (EOS_CALL *EOS_Metrics_EndPlayerSession_t)(EOS_HMetrics Handle, const EOS_Metrics_EndPlayerSessionOptions* Options);

#ifdef __cplusplus
}
#endif
