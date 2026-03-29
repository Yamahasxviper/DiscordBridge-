// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_common.h — common EOS SDK types, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.
//
// INCLUDE GUARD NOTE
// ──────────────────
// EOS_COMMON_H matches the named include guard used by the CSS engine's
// ThirdParty EOSSDK eos_common.h.  Whichever header (this one or the engine's)
// is processed first in a translation unit sets the guard and prevents the
// other from re-defining the same types — eliminating C2011 'enum type
// redefinition' and C2371 'redefinition; different basic types' errors that
// occur when BanSystem compiles with both EOSSystem and EOSDirectSDK headers.

#pragma once

#ifndef EOS_COMMON_H
#define EOS_COMMON_H

#include "eos_base.h"
#include <stdint.h>  // uint64_t for EOS_NotificationId

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Account ID string lengths (including null terminator)
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_EPICACCOUNTID_MAX_LENGTH  33
#define EOS_PRODUCTUSERID_MAX_LENGTH  33

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EResult  (moved here from eos_base.h — this is where the real EOS SDK
//  defines it, and where the engine's eos_common.h also defines it)
//
//  Defined as typedef int32_t for standalone EOSSystem builds.  When compiled
//  in a BanSystem context where the engine's eos_common.h sets EOS_COMMON_H
//  first, the engine's own definition (enum EOS_EResult) is used instead and
//  this block is skipped — so no C2371 conflict occurs.
// ─────────────────────────────────────────────────────────────────────────────
typedef int32_t EOS_EResult;

// Success / generic
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

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_NotificationId  (moved here from eos_base.h)
// ─────────────────────────────────────────────────────────────────────────────
typedef uint64_t EOS_NotificationId;
#define EOS_INVALID_NOTIFICATIONID ((EOS_NotificationId)0)

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EExternalAccountType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EExternalAccountType
{
    EOS_EAT_EPIC       = 0,
    EOS_EAT_STEAM      = 1,
    EOS_EAT_PSN        = 2,
    EOS_EAT_XBL        = 3,
    EOS_EAT_DISCORD    = 4,
    EOS_EAT_GOG        = 5,
    EOS_EAT_NINTENDO   = 6,
    EOS_EAT_OCULUS     = 7,
    EOS_EAT_ITCHIO     = 8,
    EOS_EAT_AMAZON     = 9
} EOS_EExternalAccountType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELoginStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELoginStatus
{
    EOS_LS_NotLoggedIn        = 0,
    EOS_LS_UsingLocalProfile  = 1,
    EOS_LS_LoggedIn           = 2
} EOS_ELoginStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EOnlineStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EOnlineStatus
{
    EOS_OS_Offline        = 0,
    EOS_OS_DoNotDisturb   = 1,
    EOS_OS_Away           = 2,
    EOS_OS_Online         = 3
} EOS_EOnlineStatus;

#ifdef __cplusplus
}
#endif

#endif // EOS_COMMON_H
