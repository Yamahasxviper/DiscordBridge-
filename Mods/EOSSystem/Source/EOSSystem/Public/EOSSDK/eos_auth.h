// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_auth.h — EOS SDK Auth interface, written from scratch using only
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
//  Forward declare the Auth handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_AuthHandle;
typedef struct EOS_AuthHandle* EOS_HAuth;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAuthScopeFlags  (bitmask)
// ─────────────────────────────────────────────────────────────────────────────
typedef int32_t EOS_EAuthScopeFlags;
#define EOS_AS_NoFlags        0x0000
#define EOS_AS_BasicProfile   0x0001
#define EOS_AS_FriendsAll     0x0002
#define EOS_AS_Presence       0x0004
#define EOS_AS_FriendsList    0x0008
#define EOS_AS_Email          0x0010

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAuthTokenType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAuthTokenType
{
    EOS_ATT_Client = 0,
    EOS_ATT_User   = 1
} EOS_EAuthTokenType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELoginCredentialType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELoginCredentialType
{
    EOS_LCT_Password         = 0,
    EOS_LCT_ExchangeCode     = 1,
    EOS_LCT_PersistentAuth   = 2,
    EOS_LCT_DeviceCode       = 3,
    EOS_LCT_Developer        = 4,
    EOS_LCT_RefreshToken     = 5,
    EOS_LCT_AccountPortal    = 6,
    EOS_LCT_ExternalAuth     = 7
} EOS_ELoginCredentialType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_Token
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_AUTH_TOKEN_API_LATEST 1

typedef struct EOS_Auth_Token
{
    /** API version: must be EOS_AUTH_TOKEN_API_LATEST */
    int32_t              ApiVersion;
    /** Name of the product/application this token was created for */
    const char*          App;
    /** OAuth2 client ID */
    const char*          ClientId;
    /** Epic Account ID of the authenticated user */
    EOS_EpicAccountId    AccountId;
    /** Opaque access token string */
    const char*          AccessToken;
    /** Seconds until the access token expires */
    double               ExpiresIn;
    /** ISO 8601 UTC timestamp string when the access token expires */
    const char*          ExpiresAt;
    /** Whether this is a client or user token */
    EOS_EAuthTokenType   AuthType;
    /** Opaque refresh token string */
    const char*          RefreshToken;
    /** Seconds until the refresh token expires */
    double               RefreshExpiresIn;
    /** ISO 8601 UTC timestamp string when the refresh token expires */
    const char*          RefreshExpiresAt;
} EOS_Auth_Token;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_Credentials
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_AUTH_CREDENTIALS_API_LATEST 4

typedef struct EOS_Auth_Credentials
{
    /** API version: must be EOS_AUTH_CREDENTIALS_API_LATEST */
    int32_t                    ApiVersion;
    /** ID portion of the credentials (meaning depends on Type) */
    const char*                Id;
    /** Token/secret portion of the credentials (meaning depends on Type) */
    const char*                Token;
    /** Login credential type */
    EOS_ELoginCredentialType   Type;
    /** Platform-specific authentication options (may be NULL) */
    void*                      SystemAuthCredentialsOptions;
    /** Requested OAuth2 scope flags */
    EOS_EAuthScopeFlags        ScopeFlags;
} EOS_Auth_Credentials;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_LoginOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_AUTH_LOGIN_API_LATEST 2

typedef struct EOS_Auth_LoginOptions
{
    /** API version: must be EOS_AUTH_LOGIN_API_LATEST */
    int32_t                      ApiVersion;
    /** Credentials to use for login */
    const EOS_Auth_Credentials*  Credentials;
    /** Requested OAuth2 scope flags */
    EOS_EAuthScopeFlags          ScopeFlags;
} EOS_Auth_LoginOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_LoginCallbackInfo
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Auth_LoginCallbackInfo
{
    /** Result code for the login operation */
    EOS_EResult          ResultCode;
    /** Context that was passed to the originating request */
    void*                ClientData;
    /** Epic Account ID of the local user who attempted to log in */
    EOS_EpicAccountId    LocalUserId;
    /** Continuance token for multi-step authentication flows (may be NULL) */
    EOS_ContinuanceToken ContinuanceToken;
    /** PIN grant code for device-code flow (may be NULL) */
    EOS_EpicAccountId    PinGrantCode;
} EOS_Auth_LoginCallbackInfo;

typedef void (EOS_CALL *EOS_Auth_OnLoginCallback)(const EOS_Auth_LoginCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_LogoutOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_AUTH_LOGOUT_API_LATEST 1

typedef struct EOS_Auth_LogoutOptions
{
    /** API version: must be EOS_AUTH_LOGOUT_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the user to log out */
    EOS_EpicAccountId LocalUserId;
} EOS_Auth_LogoutOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Auth_LogoutCallbackInfo
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Auth_LogoutCallbackInfo
{
    /** Result code for the logout operation */
    EOS_EResult       ResultCode;
    /** Context that was passed to the originating request */
    void*             ClientData;
    /** Epic Account ID of the local user who logged out */
    EOS_EpicAccountId LocalUserId;
} EOS_Auth_LogoutCallbackInfo;

typedef void (EOS_CALL *EOS_Auth_OnLogoutCallback)(const EOS_Auth_LogoutCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  Auth interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Initiates a login flow */
typedef void        (EOS_CALL *EOS_Auth_Login_t)(EOS_HAuth Handle, const EOS_Auth_LoginOptions* Options, void* ClientData, EOS_Auth_OnLoginCallback CompletionDelegate);

/** Logs out an authenticated user */
typedef void        (EOS_CALL *EOS_Auth_Logout_t)(EOS_HAuth Handle, const EOS_Auth_LogoutOptions* Options, void* ClientData, EOS_Auth_OnLogoutCallback CompletionDelegate);

/** Returns the login status for a given Epic Account ID */
typedef EOS_ELoginStatus (EOS_CALL *EOS_Auth_GetLoginStatus_t)(EOS_HAuth Handle, EOS_EpicAccountId LocalUserId);

/** Returns the number of accounts currently logged in */
typedef int32_t     (EOS_CALL *EOS_Auth_GetLoggedInAccountsCount_t)(EOS_HAuth Handle);

/** Returns the Epic Account ID at the given index */
typedef EOS_EpicAccountId (EOS_CALL *EOS_Auth_GetLoggedInAccountByIndex_t)(EOS_HAuth Handle, int32_t Index);

/** Copies the user auth token into an allocated EOS_Auth_Token; caller must release with EOS_Auth_Token_Release */
typedef EOS_EResult (EOS_CALL *EOS_Auth_CopyUserAuthToken_t)(EOS_HAuth Handle, EOS_EpicAccountId LocalUserId, EOS_Auth_Token** OutUserAuthToken);

/** Releases memory for an EOS_Auth_Token retrieved via EOS_Auth_CopyUserAuthToken */
typedef void        (EOS_CALL *EOS_Auth_Token_Release_t)(EOS_Auth_Token* AuthToken);

#ifdef __cplusplus
}
#endif
