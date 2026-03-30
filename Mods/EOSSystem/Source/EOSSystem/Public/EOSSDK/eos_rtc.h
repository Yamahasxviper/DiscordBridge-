// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_rtc.h — delegates to the real EOS SDK eos_rtc.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_rtc.h via the EOSSDK module include path.
#include <eos_rtc.h>

// ─────────────────────────────────────────────────────────────────────────────
//  RTC interface function pointer typedefs
//  These _t aliases are used for dynamic DLL symbol loading.
//  The real EOS SDK eos_rtc.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void    (EOS_CALL *EOS_RTC_JoinRoom_t)(EOS_HRTC Handle, const EOS_RTC_JoinRoomOptions* Options, void* ClientData, EOS_RTC_OnJoinRoomCallback CompletionDelegate);
typedef void    (EOS_CALL *EOS_RTC_LeaveRoom_t)(EOS_HRTC Handle, const EOS_RTC_LeaveRoomOptions* Options, void* ClientData, EOS_RTC_OnLeaveRoomCallback CompletionDelegate);
