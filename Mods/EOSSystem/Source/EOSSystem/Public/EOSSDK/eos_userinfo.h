// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_userinfo.h — delegates to the real EOS SDK eos_userinfo.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_userinfo.h via the EOSSDK module include path.
#include <eos_userinfo.h>

// ─────────────────────────────────────────────────────────────────────────────
//  UserInfo interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_userinfo.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_UserInfo_QueryUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_UserInfo_QueryUserInfoByDisplayName_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoByDisplayNameOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_UserInfo_CopyUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_CopyUserInfoOptions* Options, EOS_UserInfo** OutUserInfo);
typedef void        (EOS_CALL *EOS_UserInfo_Release_t)(EOS_UserInfo* UserInfo);
