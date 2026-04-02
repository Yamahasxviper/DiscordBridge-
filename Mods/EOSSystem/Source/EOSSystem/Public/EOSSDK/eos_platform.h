// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_platform.h — custom platform interface header for the CSS engine.
//
// The CSS (CoffeeStain Satisfactory) UE5.3.2 engine ships the EOSSDK plugin
// but omits eos_platform.h from its distribution.  The engine DOES ship
// eos_types.h, which provides EOS_HPlatform, EOS_Platform_ClientCredentials,
// EOS_Platform_RTCOptions, and EOS_Platform_Options.  This file:
//
//   1. Includes <eos_types.h> to get the canonical platform struct definitions
//      (replacing our old hand-written copies that caused C2371 conflicts when
//      the engine's eos_types.h was also compiled in the same TU).
//
//   2. Provides the missing EOS_Platform_Get*Interface function declarations
//      and EOS_Platform_*_t function-pointer typedefs needed by EOSSDKLoader.
//
// INCLUDE GUARD
// ─────────────
// EOS_Platform_H matches the guard the real EOS SDK eos_platform.h would use.
// Setting it here prevents any future engine eos_platform.h from re-declaring
// the same symbols.

#pragma once

#ifndef EOS_Platform_H
#define EOS_Platform_H

// eos_types.h from the real EOSSDK provides the canonical definitions of:
//   EOS_HPlatform, EOS_Platform_ClientCredentials, EOS_Platform_RTCOptions,
//   EOS_Platform_Options, EOS_PF_* flags, and other platform structs.
// Using angle brackets finds the engine's ThirdParty EOSSDK copy rather
// than this directory (Public/EOSSDK/ is not an /I root).
#include <eos_types.h>

// eos_logging.h provides EOS_ELogCategory, EOS_ELogLevel, EOS_LogMessageFunc
// used in the function-pointer typedefs below.
#include "eos_logging.h"

// eos_common.h provides EOS_EResult, EOS_Bool, EOS_ProductUserId, etc.
#include "eos_common.h"

// eos_init.h provides EOS_InitializeOptions (needed for EOS_Initialize_t below).
#include "eos_init.h"

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PF_* platform creation flags
//  Guard prevents redefining if already defined by <eos_types.h>.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_PF_LOADING_IN_EDITOR
#define EOS_PF_LOADING_IN_EDITOR               0x00001u
#define EOS_PF_DISABLE_OVERLAY                 0x00002u
#define EOS_PF_DISABLE_SOCIAL_OVERLAY          0x00004u
#define EOS_PF_RESERVED1                       0x00008u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D9     0x00010u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_D3D10    0x00020u
#define EOS_PF_WINDOWS_ENABLE_OVERLAY_OPENGL   0x00040u
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declarations for interface handles
//  Struct names match the real EOS SDK (no 'Details' suffix for handle types).
//  Identical typedef redeclarations are valid in C++.
// ─────────────────────────────────────────────────────────────────────────────

#ifndef EOS_HPlatform_DEFINED
#define EOS_HPlatform_DEFINED
struct EOS_PlatformHandle;
typedef struct EOS_PlatformHandle* EOS_HPlatform;
#endif

struct EOS_MetricsHandle;
typedef struct EOS_MetricsHandle*             EOS_HMetrics;
struct EOS_AuthHandle;
typedef struct EOS_AuthHandle*                EOS_HAuth;
struct EOS_ConnectHandle;
typedef struct EOS_ConnectHandle*             EOS_HConnect;
struct EOS_EcomHandle;
typedef struct EOS_EcomHandle*                EOS_HEcom;
struct EOS_UIHandle;
typedef struct EOS_UIHandle*                  EOS_HUI;
struct EOS_FriendsHandle;
typedef struct EOS_FriendsHandle*             EOS_HFriends;
struct EOS_PresenceHandle;
typedef struct EOS_PresenceHandle*            EOS_HPresence;
struct EOS_SessionsHandle;
typedef struct EOS_SessionsHandle*            EOS_HSessions;
struct EOS_LobbyHandle;
typedef struct EOS_LobbyHandle*               EOS_HLobby;
struct EOS_LobbyModificationHandle;
typedef struct EOS_LobbyModificationHandle*   EOS_HLobbyModification;
struct EOS_LobbyDetailsHandle;
typedef struct EOS_LobbyDetailsHandle*        EOS_HLobbyDetails;
struct EOS_UserInfoHandle;
typedef struct EOS_UserInfoHandle*            EOS_HUserInfo;
struct EOS_P2PHandle;
typedef struct EOS_P2PHandle*                 EOS_HP2P;
struct EOS_PlayerDataStorageHandle;
typedef struct EOS_PlayerDataStorageHandle*   EOS_HPlayerDataStorage;
struct EOS_TitleStorageHandle;
typedef struct EOS_TitleStorageHandle*        EOS_HTitleStorage;
struct EOS_AchievementsHandle;
typedef struct EOS_AchievementsHandle*        EOS_HAchievements;
struct EOS_StatsHandle;
typedef struct EOS_StatsHandle*               EOS_HStats;
struct EOS_LeaderboardsHandle;
typedef struct EOS_LeaderboardsHandle*        EOS_HLeaderboards;
struct EOS_ModsHandle;
typedef struct EOS_ModsHandle*                EOS_HMods;
struct EOS_AntiCheatClientHandle;
typedef struct EOS_AntiCheatClientHandle*     EOS_HAntiCheatClient;
struct EOS_AntiCheatServerHandle;
typedef struct EOS_AntiCheatServerHandle*     EOS_HAntiCheatServer;
struct EOS_ReportsHandle;
typedef struct EOS_ReportsHandle*             EOS_HReports;
struct EOS_SanctionsHandle;
typedef struct EOS_SanctionsHandle*           EOS_HSanctions;
struct EOS_KWSHandle;
typedef struct EOS_KWSHandle*                 EOS_HKWS;
struct EOS_ProgressionSnapshotHandle;
typedef struct EOS_ProgressionSnapshotHandle* EOS_HProgressionSnapshot;
struct EOS_RTCHandle;
typedef struct EOS_RTCHandle*                 EOS_HRTC;
struct EOS_RTCAdminHandle;
typedef struct EOS_RTCAdminHandle*            EOS_HRTCAdmin;
struct EOS_CustomInvitesHandle;
typedef struct EOS_CustomInvitesHandle*       EOS_HCustomInvites;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_* function pointer typedefs
//  These are missing from the CSS engine's EOSSDK distribution (which omits
//  eos_platform.h entirely).  They are required by EOSSDKLoader.h.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

/** Creates and returns a new platform instance */
typedef EOS_HPlatform      (EOS_CALL *EOS_Platform_Create_t)(const EOS_Platform_Options* Options);

/** Initialises the EOS SDK (called before EOS_Platform_Create) */
typedef EOS_EResult        (EOS_CALL *EOS_Initialize_t)(const EOS_InitializeOptions* Options);

/** Shuts down the EOS SDK (called after all platforms are released) */
typedef EOS_EResult        (EOS_CALL *EOS_Shutdown_t)(void);

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

// ─────────────────────────────────────────────────────────────────────────────
//  Actual C-linkage function declarations
//  These mirror the real EOS C SDK ABI so that any translation unit that
//  processes this header still has the EOS_Platform_Get*Interface declarations.
// ─────────────────────────────────────────────────────────────────────────────

EOS_HMetrics     EOS_Platform_GetMetricsInterface(EOS_HPlatform Handle);
EOS_HAuth        EOS_Platform_GetAuthInterface(EOS_HPlatform Handle);
EOS_HConnect     EOS_Platform_GetConnectInterface(EOS_HPlatform Handle);
EOS_HEcom        EOS_Platform_GetEcomInterface(EOS_HPlatform Handle);
EOS_HFriends     EOS_Platform_GetFriendsInterface(EOS_HPlatform Handle);
EOS_HPresence    EOS_Platform_GetPresenceInterface(EOS_HPlatform Handle);
EOS_HSessions    EOS_Platform_GetSessionsInterface(EOS_HPlatform Handle);
EOS_HLobby       EOS_Platform_GetLobbyInterface(EOS_HPlatform Handle);
EOS_HUserInfo    EOS_Platform_GetUserInfoInterface(EOS_HPlatform Handle);
EOS_HPlayerDataStorage EOS_Platform_GetPlayerDataStorageInterface(EOS_HPlatform Handle);
EOS_HTitleStorage      EOS_Platform_GetTitleStorageInterface(EOS_HPlatform Handle);
EOS_HAchievements      EOS_Platform_GetAchievementsInterface(EOS_HPlatform Handle);
EOS_HStats       EOS_Platform_GetStatsInterface(EOS_HPlatform Handle);
EOS_HLeaderboards EOS_Platform_GetLeaderboardsInterface(EOS_HPlatform Handle);
EOS_HAntiCheatServer EOS_Platform_GetAntiCheatServerInterface(EOS_HPlatform Handle);
EOS_HAntiCheatClient EOS_Platform_GetAntiCheatClientInterface(EOS_HPlatform Handle);
EOS_HReports     EOS_Platform_GetReportsInterface(EOS_HPlatform Handle);
EOS_HSanctions   EOS_Platform_GetSanctionsInterface(EOS_HPlatform Handle);
EOS_HRTC         EOS_Platform_GetRTCInterface(EOS_HPlatform Handle);
EOS_HRTCAdmin    EOS_Platform_GetRTCAdminInterface(EOS_HPlatform Handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EOS_Platform_H
