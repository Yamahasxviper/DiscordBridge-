// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_base.h  — fundamental EOS C-SDK types written from scratch.
//
// These declarations are derived solely from the publicly available EOS SDK
// documentation (https://dev.epicgames.com/docs).  No UE EOSSDK module,
// no EOSShared, and no CSS FactoryGame headers are referenced anywhere in
// this file or the EOSSystem mod.

#pragma once

#include <stdint.h>  // int32_t, uint64_t, etc.

// ─────────────────────────────────────────────────────────────────────────────
//  Calling convention
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_CALL
  #if defined(_WIN32)
    #define EOS_CALL __cdecl
  #else
    #define EOS_CALL
  #endif
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Boolean
// ─────────────────────────────────────────────────────────────────────────────
typedef int32_t EOS_Bool;
#define EOS_TRUE  1
#define EOS_FALSE 0

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque handle types  (all are pointer-sized; actual structs are internal)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PlatformHandleDetails;
struct EOS_EpicAccountIdDetails;
struct EOS_ProductUserIdDetails;
struct EOS_ConnectHandleDetails;
struct EOS_SessionsHandleDetails;
struct EOS_SessionModificationHandleDetails;
struct EOS_SessionHandleDetails;
struct EOS_SessionSearchHandleDetails;
struct EOS_AntiCheatServerHandleDetails;
struct EOS_SanctionsHandleDetails;
struct EOS_MetricsHandleDetails;
struct EOS_ReportsHandleDetails;
struct EOS_ContinuanceTokenDetails;
struct EOS_PlayerSanctionDetails;

typedef EOS_PlatformHandleDetails*           EOS_HPlatform;
typedef EOS_EpicAccountIdDetails*            EOS_EpicAccountId;
typedef EOS_ProductUserIdDetails*            EOS_ProductUserId;
typedef EOS_ConnectHandleDetails*            EOS_HConnect;
typedef EOS_SessionsHandleDetails*           EOS_HSessions;
typedef EOS_SessionModificationHandleDetails* EOS_HSessionModification;
typedef EOS_SessionHandleDetails*            EOS_HSessionDetails;
typedef EOS_SessionSearchHandleDetails*      EOS_HSessionSearch;
typedef EOS_AntiCheatServerHandleDetails*    EOS_HAntiCheatServer;
typedef EOS_SanctionsHandleDetails*          EOS_HSanctions;
typedef EOS_MetricsHandleDetails*            EOS_HMetrics;
typedef EOS_ReportsHandleDetails*            EOS_HReports;
typedef EOS_ContinuanceTokenDetails*         EOS_ContinuanceToken;
typedef EOS_PlayerSanctionDetails*           EOS_HPlayerSanction;

// ─────────────────────────────────────────────────────────────────────────────
//  Notification ID
// ─────────────────────────────────────────────────────────────────────────────
typedef uint64_t EOS_NotificationId;
#define EOS_INVALID_NOTIFICATIONID ((EOS_NotificationId)0)

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EResult  (integer result codes — values from public EOS SDK docs)
// ─────────────────────────────────────────────────────────────────────────────
typedef int32_t EOS_EResult;

// Success
#define EOS_Success                                  ((EOS_EResult)  0)
#define EOS_NoChange                                 ((EOS_EResult)  1)
#define EOS_VersionMismatch                          ((EOS_EResult)  2)
#define EOS_LimitExceeded                            ((EOS_EResult)  3)
#define EOS_Duplicate                                ((EOS_EResult)  4)
#define EOS_MissingParameters                        ((EOS_EResult)  5)
#define EOS_InvalidParameters                        ((EOS_EResult)  6)
#define EOS_InvalidUser                              ((EOS_EResult)  7)
#define EOS_InvalidAuth                              ((EOS_EResult)  8)
#define EOS_AccessDenied                             ((EOS_EResult)  9)
#define EOS_MissingInterface                         ((EOS_EResult) 10)
#define EOS_TimedOut                                 ((EOS_EResult) 11)
#define EOS_Pending                                  ((EOS_EResult) 12)
#define EOS_TooManyRequests                          ((EOS_EResult) 13)
#define EOS_AlreadyPending                           ((EOS_EResult) 14)
#define EOS_InvalidSandboxId                         ((EOS_EResult) 15)
#define EOS_NotFound                                 ((EOS_EResult) 16)
#define EOS_Disabled                                 ((EOS_EResult) 17)
#define EOS_DuplicateNotAllowed                      ((EOS_EResult) 18)
#define EOS_NotImplemented                           ((EOS_EResult) 19)
#define EOS_Canceled                                 ((EOS_EResult) 20)
#define EOS_Unknown                                  ((EOS_EResult) 21)
#define EOS_OperationWillRetry                       ((EOS_EResult) 22)
#define EOS_NoChange2                                ((EOS_EResult) 23)
#define EOS_YieldTaskCompleted                       ((EOS_EResult) 24)

// Auth
#define EOS_Auth_AccountLocked                       ((EOS_EResult)1001)
#define EOS_Auth_AccountLockedForUpdate              ((EOS_EResult)1002)
#define EOS_Auth_InvalidRefreshToken                 ((EOS_EResult)1003)
#define EOS_Auth_InvalidToken                        ((EOS_EResult)1004)
#define EOS_Auth_AuthenticationFailure               ((EOS_EResult)1005)
#define EOS_Auth_InvalidPlatformToken                ((EOS_EResult)1006)
#define EOS_Auth_WrongAccount                        ((EOS_EResult)1007)
#define EOS_Auth_WrongClient                         ((EOS_EResult)1008)
#define EOS_Auth_FullAccountRequired                 ((EOS_EResult)1009)
#define EOS_Auth_HeadlessAccountRequired             ((EOS_EResult)1010)
#define EOS_Auth_PasswordResetRequired               ((EOS_EResult)1011)
#define EOS_Auth_PasswordCannotBeReused              ((EOS_EResult)1012)
#define EOS_Auth_Expired                             ((EOS_EResult)1013)
#define EOS_Auth_ScopeConsentRequired                ((EOS_EResult)1014)
#define EOS_Auth_ApplicationNotFound                 ((EOS_EResult)1015)
#define EOS_Auth_ScopeNotFound                       ((EOS_EResult)1016)
#define EOS_Auth_AccountFeatureRestricted            ((EOS_EResult)1017)

// Connect
#define EOS_Connect_ExternalTokenValidationFailed    ((EOS_EResult)2001)
#define EOS_Connect_UserAlreadyExists                ((EOS_EResult)2002)
#define EOS_Connect_AuthExpired                      ((EOS_EResult)2003)
#define EOS_Connect_InvalidToken                     ((EOS_EResult)2004)
#define EOS_Connect_UnsupportedTokenType             ((EOS_EResult)2005)
#define EOS_Connect_LinkAccountFailed                ((EOS_EResult)2006)
#define EOS_Connect_ExternalServiceUnavailable       ((EOS_EResult)2007)
#define EOS_Connect_ExternalServiceForbidden         ((EOS_EResult)2008)

// Sessions
#define EOS_Sessions_SessionInProgress               ((EOS_EResult)3001)
#define EOS_Sessions_TooManyPlayers                  ((EOS_EResult)3002)
#define EOS_Sessions_NoPermission                    ((EOS_EResult)3003)
#define EOS_Sessions_SessionAlreadyExists            ((EOS_EResult)3004)
#define EOS_Sessions_InvalidLock                     ((EOS_EResult)3005)
#define EOS_Sessions_InvalidSession                  ((EOS_EResult)3006)
#define EOS_Sessions_SandboxNotAllowed               ((EOS_EResult)3007)
#define EOS_Sessions_InviteFailed                    ((EOS_EResult)3008)
#define EOS_Sessions_InviteNotFound                  ((EOS_EResult)3009)
#define EOS_Sessions_UpsertNotAllowed                ((EOS_EResult)3010)
#define EOS_Sessions_AggregationFailed               ((EOS_EResult)3011)
#define EOS_Sessions_HostAtCapacity                  ((EOS_EResult)3012)
#define EOS_Sessions_SandboxAtCapacity               ((EOS_EResult)3013)
#define EOS_Sessions_SessionNotAnonymous             ((EOS_EResult)3014)
#define EOS_Sessions_OutOfSync                       ((EOS_EResult)3015)
#define EOS_Sessions_TooManyInvites                  ((EOS_EResult)3016)
#define EOS_Sessions_PresenceSessionExists           ((EOS_EResult)3017)
#define EOS_Sessions_DeploymentAtCapacity            ((EOS_EResult)3018)
#define EOS_Sessions_NotAllowed                      ((EOS_EResult)3019)

// AntiCheat
#define EOS_AntiCheat_ClientProtectionNotAvailable   ((EOS_EResult)6001)
#define EOS_AntiCheat_InvalidMode                    ((EOS_EResult)6002)
#define EOS_AntiCheat_ClientProductIdMismatch        ((EOS_EResult)6003)
#define EOS_AntiCheat_ClientSandboxIdMismatch        ((EOS_EResult)6004)
#define EOS_AntiCheat_ProtectMessageSessionKeyRequired ((EOS_EResult)6005)
#define EOS_AntiCheat_ProtectMessageValidationFailed ((EOS_EResult)6006)
#define EOS_AntiCheat_ProtectMessageInitializationFailed ((EOS_EResult)6007)
#define EOS_AntiCheat_PeerAlreadyRegistered          ((EOS_EResult)6008)
#define EOS_AntiCheat_PeerNotFound                   ((EOS_EResult)6009)
#define EOS_AntiCheat_PeerNotProtected               ((EOS_EResult)6010)
#define EOS_AntiCheat_ClientDeploymentIdMismatch     ((EOS_EResult)6011)
#define EOS_AntiCheat_DeviceIdAuthIsNotSupported     ((EOS_EResult)6012)
