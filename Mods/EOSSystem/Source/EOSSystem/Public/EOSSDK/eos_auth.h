// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_auth.h — delegates to the real EOS SDK eos_auth.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C3431/C5257 conflicts when EOSShared.h (included
// via EOSDirectSDK) tries to redeclare EOS_EAuthTokenType / EOS_ELoginCredentialType
// as scoped enums after our unscoped definitions were processed first.

#pragma once

// Delegate to the real EOS SDK's eos_auth.h via the EOSSDK module include path.
#include <eos_auth.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Auth interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_auth.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void           (EOS_CALL *EOS_Auth_Login_t)(EOS_HAuth Handle, const EOS_Auth_LoginOptions* Options, void* ClientData, EOS_Auth_OnLoginCallback CompletionDelegate);
typedef void           (EOS_CALL *EOS_Auth_Logout_t)(EOS_HAuth Handle, const EOS_Auth_LogoutOptions* Options, void* ClientData, EOS_Auth_OnLogoutCallback CompletionDelegate);
typedef EOS_ELoginStatus (EOS_CALL *EOS_Auth_GetLoginStatus_t)(EOS_HAuth Handle, EOS_EpicAccountId LocalUserId);
typedef int32_t        (EOS_CALL *EOS_Auth_GetLoggedInAccountsCount_t)(EOS_HAuth Handle);
typedef EOS_EpicAccountId (EOS_CALL *EOS_Auth_GetLoggedInAccountByIndex_t)(EOS_HAuth Handle, int32_t Index);
typedef EOS_EResult    (EOS_CALL *EOS_Auth_CopyUserAuthToken_t)(EOS_HAuth Handle, const EOS_Auth_CopyUserAuthTokenOptions* Options, EOS_EpicAccountId LocalUserId, EOS_Auth_Token** OutUserAuthToken);
typedef void           (EOS_CALL *EOS_Auth_Token_Release_t)(EOS_Auth_Token* AuthToken);

