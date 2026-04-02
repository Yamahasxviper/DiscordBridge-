// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_auth.h — delegates to the real EOS SDK eos_auth.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_auth.h>)
#  include <eos_auth.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Auth interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void         (EOS_CALL *EOS_Auth_Login_t)(EOS_HAuth Handle, const EOS_Auth_LoginOptions* Options, void* ClientData, EOS_Auth_OnLoginCallback CompletionDelegate);
typedef void         (EOS_CALL *EOS_Auth_Logout_t)(EOS_HAuth Handle, const EOS_Auth_LogoutOptions* Options, void* ClientData, EOS_Auth_OnLogoutCallback CompletionDelegate);
typedef EOS_ELoginStatus (EOS_CALL *EOS_Auth_GetLoginStatus_t)(EOS_HAuth Handle, EOS_EpicAccountId LocalUserId);
typedef int32_t      (EOS_CALL *EOS_Auth_GetLoggedInAccountsCount_t)(EOS_HAuth Handle);
typedef EOS_EpicAccountId (EOS_CALL *EOS_Auth_GetLoggedInAccountByIndex_t)(EOS_HAuth Handle, int32_t Index);
typedef EOS_EResult  (EOS_CALL *EOS_Auth_CopyUserAuthToken_t)(EOS_HAuth Handle, const EOS_Auth_CopyUserAuthTokenOptions* Options, EOS_EpicAccountId LocalUserId, EOS_Auth_Token** OutUserAuthToken);
typedef void         (EOS_CALL *EOS_Auth_Token_Release_t)(EOS_Auth_Token* AuthToken);
