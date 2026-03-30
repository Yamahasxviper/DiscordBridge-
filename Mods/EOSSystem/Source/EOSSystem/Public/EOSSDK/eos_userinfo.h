// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_userinfo.h — delegates to the real EOS SDK eos_userinfo.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_userinfo.h>)
#  include <eos_userinfo.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  UserInfo interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void         (EOS_CALL *EOS_UserInfo_QueryUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate);
typedef void         (EOS_CALL *EOS_UserInfo_QueryUserInfoByDisplayName_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoByDisplayNameOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback CompletionDelegate);
typedef EOS_EResult  (EOS_CALL *EOS_UserInfo_CopyUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_CopyUserInfoOptions* Options, EOS_UserInfo** OutUserInfo);
typedef void         (EOS_CALL *EOS_UserInfo_Release_t)(EOS_UserInfo* UserInfo);
