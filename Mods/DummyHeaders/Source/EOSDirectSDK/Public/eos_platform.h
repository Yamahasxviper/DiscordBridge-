// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_platform.h  (module: EOSDirectSDK, plugin: DummyHeaders)
//
// ─────────────────────────────────────────────────────────────────────────────
// STUB REPLACEMENT FOR THE MISSING EOS C SDK eos_platform.h
// ─────────────────────────────────────────────────────────────────────────────
//
// The CSS (CoffeeStain Satisfactory) UE5.3.2 engine ships the EOSSDK plugin
// but omits eos_platform.h from its distribution.  This file is a complete,
// compatible stub that provides every opaque handle typedef and every
// EOS_Platform_Get*Interface declaration that a Satisfactory server mod can
// realistically call, without initialising a second EOS platform.
//
// HOW IT WORKS
// ─────────────
// Unreal Build Tool searches a module's own Public/ directory before the
// transitive dependency paths (e.g. EOSSDK).  Because this file lives in
// EOSDirectSDK/Public/, it is always found when code does:
//
//   #include "eos_platform.h"
//
// If a future CSS engine build does ship eos_platform.h inside the EOSSDK
// plugin, the include guard below (EOS_Platform_H — matching the real SDK
// guard name) prevents any redeclaration conflicts.
//
// CALLING CONVENTION
// ───────────────────
// All EOS C SDK functions use EOS_CALL, defined in eos_base.h as __cdecl on
// Windows.  On Windows x64 __cdecl is the default calling convention, so no
// explicit annotation is required for compatibility in C++ code.  We keep
// the extern "C" linkage specifiers to match the real header exactly.
//
// USAGE
// ──────
// This header is included automatically by EOSDirectSDK.h.  Consumers that
// list "EOSDirectSDK" in their Build.cs do not need to include it directly.

#pragma once

// The real EOS SDK eos_platform.h uses EOS_Platform_H as its include guard.
// We reuse the same guard name so that if the real file is somehow also on the
// include path it cannot produce duplicate-declaration errors.
#ifndef EOS_Platform_H
#define EOS_Platform_H

// eos_common.h must be included first: it defines EOS_HPlatform, EOS_Bool,
// EOS_ProductUserId, and all other primitive EOS types used below.
#include "eos_common.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque interface handle typedefs
//  (matching the real EOS SDK 1.x declarations in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────

/** EOS_Metrics interface handle. */
struct EOS_MetricsHandle;
typedef struct EOS_MetricsHandle* EOS_HMetrics;

/** EOS_Auth interface handle. */
struct EOS_AuthHandle;
typedef struct EOS_AuthHandle* EOS_HAuth;

/** EOS_Connect interface handle — EOS identity / Product User IDs. */
struct EOS_ConnectHandle;
typedef struct EOS_ConnectHandle* EOS_HConnect;

/** EOS_Ecom interface handle — in-game purchases / entitlements. */
struct EOS_EcomHandle;
typedef struct EOS_EcomHandle* EOS_HEcom;

/** EOS_UI interface handle — Epic overlay. */
struct EOS_UIHandle;
typedef struct EOS_UIHandle* EOS_HUI;

/** EOS_Friends interface handle. */
struct EOS_FriendsHandle;
typedef struct EOS_FriendsHandle* EOS_HFriends;

/** EOS_Presence interface handle. */
struct EOS_PresenceHandle;
typedef struct EOS_PresenceHandle* EOS_HPresence;

/** EOS_Sessions interface handle. */
struct EOS_SessionsHandle;
typedef struct EOS_SessionsHandle* EOS_HSessions;

/** EOS_Lobby interface handle. */
struct EOS_LobbyHandle;
typedef struct EOS_LobbyHandle* EOS_HLobby;

/** EOS_UserInfo interface handle — public profile data. */
struct EOS_UserInfoHandle;
typedef struct EOS_UserInfoHandle* EOS_HUserInfo;

/** EOS_P2P interface handle — peer-to-peer networking. */
struct EOS_P2PHandle;
typedef struct EOS_P2PHandle* EOS_HP2P;

/** EOS_PlayerDataStorage interface handle. */
struct EOS_PlayerDataStorageHandle;
typedef struct EOS_PlayerDataStorageHandle* EOS_HPlayerDataStorage;

/** EOS_TitleStorage interface handle. */
struct EOS_TitleStorageHandle;
typedef struct EOS_TitleStorageHandle* EOS_HTitleStorage;

/** EOS_Achievements interface handle. */
struct EOS_AchievementsHandle;
typedef struct EOS_AchievementsHandle* EOS_HAchievements;

/** EOS_Stats interface handle. */
struct EOS_StatsHandle;
typedef struct EOS_StatsHandle* EOS_HStats;

/** EOS_Leaderboards interface handle. */
struct EOS_LeaderboardsHandle;
typedef struct EOS_LeaderboardsHandle* EOS_HLeaderboards;

/** EOS_Mods interface handle. */
struct EOS_ModsHandle;
typedef struct EOS_ModsHandle* EOS_HMods;

/** EOS_AntiCheatClient interface handle. */
struct EOS_AntiCheatClientHandle;
typedef struct EOS_AntiCheatClientHandle* EOS_HAntiCheatClient;

/** EOS_AntiCheatServer interface handle. */
struct EOS_AntiCheatServerHandle;
typedef struct EOS_AntiCheatServerHandle* EOS_HAntiCheatServer;

/** EOS_Reports interface handle — player behaviour reports. */
struct EOS_ReportsHandle;
typedef struct EOS_ReportsHandle* EOS_HReports;

/** EOS_Sanctions interface handle — game and network sanctions / bans. */
struct EOS_SanctionsHandle;
typedef struct EOS_SanctionsHandle* EOS_HSanctions;

/** EOS_KWS interface handle — Kids Web Services. */
struct EOS_KWSHandle;
typedef struct EOS_KWSHandle* EOS_HKWS;

/** EOS_ProgressionSnapshot interface handle. */
struct EOS_ProgressionSnapshotHandle;
typedef struct EOS_ProgressionSnapshotHandle* EOS_HProgressionSnapshot;

/** EOS_RTC interface handle — Real-Time Communication (voice). */
struct EOS_RTCHandle;
typedef struct EOS_RTCHandle* EOS_HRTC;

/** EOS_RTCAdmin interface handle. */
struct EOS_RTCAdminHandle;
typedef struct EOS_RTCAdminHandle* EOS_HRTCAdmin;

/** EOS_CustomInvites interface handle. */
struct EOS_CustomInvitesHandle;
typedef struct EOS_CustomInvitesHandle* EOS_HCustomInvites;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_HPlatform — the main platform handle
//  In the real EOS C SDK this typedef is declared here in eos_platform.h, not
//  in eos_common.h.  Define it here so that every EOS_Platform_Get*Interface
//  function declaration below compiles correctly even when eos_common.h does
//  not forward-declare it (as is the case in the CSS UE5.3.2 engine build).
// ─────────────────────────────────────────────────────────────────────────────

#ifndef EOS_HPlatform_DEFINED
#define EOS_HPlatform_DEFINED
/** Opaque handle representing the main EOS platform instance. */
struct EOS_PlatformHandle;
typedef struct EOS_PlatformHandle* EOS_HPlatform;
#endif // EOS_HPlatform_DEFINED

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Platform_Get*Interface — interface accessor declarations
//  All functions are C-linkage, matching the real EOS C SDK ABI exactly.
// ─────────────────────────────────────────────────────────────────────────────

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Acquire the Metrics interface from the given platform handle.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HMetrics, or nullptr on failure.
 */
EOS_HMetrics EOS_Platform_GetMetricsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Auth interface (Epic Account Services authentication).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HAuth, or nullptr on failure.
 */
EOS_HAuth EOS_Platform_GetAuthInterface(EOS_HPlatform Handle);

/**
 * Acquire the Connect interface (EOS Game Services identity / Product User IDs).
 *
 * This is the primary interface used by BanSystem for PUID-based identity
 * operations: EOS_Connect_QueryExternalAccountMappings, etc.
 *
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HConnect, or nullptr on failure.
 */
EOS_HConnect EOS_Platform_GetConnectInterface(EOS_HPlatform Handle);

/**
 * Acquire the Ecom interface (in-game purchasing and entitlements).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HEcom, or nullptr on failure.
 */
EOS_HEcom EOS_Platform_GetEcomInterface(EOS_HPlatform Handle);

/**
 * Acquire the UI interface (Epic overlay).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HUI, or nullptr on failure.
 */
EOS_HUI EOS_Platform_GetUIInterface(EOS_HPlatform Handle);

/**
 * Acquire the Friends interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HFriends, or nullptr on failure.
 */
EOS_HFriends EOS_Platform_GetFriendsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Presence interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HPresence, or nullptr on failure.
 */
EOS_HPresence EOS_Platform_GetPresenceInterface(EOS_HPlatform Handle);

/**
 * Acquire the Sessions interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HSessions, or nullptr on failure.
 */
EOS_HSessions EOS_Platform_GetSessionsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Lobby interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HLobby, or nullptr on failure.
 */
EOS_HLobby EOS_Platform_GetLobbyInterface(EOS_HPlatform Handle);

/**
 * Acquire the UserInfo interface (public player profile data).
 *
 * Useful for retrieving display names and other profile information by PUID.
 *
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HUserInfo, or nullptr on failure.
 */
EOS_HUserInfo EOS_Platform_GetUserInfoInterface(EOS_HPlatform Handle);

/**
 * Acquire the P2P interface (peer-to-peer networking via EOS relays).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HP2P, or nullptr on failure.
 */
EOS_HP2P EOS_Platform_GetP2PInterface(EOS_HPlatform Handle);

/**
 * Acquire the PlayerDataStorage interface (cloud save slots per user).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HPlayerDataStorage, or nullptr on failure.
 */
EOS_HPlayerDataStorage EOS_Platform_GetPlayerDataStorageInterface(EOS_HPlatform Handle);

/**
 * Acquire the TitleStorage interface (read-only game data in EOS cloud).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HTitleStorage, or nullptr on failure.
 */
EOS_HTitleStorage EOS_Platform_GetTitleStorageInterface(EOS_HPlatform Handle);

/**
 * Acquire the Achievements interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HAchievements, or nullptr on failure.
 */
EOS_HAchievements EOS_Platform_GetAchievementsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Stats interface (player statistics).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HStats, or nullptr on failure.
 */
EOS_HStats EOS_Platform_GetStatsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Leaderboards interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HLeaderboards, or nullptr on failure.
 */
EOS_HLeaderboards EOS_Platform_GetLeaderboardsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Mods interface (DLC / mod enumeration).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HMods, or nullptr on failure.
 */
EOS_HMods EOS_Platform_GetModsInterface(EOS_HPlatform Handle);

/**
 * Acquire the AntiCheatClient interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HAntiCheatClient, or nullptr on failure.
 */
EOS_HAntiCheatClient EOS_Platform_GetAntiCheatClientInterface(EOS_HPlatform Handle);

/**
 * Acquire the AntiCheatServer interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HAntiCheatServer, or nullptr on failure.
 */
EOS_HAntiCheatServer EOS_Platform_GetAntiCheatServerInterface(EOS_HPlatform Handle);

/**
 * Acquire the Reports interface (player behaviour reports).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HReports, or nullptr on failure.
 */
EOS_HReports EOS_Platform_GetReportsInterface(EOS_HPlatform Handle);

/**
 * Acquire the Sanctions interface (game/network ban management).
 *
 * BanSystem can use this interface for EOS-native ban queries:
 *   EOS_Sanctions_QueryActivePlayerSanctions(Sanctions, &Options, Callback)
 *
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HSanctions, or nullptr on failure.
 */
EOS_HSanctions EOS_Platform_GetSanctionsInterface(EOS_HPlatform Handle);

/**
 * Acquire the KWS interface (Kids Web Services — parental consent).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HKWS, or nullptr on failure.
 */
EOS_HKWS EOS_Platform_GetKWSInterface(EOS_HPlatform Handle);

/**
 * Acquire the ProgressionSnapshot interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HProgressionSnapshot, or nullptr on failure.
 */
EOS_HProgressionSnapshot EOS_Platform_GetProgressionSnapshotInterface(EOS_HPlatform Handle);

/**
 * Acquire the RTC (Real-Time Communication / voice) interface.
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HRTC, or nullptr on failure.
 */
EOS_HRTC EOS_Platform_GetRTCInterface(EOS_HPlatform Handle);

/**
 * Acquire the RTCAdmin interface (server-side voice channel management).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HRTCAdmin, or nullptr on failure.
 */
EOS_HRTCAdmin EOS_Platform_GetRTCAdminInterface(EOS_HPlatform Handle);

/**
 * Acquire the CustomInvites interface (custom game invite flow).
 * @param Handle  EOS_HPlatform obtained from IEOSSDKManager (engine-owned).
 * @return EOS_HCustomInvites, or nullptr on failure.
 */
EOS_HCustomInvites EOS_Platform_GetCustomInvitesInterface(EOS_HPlatform Handle);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // EOS_Platform_H
