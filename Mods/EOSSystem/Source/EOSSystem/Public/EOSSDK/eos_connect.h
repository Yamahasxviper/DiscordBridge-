// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_connect.h — EOS SDK Connect interface, written from scratch using only
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
//  EOS_EExternalCredentialType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EExternalCredentialType
{
    EOS_ECT_EPIC                  = 0,
    EOS_ECT_STEAM_APP_TICKET      = 1,
    EOS_ECT_PSN_ID_TOKEN          = 2,
    EOS_ECT_XBL_XSTS_TOKEN        = 3,
    EOS_ECT_DISCORD_ACCESS_TOKEN  = 4,
    EOS_ECT_GOG_SESSION_TICKET    = 5,
    EOS_ECT_NINTENDO_ID_TOKEN     = 6,
    EOS_ECT_NINTENDO_NSA_ID_TOKEN = 7,
    EOS_ECT_UPLAY_ACCESS_TOKEN    = 8,
    EOS_ECT_OPENID_ACCESS_TOKEN   = 9,
    EOS_ECT_DEVICEID_ACCESS_TOKEN = 10,
    EOS_ECT_APPLE_ID_TOKEN        = 11,
    EOS_ECT_GOOGLE_ID_TOKEN       = 12,
    EOS_ECT_OCULUS_USERID_NONCE   = 13,
    EOS_ECT_ITCHIO_JWT            = 14,
    EOS_ECT_ITCHIO_KEY            = 15,
    EOS_ECT_EPIC_ID_TOKEN         = 16,
    EOS_ECT_AMAZON_ACCESS_TOKEN   = 17
} EOS_EExternalCredentialType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_Credentials
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_CREDENTIALS_API_LATEST 1

typedef struct EOS_Connect_Credentials
{
    /** API version: must be EOS_CONNECT_CREDENTIALS_API_LATEST */
    int32_t                    ApiVersion;
    /** External service access token or EOS auth token string */
    const char*                Token;
    /** Type of external credential / token */
    EOS_EExternalCredentialType Type;
} EOS_Connect_Credentials;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_UserLoginInfo
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_USERLOGININFO_API_LATEST 2

typedef struct EOS_Connect_UserLoginInfo
{
    /** API version: must be EOS_CONNECT_USERLOGININFO_API_LATEST */
    int32_t      ApiVersion;
    /** UTF-8 display name for the user (used for Device ID flow) */
    const char*  DisplayName;
} EOS_Connect_UserLoginInfo;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_LoginOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_LOGIN_API_LATEST 1

typedef struct EOS_Connect_LoginOptions
{
    /** API version: must be EOS_CONNECT_LOGIN_API_LATEST */
    int32_t                          ApiVersion;
    /** Credentials to authenticate with */
    const EOS_Connect_Credentials*   Credentials;
    /** Additional display-name info (required for device-ID flow) */
    const EOS_Connect_UserLoginInfo* UserLoginInfo;
} EOS_Connect_LoginOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_LoginCallbackInfo
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Connect_LoginCallbackInfo
{
    /** Result code for the login operation */
    EOS_EResult          ResultCode;
    /** Context that was passed to the originating request */
    void*                ClientData;
    /** Product User ID of the authenticated user (may be NULL on failure) */
    EOS_ProductUserId    LocalUserId;
    /** Continuance token for new-user creation flow (may be NULL) */
    EOS_ContinuanceToken ContinuanceToken;
} EOS_Connect_LoginCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnLoginCallback)(const EOS_Connect_LoginCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_CreateUserOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_CREATEUSER_API_LATEST 1

typedef struct EOS_Connect_CreateUserOptions
{
    /** API version: must be EOS_CONNECT_CREATEUSER_API_LATEST */
    int32_t              ApiVersion;
    /** Continuance token received from a failed login attempt */
    EOS_ContinuanceToken ContinuanceToken;
} EOS_Connect_CreateUserOptions;

typedef struct EOS_Connect_CreateUserCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
} EOS_Connect_CreateUserCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnCreateUserCallback)(const EOS_Connect_CreateUserCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_CreateDeviceIdOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_CREATEDEVICEID_API_LATEST 1

typedef struct EOS_Connect_CreateDeviceIdOptions
{
    /** API version: must be EOS_CONNECT_CREATEDEVICEID_API_LATEST */
    int32_t     ApiVersion;
    /** Human-readable device model string (e.g., "PC", "PlayStation 5") */
    const char* DeviceModel;
} EOS_Connect_CreateDeviceIdOptions;

typedef struct EOS_Connect_CreateDeviceIdCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
} EOS_Connect_CreateDeviceIdCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnCreateDeviceIdCallback)(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_DeleteDeviceIdOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_DELETEDEVICEID_API_LATEST 1

typedef struct EOS_Connect_DeleteDeviceIdOptions
{
    /** API version: must be EOS_CONNECT_DELETEDEVICEID_API_LATEST */
    int32_t ApiVersion;
} EOS_Connect_DeleteDeviceIdOptions;

typedef struct EOS_Connect_DeleteDeviceIdCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
} EOS_Connect_DeleteDeviceIdCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnDeleteDeviceIdCallback)(const EOS_Connect_DeleteDeviceIdCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_QueryExternalAccountMappingsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST 1

typedef struct EOS_Connect_QueryExternalAccountMappingsOptions
{
    /** API version: must be EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST */
    int32_t                   ApiVersion;
    /** Local Product User ID of the querying user */
    EOS_ProductUserId         LocalUserId;
    /** The external account type to query */
    EOS_EExternalAccountType  AccountIdType;
    /** Array of external account ID strings to resolve */
    const char**              ExternalAccountIds;
    /** Number of entries in ExternalAccountIds */
    uint32_t                  ExternalAccountIdCount;
} EOS_Connect_QueryExternalAccountMappingsOptions;

typedef struct EOS_Connect_QueryExternalAccountMappingsCallbackInfo
{
    EOS_EResult              ResultCode;
    void*                    ClientData;
    EOS_ProductUserId        LocalUserId;
    EOS_EExternalAccountType AccountIdType;
    const char**             ExternalAccountIds;
    uint32_t                 ExternalAccountIdCount;
} EOS_Connect_QueryExternalAccountMappingsCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnQueryExternalAccountMappingsCallback)(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_GetExternalAccountMappingsOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST 1

typedef struct EOS_Connect_GetExternalAccountMappingsOptions
{
    /** API version: must be EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST */
    int32_t                  ApiVersion;
    /** Local Product User ID of the querying user */
    EOS_ProductUserId        LocalUserId;
    /** External account type to look up */
    EOS_EExternalAccountType AccountIdType;
    /** The specific external account ID string to resolve */
    const char*              TargetExternalUserId;
} EOS_Connect_GetExternalAccountMappingsOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_AuthExpirationCallbackInfo / AddNotifyAuthExpiration
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Connect_AuthExpirationCallbackInfo
{
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
} EOS_Connect_AuthExpirationCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnAuthExpirationCallback)(const EOS_Connect_AuthExpirationCallbackInfo* Data);

#define EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST 1

typedef struct EOS_Connect_AddNotifyAuthExpirationOptions
{
    /** API version: must be EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST */
    int32_t ApiVersion;
} EOS_Connect_AddNotifyAuthExpirationOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Connect_LoginStatusChangedCallbackInfo / AddNotifyLoginStatusChanged
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Connect_LoginStatusChangedCallbackInfo
{
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    EOS_ELoginStatus  PreviousStatus;
    EOS_ELoginStatus  CurrentStatus;
} EOS_Connect_LoginStatusChangedCallbackInfo;

typedef void (EOS_CALL *EOS_Connect_OnLoginStatusChangedCallback)(const EOS_Connect_LoginStatusChangedCallbackInfo* Data);

#define EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST 1

typedef struct EOS_Connect_AddNotifyLoginStatusChangedOptions
{
    /** API version: must be EOS_CONNECT_ADDNOTIFYLOGINSTATUSCHANGED_API_LATEST */
    int32_t ApiVersion;
} EOS_Connect_AddNotifyLoginStatusChangedOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Connect interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Authenticates a user via an external service credential */
typedef void              (EOS_CALL *EOS_Connect_Login_t)(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, EOS_Connect_OnLoginCallback CompletionDelegate);

/** Creates a new EOS user account linked to the current continuance token */
typedef void              (EOS_CALL *EOS_Connect_CreateUser_t)(EOS_HConnect Handle, const EOS_Connect_CreateUserOptions* Options, void* ClientData, EOS_Connect_OnCreateUserCallback CompletionDelegate);

/** Creates a persistent device-ID credential */
typedef void              (EOS_CALL *EOS_Connect_CreateDeviceId_t)(EOS_HConnect Handle, const EOS_Connect_CreateDeviceIdOptions* Options, void* ClientData, EOS_Connect_OnCreateDeviceIdCallback CompletionDelegate);

/** Deletes the device-ID credential for the current device */
typedef void              (EOS_CALL *EOS_Connect_DeleteDeviceId_t)(EOS_HConnect Handle, const EOS_Connect_DeleteDeviceIdOptions* Options, void* ClientData, EOS_Connect_OnDeleteDeviceIdCallback CompletionDelegate);

/** Queries external account-ID → Product User ID mappings */
typedef void              (EOS_CALL *EOS_Connect_QueryExternalAccountMappings_t)(EOS_HConnect Handle, const EOS_Connect_QueryExternalAccountMappingsOptions* Options, void* ClientData, EOS_Connect_OnQueryExternalAccountMappingsCallback CompletionDelegate);

/** Returns the Product User ID mapped to the given external account */
typedef EOS_ProductUserId (EOS_CALL *EOS_Connect_GetExternalAccountMapping_t)(EOS_HConnect Handle, const EOS_Connect_GetExternalAccountMappingsOptions* Options);

/** Returns the current login status for the given Product User ID */
typedef EOS_ELoginStatus  (EOS_CALL *EOS_Connect_GetLoginStatus_t)(EOS_HConnect Handle, EOS_ProductUserId LocalUserId);

/** Returns the number of Product User IDs currently logged in */
typedef int32_t           (EOS_CALL *EOS_Connect_GetLoggedInUsersCount_t)(EOS_HConnect Handle);

/** Returns the Product User ID at the given index */
typedef EOS_ProductUserId (EOS_CALL *EOS_Connect_GetLoggedInUserByIndex_t)(EOS_HConnect Handle, int32_t Index);

/** Registers a callback to receive auth-expiration notifications */
typedef EOS_NotificationId (EOS_CALL *EOS_Connect_AddNotifyAuthExpiration_t)(EOS_HConnect Handle, const EOS_Connect_AddNotifyAuthExpirationOptions* Options, void* ClientData, EOS_Connect_OnAuthExpirationCallback Notification);

/** Removes an auth-expiration notification previously registered */
typedef void               (EOS_CALL *EOS_Connect_RemoveNotifyAuthExpiration_t)(EOS_HConnect Handle, EOS_NotificationId InId);

/** Registers a callback to receive login-status-changed notifications */
typedef EOS_NotificationId (EOS_CALL *EOS_Connect_AddNotifyLoginStatusChanged_t)(EOS_HConnect Handle, const EOS_Connect_AddNotifyLoginStatusChangedOptions* Options, void* ClientData, EOS_Connect_OnLoginStatusChangedCallback Notification);

/** Removes a login-status-changed notification previously registered */
typedef void               (EOS_CALL *EOS_Connect_RemoveNotifyLoginStatusChanged_t)(EOS_HConnect Handle, EOS_NotificationId InId);

#ifdef __cplusplus
}
#endif
