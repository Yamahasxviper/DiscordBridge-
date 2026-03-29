// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_rtc.h — EOS SDK RTC (Real-Time Communication / voice) interface,
// written from scratch using only public EOS SDK documentation
// (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque RTC and RTCAdmin interface handles
//  (concrete types forward-declared; defined in eos_platform.h as well)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_RTCHandleDetails;
typedef struct EOS_RTCHandleDetails*      EOS_HRTC;

struct EOS_RTCAdminHandleDetails;
typedef struct EOS_RTCAdminHandleDetails* EOS_HRTCAdmin;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_RTC_JoinRoomOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_RTC_JOINROOM_API_LATEST 1

typedef struct EOS_RTC_JoinRoomOptions
{
    /** API version: must be EOS_RTC_JOINROOM_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user joining the room */
    EOS_ProductUserId LocalUserId;
    /** Name of the RTC room to join */
    const char*       RoomName;
    /** Base URL used to reach the RTC back-end (provided by the lobby/session) */
    const char*       ClientBaseUrl;
    /** Participant token obtained from the lobby/session RTC token API */
    const char*       ParticipantToken;
    /** EOS_TRUE if the local audio input device starts muted */
    EOS_Bool          bManualAudioInputEnabled;
    /** EOS_TRUE if the local audio output starts muted */
    EOS_Bool          bManualAudioOutputEnabled;
    /** EOS_TRUE if the local audio input device starts muted at the device level */
    EOS_Bool          bLocalAudioDeviceInputStartsMuted;
} EOS_RTC_JoinRoomOptions;

typedef struct EOS_RTC_JoinRoomCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       RoomName;
} EOS_RTC_JoinRoomCallbackInfo;

typedef void (EOS_CALL *EOS_RTC_OnJoinRoomCallback)(const EOS_RTC_JoinRoomCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_RTC_LeaveRoomOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_RTC_LEAVEROOM_API_LATEST 1

typedef struct EOS_RTC_LeaveRoomOptions
{
    /** API version: must be EOS_RTC_LEAVEROOM_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user leaving the room */
    EOS_ProductUserId LocalUserId;
    /** Name of the RTC room to leave */
    const char*       RoomName;
} EOS_RTC_LeaveRoomOptions;

typedef struct EOS_RTC_LeaveRoomCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       RoomName;
} EOS_RTC_LeaveRoomCallbackInfo;

typedef void (EOS_CALL *EOS_RTC_OnLeaveRoomCallback)(const EOS_RTC_LeaveRoomCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  RTC interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Joins an RTC voice room */
typedef void    (EOS_CALL *EOS_RTC_JoinRoom_t)(EOS_HRTC Handle, const EOS_RTC_JoinRoomOptions* Options, void* ClientData, EOS_RTC_OnJoinRoomCallback CompletionDelegate);

/** Leaves an RTC voice room */
typedef void    (EOS_CALL *EOS_RTC_LeaveRoom_t)(EOS_HRTC Handle, const EOS_RTC_LeaveRoomOptions* Options, void* ClientData, EOS_RTC_OnLeaveRoomCallback CompletionDelegate);

/** Returns the RTC interface handle from a platform instance */
typedef EOS_HRTC (EOS_CALL *EOS_Platform_GetRTCInterface_t)(EOS_HPlatform Handle);

#ifdef __cplusplus
}
#endif
