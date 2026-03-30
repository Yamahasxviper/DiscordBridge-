// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_connect.h — delegates to the real EOS SDK eos_connect.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C2011 conflicts for EOS_EExternalCredentialType
// when BanSystem includes both EOSSystem and EOSDirectSDK headers.

#pragma once

// Delegate to the real EOS SDK's eos_connect.h via the EOSSDK module include path.
#include <eos_connect.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Connect interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_connect.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void             (EOS_CALL *EOS_Connect_Login_t)(EOS_HConnect Handle, const EOS_Connect_LoginOptions* Options, void* ClientData, EOS_Connect_OnLoginCallback CompletionDelegate);
typedef void             (EOS_CALL *EOS_Connect_CreateUser_t)(EOS_HConnect Handle, const EOS_Connect_CreateUserOptions* Options, void* ClientData, EOS_Connect_OnCreateUserCallback CompletionDelegate);
typedef void             (EOS_CALL *EOS_Connect_CreateDeviceId_t)(EOS_HConnect Handle, const EOS_Connect_CreateDeviceIdOptions* Options, void* ClientData, EOS_Connect_OnCreateDeviceIdCallback CompletionDelegate);
typedef void             (EOS_CALL *EOS_Connect_DeleteDeviceId_t)(EOS_HConnect Handle, const EOS_Connect_DeleteDeviceIdOptions* Options, void* ClientData, EOS_Connect_OnDeleteDeviceIdCallback CompletionDelegate);
typedef void             (EOS_CALL *EOS_Connect_QueryExternalAccountMappings_t)(EOS_HConnect Handle, const EOS_Connect_QueryExternalAccountMappingsOptions* Options, void* ClientData, EOS_Connect_OnQueryExternalAccountMappingsCallback CompletionDelegate);
typedef EOS_ProductUserId (EOS_CALL *EOS_Connect_GetExternalAccountMapping_t)(EOS_HConnect Handle, const EOS_Connect_GetExternalAccountMappingsOptions* Options);
typedef EOS_ELoginStatus (EOS_CALL *EOS_Connect_GetLoginStatus_t)(EOS_HConnect Handle, EOS_ProductUserId LocalUserId);
typedef int32_t          (EOS_CALL *EOS_Connect_GetLoggedInUsersCount_t)(EOS_HConnect Handle);
typedef EOS_ProductUserId (EOS_CALL *EOS_Connect_GetLoggedInUserByIndex_t)(EOS_HConnect Handle, int32_t Index);
typedef EOS_NotificationId (EOS_CALL *EOS_Connect_AddNotifyAuthExpiration_t)(EOS_HConnect Handle, const EOS_Connect_AddNotifyAuthExpirationOptions* Options, void* ClientData, EOS_Connect_OnAuthExpirationCallback Notification);
typedef void             (EOS_CALL *EOS_Connect_RemoveNotifyAuthExpiration_t)(EOS_HConnect Handle, EOS_NotificationId InId);
typedef EOS_NotificationId (EOS_CALL *EOS_Connect_AddNotifyLoginStatusChanged_t)(EOS_HConnect Handle, const EOS_Connect_AddNotifyLoginStatusChangedOptions* Options, void* ClientData, EOS_Connect_OnLoginStatusChangedCallback Notification);
typedef void             (EOS_CALL *EOS_Connect_RemoveNotifyLoginStatusChanged_t)(EOS_HConnect Handle, EOS_NotificationId InId);
typedef void             (EOS_CALL *EOS_Connect_QueryProductUserIdMappings_t)(EOS_HConnect Handle, const EOS_Connect_QueryProductUserIdMappingsOptions* Options, void* ClientData, EOS_Connect_OnQueryProductUserIdMappingsCallback CompletionDelegate);
typedef uint32_t         (EOS_CALL *EOS_Connect_GetProductUserExternalAccountCount_t)(EOS_HConnect Handle, const EOS_Connect_GetProductUserExternalAccountCountOptions* Options);
typedef EOS_EResult      (EOS_CALL *EOS_Connect_CopyProductUserExternalAccountByIndex_t)(EOS_HConnect Handle, const EOS_Connect_CopyProductUserExternalAccountByIndexOptions* Options, EOS_Connect_ExternalAccountInfo** OutExternalAccountInfo);
typedef void             (EOS_CALL *EOS_Connect_ExternalAccountInfo_Release_t)(EOS_Connect_ExternalAccountInfo* ExternalAccountInfo);

