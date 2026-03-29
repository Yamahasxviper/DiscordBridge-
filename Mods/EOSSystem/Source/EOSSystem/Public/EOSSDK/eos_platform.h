// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_platform.h — EOS SDK platform interface, written from scratch using
// only public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.
//
// INCLUDE GUARD NOTE
// ──────────────────
// Two additional guards are defined here:
//
//  EOS_Platform_H  — matches the guard in the CSS engine's EOSSDK
//    eos_platform.h (and in EOSDirectSDK's mod-local eos_platform.h stub).
//    Whichever platform header is processed first sets this guard, preventing
//    the other from re-defining EOS_Platform_Options and related structs.
//
//  EOS_TYPES_H  — matches the guard used by the CSS engine's EOSSDK
//    eos_types.h.  Setting it here pre-empts the engine's eos_types.h from
//    re-defining EOS_HPlatform, EOS_Platform_ClientCredentials, etc. that are
//    already defined in this file (via eos_base.h for the handle types, and
//    directly here for the platform-creation structs).
//
// STRUCT NAME NOTE
// ────────────────
// All forward-declared handle struct names here match the real EOS SDK (no
// 'Details' suffix), consistent with eos_base.h.  This allows identical
// typedef re-declarations from the engine's EOSSDK headers without C2371.

#pragma once

#ifndef EOS_Platform_H
#define EOS_Platform_H

// Claim the engine's eos_types.h guard so that file's duplicate definitions
// of EOS_HPlatform, EOS_Platform_ClientCredentials, etc. are suppressed when
// this header has already been processed.
#ifndef EOS_TYPES_H
#define EOS_TYPES_H
#endif

#include "eos_common.h"  // EOS_EResult, EOS_HPlatform (via eos_base.h), EOS_Bool
#include "eos_init.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Platform creation flags  (EOS_PF_*)
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PF_LOADING_IN_EDITOR               0x00001u
#define EOS_PF_DISABLE_OVERLAY                 0x00002u
#define EOS_PF_DISABLE_SOCIAL_OVERLAY          0x00004u
#define EOS_PF_RESERVED1                       0x00008u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9     0x00010u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10    0x00020u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL   0x00040u

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_ClientCredentials
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Platform_ClientCredentials
{
    /** OAuth2 client ID for your product */
    const char* ClientId;
    /** OAuth2 client secret for your product */
    const char* ClientSecret;
} EOS_Platform_ClientCredentials;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_RTCOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLATFORM_RTCOPTIONS_API_LATEST 1

typedef struct EOS_Platform_RTCOptions
{
    /** API version: must be EOS_PLATFORM_RTCOPTIONS_API_LATEST */
    int32_t      ApiVersion;
    /** Platform-specific RTC options (platform-dependent struct pointer) */
    const void*  PlatformSpecificOptions;
} EOS_Platform_RTCOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_Options
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLATFORM_OPTIONS_API_LATEST 12

typedef struct EOS_Platform_Options
{
    /** API version: must be EOS_PLATFORM_OPTIONS_API_LATEST */
    int32_t                        ApiVersion;
    /** Reserved — must be NULL */
    void*                          Reserved;
    /** Product ID from the Developer Portal */
    const char*                    ProductId;
    /** Sandbox ID from the Developer Portal */
    const char*                    SandboxId;
    /** OAuth2 client credentials */
    EOS_Platform_ClientCredentials ClientCredentials;
    /** EOS_TRUE when running a dedicated server, EOS_FALSE for a client */
    EOS_Bool                       bIsServer;
    /** Optional 64-character hex encryption key for player data */
    const char*                    EncryptionKey;
    /** Override the country code detected from the OS (NULL = auto) */
    const char*                    OverrideCountryCode;
    /** Override the locale code detected from the OS (NULL = auto) */
    const char*                    OverrideLocaleCode;
    /** Deployment ID from the Developer Portal */
    const char*                    DeploymentId;
    /** Bitfield of EOS_PF_* platform flags */
    uint64_t                       Flags;
    /** Path the SDK uses for its local disk cache */
    const char*                    CacheDirectory;
    /** Throttle for EOS_Platform_Tick in milliseconds (0 = no throttle) */
    uint32_t                       TickBudgetInMilliseconds;
    /** Optional RTC options; NULL = disable RTC */
    const EOS_Platform_RTCOptions* RTCOptions;
} EOS_Platform_Options;

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations for interface handles used in function pointer typedefs
//
//  Struct names match the real EOS SDK (no 'Details' suffix) so that if the
//  engine's EOSSDK headers also define these typedefs with the same struct
//  names, the identical re-declarations are harmless in C++.
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_AuthHandle;
typedef struct EOS_AuthHandle*               EOS_HAuth;

struct EOS_UserInfoHandle;
typedef struct EOS_UserInfoHandle*           EOS_HUserInfo;

struct EOS_FriendsHandle;
typedef struct EOS_FriendsHandle*            EOS_HFriends;

struct EOS_PresenceHandle;
typedef struct EOS_PresenceHandle*           EOS_HPresence;

struct EOS_P2PHandle;
typedef struct EOS_P2PHandle*                EOS_HP2P;

struct EOS_AchievementsHandle;
typedef struct EOS_AchievementsHandle*       EOS_HAchievements;

struct EOS_StatsHandle;
typedef struct EOS_StatsHandle*              EOS_HStats;

struct EOS_LeaderboardsHandle;
typedef struct EOS_LeaderboardsHandle*       EOS_HLeaderboards;

struct EOS_PlayerDataStorageHandle;
typedef struct EOS_PlayerDataStorageHandle*  EOS_HPlayerDataStorage;

struct EOS_TitleStorageHandle;
typedef struct EOS_TitleStorageHandle*       EOS_HTitleStorage;

struct EOS_EcomHandle;
typedef struct EOS_EcomHandle*               EOS_HEcom;

struct EOS_RTCHandle;
typedef struct EOS_RTCHandle*                EOS_HRTC;

struct EOS_RTCAdminHandle;
typedef struct EOS_RTCAdminHandle*           EOS_HRTCAdmin;

struct EOS_LobbyHandle;
typedef struct EOS_LobbyHandle*              EOS_HLobby;

struct EOS_LobbyModificationHandle;
typedef struct EOS_LobbyModificationHandle*  EOS_HLobbyModification;

struct EOS_LobbyDetailsHandle;
typedef struct EOS_LobbyDetailsHandle*       EOS_HLobbyDetails;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_* function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Creates and returns a new platform instance */
typedef EOS_HPlatform      (EOS_CALL *EOS_Platform_Create_t)(const EOS_Platform_Options* Options);

/** Releases a platform instance previously created with EOS_Platform_Create */
typedef void               (EOS_CALL *EOS_Platform_Release_t)(EOS_HPlatform Handle);

/** Tick the platform — drives callbacks and timed operations */
typedef void               (EOS_CALL *EOS_Platform_Tick_t)(EOS_HPlatform Handle);

/** Returns the Connect interface handle */
typedef EOS_HConnect       (EOS_CALL *EOS_Platform_GetConnectInterface_t)(EOS_HPlatform Handle);

/** Returns the Auth interface handle */
typedef EOS_HAuth          (EOS_CALL *EOS_Platform_GetAuthInterface_t)(EOS_HPlatform Handle);

/** Returns the UserInfo interface handle */
typedef EOS_HUserInfo      (EOS_CALL *EOS_Platform_GetUserInfoInterface_t)(EOS_HPlatform Handle);

/** Returns the Friends interface handle */
typedef EOS_HFriends       (EOS_CALL *EOS_Platform_GetFriendsInterface_t)(EOS_HPlatform Handle);

/** Returns the Presence interface handle */
typedef EOS_HPresence      (EOS_CALL *EOS_Platform_GetPresenceInterface_t)(EOS_HPlatform Handle);

/** Returns the Sessions interface handle */
typedef EOS_HSessions      (EOS_CALL *EOS_Platform_GetSessionsInterface_t)(EOS_HPlatform Handle);

/** Returns the Lobby interface handle */
typedef EOS_HLobby         (EOS_CALL *EOS_Platform_GetLobbyInterface_t)(EOS_HPlatform Handle);

/** Returns the P2P interface handle */
typedef EOS_HP2P           (EOS_CALL *EOS_Platform_GetP2PInterface_t)(EOS_HPlatform Handle);

/** Returns the Achievements interface handle */
typedef EOS_HAchievements  (EOS_CALL *EOS_Platform_GetAchievementsInterface_t)(EOS_HPlatform Handle);

/** Returns the Stats interface handle */
typedef EOS_HStats         (EOS_CALL *EOS_Platform_GetStatsInterface_t)(EOS_HPlatform Handle);

/** Returns the Leaderboards interface handle */
typedef EOS_HLeaderboards  (EOS_CALL *EOS_Platform_GetLeaderboardsInterface_t)(EOS_HPlatform Handle);

/** Returns the PlayerDataStorage interface handle */
typedef EOS_HPlayerDataStorage (EOS_CALL *EOS_Platform_GetPlayerDataStorageInterface_t)(EOS_HPlatform Handle);

/** Returns the TitleStorage interface handle */
typedef EOS_HTitleStorage  (EOS_CALL *EOS_Platform_GetTitleStorageInterface_t)(EOS_HPlatform Handle);

/** Returns the AntiCheatServer interface handle */
typedef EOS_HAntiCheatServer (EOS_CALL *EOS_Platform_GetAntiCheatServerInterface_t)(EOS_HPlatform Handle);

/** Returns the Sanctions interface handle */
typedef EOS_HSanctions     (EOS_CALL *EOS_Platform_GetSanctionsInterface_t)(EOS_HPlatform Handle);

/** Returns the Metrics interface handle */
typedef EOS_HMetrics       (EOS_CALL *EOS_Platform_GetMetricsInterface_t)(EOS_HPlatform Handle);

/** Returns the Reports interface handle */
typedef EOS_HReports       (EOS_CALL *EOS_Platform_GetReportsInterface_t)(EOS_HPlatform Handle);

/** Returns the Ecom interface handle */
typedef EOS_HEcom          (EOS_CALL *EOS_Platform_GetEcomInterface_t)(EOS_HPlatform Handle);

/** Returns the RTC interface handle */
typedef EOS_HRTC           (EOS_CALL *EOS_Platform_GetRTCInterface_t)(EOS_HPlatform Handle);

/** Returns the RTCAdmin interface handle */
typedef EOS_HRTCAdmin      (EOS_CALL *EOS_Platform_GetRTCAdminInterface_t)(EOS_HPlatform Handle);

/** Sets the log level for the specified category */
typedef EOS_EResult        (EOS_CALL *EOS_Platform_SetLogLevel_t)(EOS_ELogCategory LogCategory, EOS_ELogLevel LogLevel);

/** Registers a callback for log messages */
typedef EOS_EResult        (EOS_CALL *EOS_Logging_SetCallback_t)(EOS_LogMessageFunc Callback);

/** Returns a human-readable string for an EOS_EResult code */
typedef const char*        (EOS_CALL *EOS_EResult_ToString_t)(EOS_EResult Result);

/** Converts a EOS_ProductUserId to a string */
typedef EOS_EResult        (EOS_CALL *EOS_ProductUserId_ToString_t)(EOS_ProductUserId AccountId, char* OutBuffer, int32_t* InOutBufferLength);

/** Creates an EOS_ProductUserId from a string */
typedef EOS_ProductUserId  (EOS_CALL *EOS_ProductUserId_FromString_t)(const char* AccountIdString);

/** Returns EOS_TRUE if the EOS_ProductUserId is valid */
typedef EOS_Bool           (EOS_CALL *EOS_ProductUserId_IsValid_t)(EOS_ProductUserId AccountId);

/** Converts an EOS_EpicAccountId to a string */
typedef EOS_EResult        (EOS_CALL *EOS_EpicAccountId_ToString_t)(EOS_EpicAccountId AccountId, char* OutBuffer, int32_t* InOutBufferLength);

/** Returns EOS_TRUE if the EOS_EpicAccountId is valid */
typedef EOS_Bool           (EOS_CALL *EOS_EpicAccountId_IsValid_t)(EOS_EpicAccountId AccountId);

/** Creates an EOS_EpicAccountId from a string */
typedef EOS_EpicAccountId  (EOS_CALL *EOS_EpicAccountId_FromString_t)(const char* AccountIdString);

#ifdef __cplusplus
}
#endif

#endif // EOS_Platform_H
