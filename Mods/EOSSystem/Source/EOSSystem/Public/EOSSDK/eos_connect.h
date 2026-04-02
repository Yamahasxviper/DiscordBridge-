// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_connect.h — delegates to the real EOS SDK eos_connect.h, with fallback
// definitions for the CSS UE5.3.2 engine which may ship a minimal EOSSDK that
// omits EOS_EExternalCredentialType enum value constants.
//
// See eos_common.h for the rationale behind the #ifndef-per-value fallback
// pattern (handles cases where the real SDK defines types-only, without values).
// Delegating here prevents C2011 conflicts for EOS_EExternalCredentialType
// when BanSystem includes both EOSSystem and EOSDirectSDK headers.

#pragma once

#if defined(__has_include) && __has_include(<eos_connect.h>)
#  include <eos_connect.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EExternalCredentialType value constants (matching EOS SDK 1.15.x).
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_ECT_EPIC
// Use explicit casts so these constants work with a C++ scoped enum class.
#  define EOS_ECT_EPIC                        ((EOS_EExternalCredentialType)0)
#  define EOS_ECT_STEAM_APP_TICKET            ((EOS_EExternalCredentialType)1)
#  define EOS_ECT_PSN_ID_TOKEN                ((EOS_EExternalCredentialType)2)
#  define EOS_ECT_XBL_XSTS_TOKEN              ((EOS_EExternalCredentialType)3)
#  define EOS_ECT_DISCORD_ACCESS_TOKEN        ((EOS_EExternalCredentialType)4)
#  define EOS_ECT_GOG_SESSION_TICKET          ((EOS_EExternalCredentialType)5)
#  define EOS_ECT_NINTENDO_ID_TOKEN           ((EOS_EExternalCredentialType)6)
#  define EOS_ECT_NINTENDO_NSA_ID_TOKEN       ((EOS_EExternalCredentialType)7)
#  define EOS_ECT_UPLAY_ACCESS_TOKEN          ((EOS_EExternalCredentialType)8)
#  define EOS_ECT_OPENID_ACCESS_TOKEN         ((EOS_EExternalCredentialType)9)
#  define EOS_ECT_DEVICEID_ACCESS_TOKEN       ((EOS_EExternalCredentialType)10)
#  define EOS_ECT_APPLE                       ((EOS_EExternalCredentialType)11)
#  define EOS_ECT_GOOGLE                      ((EOS_EExternalCredentialType)12)
#  define EOS_ECT_OCULUS_USERID_NONCE         ((EOS_EExternalCredentialType)13)
#  define EOS_ECT_ITCHIO_JWT                  ((EOS_EExternalCredentialType)14)
#  define EOS_ECT_ITCHIO_KEY                  ((EOS_EExternalCredentialType)15)
#  define EOS_ECT_EPIC_ID_TOKEN               ((EOS_EExternalCredentialType)16)
#  define EOS_ECT_AMAZON_ACCESS_TOKEN         ((EOS_EExternalCredentialType)17)
#endif // EOS_ECT_EPIC

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

