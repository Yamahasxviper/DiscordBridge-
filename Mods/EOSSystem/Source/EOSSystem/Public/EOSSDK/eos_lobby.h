// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_lobby.h — EOS SDK Lobby interface, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include "eos_sessions.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELobbyPermissionLevel
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELobbyPermissionLevel
{
    EOS_LPL_PUBLICADVERTISED  = 0,
    EOS_LPL_JOINVIAPRESENCE   = 1,
    EOS_LPL_INVITEONLY        = 2
} EOS_ELobbyPermissionLevel;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_CreateLobbyOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_CREATELOBBY_API_LATEST 8

typedef struct EOS_Lobby_CreateLobbyOptions
{
    /** API version: must be EOS_LOBBY_CREATELOBBY_API_LATEST */
    int32_t                  ApiVersion;
    /** Product User ID of the local user creating the lobby */
    EOS_ProductUserId        LocalUserId;
    /** Maximum number of lobby members */
    uint32_t                 MaxLobbyMembers;
    /** Visibility / join permission level */
    EOS_ELobbyPermissionLevel PermissionLevel;
    /** EOS_TRUE to link presence with this lobby */
    EOS_Bool                 bPresenceEnabled;
    /** EOS_TRUE to allow members to send invites */
    EOS_Bool                 bAllowInvites;
    /** Bucket ID string for matchmaking categorisation */
    const char*              BucketId;
    /** EOS_TRUE to disable automatic host migration on host disconnect */
    EOS_Bool                 bDisableHostMigration;
    /** EOS_TRUE to enable the RTC voice room for this lobby */
    EOS_Bool                 bEnableRTCRoom;
    /** EOS_TRUE to allow joining by lobby ID without an invite */
    EOS_Bool                 bEnableJoinById;
    /** EOS_TRUE to require an invite to rejoin after being kicked */
    EOS_Bool                 bRejoinAfterKickRequiresInvite;
} EOS_Lobby_CreateLobbyOptions;

typedef struct EOS_Lobby_CreateLobbyCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_CreateLobbyCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnCreateLobbyCallback)(const EOS_Lobby_CreateLobbyCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_DestroyLobbyOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_DESTROYLOBBY_API_LATEST 1

typedef struct EOS_Lobby_DestroyLobbyOptions
{
    /** API version: must be EOS_LOBBY_DESTROYLOBBY_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user (must be lobby owner) */
    EOS_ProductUserId LocalUserId;
    /** ID of the lobby to destroy */
    const char*       LobbyId;
} EOS_Lobby_DestroyLobbyOptions;

typedef struct EOS_Lobby_DestroyLobbyCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_DestroyLobbyCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnDestroyLobbyCallback)(const EOS_Lobby_DestroyLobbyCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_JoinLobbyOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_JOINLOBBY_API_LATEST 2

typedef struct EOS_Lobby_JoinLobbyOptions
{
    /** API version: must be EOS_LOBBY_JOINLOBBY_API_LATEST */
    int32_t           ApiVersion;
    /** Handle to lobby details describing the lobby to join */
    EOS_HLobbyDetails LobbyDetailsHandle;
    /** Product User ID of the local user joining */
    EOS_ProductUserId LocalUserId;
    /** EOS_TRUE to link presence with this lobby */
    EOS_Bool          bPresenceEnabled;
} EOS_Lobby_JoinLobbyOptions;

typedef struct EOS_Lobby_JoinLobbyCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_JoinLobbyCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnJoinLobbyCallback)(const EOS_Lobby_JoinLobbyCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_LeaveLobbyOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_LEAVELOBBY_API_LATEST 1

typedef struct EOS_Lobby_LeaveLobbyOptions
{
    /** API version: must be EOS_LOBBY_LEAVELOBBY_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user leaving */
    EOS_ProductUserId LocalUserId;
    /** ID of the lobby to leave */
    const char*       LobbyId;
} EOS_Lobby_LeaveLobbyOptions;

typedef struct EOS_Lobby_LeaveLobbyCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_LeaveLobbyCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnLeaveLobbyCallback)(const EOS_Lobby_LeaveLobbyCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_UpdateLobbyModificationOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST 1

typedef struct EOS_Lobby_UpdateLobbyModificationOptions
{
    /** API version: must be EOS_LOBBY_UPDATELOBBYMODIFICATION_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user (must be lobby owner) */
    EOS_ProductUserId LocalUserId;
    /** ID of the lobby to update */
    const char*       LobbyId;
} EOS_Lobby_UpdateLobbyModificationOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_UpdateLobbyOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_UPDATELOBBY_API_LATEST 1

typedef struct EOS_Lobby_UpdateLobbyOptions
{
    /** API version: must be EOS_LOBBY_UPDATELOBBY_API_LATEST */
    int32_t              ApiVersion;
    /** Handle to a lobby modification object */
    EOS_HLobbyModification LobbyModificationHandle;
} EOS_Lobby_UpdateLobbyOptions;

typedef struct EOS_Lobby_UpdateLobbyCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_UpdateLobbyCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnUpdateLobbyCallback)(const EOS_Lobby_UpdateLobbyCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Lobby_KickMemberOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBY_KICKMEMBER_API_LATEST 1

typedef struct EOS_Lobby_KickMemberOptions
{
    /** API version: must be EOS_LOBBY_KICKMEMBER_API_LATEST */
    int32_t           ApiVersion;
    /** ID of the lobby */
    const char*       LobbyId;
    /** Product User ID of the lobby owner performing the kick */
    EOS_ProductUserId LocalUserId;
    /** Product User ID of the member to kick */
    EOS_ProductUserId TargetUserId;
} EOS_Lobby_KickMemberOptions;

typedef struct EOS_Lobby_KickMemberCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* LobbyId;
} EOS_Lobby_KickMemberCallbackInfo;

typedef void (EOS_CALL *EOS_Lobby_OnKickMemberCallback)(const EOS_Lobby_KickMemberCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_LobbyModification_SetBucketIdOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBYMODIFICATION_SETBUCKETID_API_LATEST 1

typedef struct EOS_LobbyModification_SetBucketIdOptions
{
    /** API version: must be EOS_LOBBYMODIFICATION_SETBUCKETID_API_LATEST */
    int32_t     ApiVersion;
    /** Bucket ID string */
    const char* BucketId;
} EOS_LobbyModification_SetBucketIdOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_LobbyModification_SetMaxMembersOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBYMODIFICATION_SETMAXMEMBERS_API_LATEST 1

typedef struct EOS_LobbyModification_SetMaxMembersOptions
{
    /** API version: must be EOS_LOBBYMODIFICATION_SETMAXMEMBERS_API_LATEST */
    int32_t  ApiVersion;
    /** New maximum member count */
    uint32_t MaxMembers;
} EOS_LobbyModification_SetMaxMembersOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_LobbyModification_SetPermissionLevelOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST 1

typedef struct EOS_LobbyModification_SetPermissionLevelOptions
{
    /** API version: must be EOS_LOBBYMODIFICATION_SETPERMISSIONLEVEL_API_LATEST */
    int32_t                  ApiVersion;
    /** New permission level */
    EOS_ELobbyPermissionLevel PermissionLevel;
} EOS_LobbyModification_SetPermissionLevelOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Lobby interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Creates a new lobby */
typedef void        (EOS_CALL *EOS_Lobby_CreateLobby_t)(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, EOS_Lobby_OnCreateLobbyCallback CompletionDelegate);

/** Destroys an existing lobby */
typedef void        (EOS_CALL *EOS_Lobby_DestroyLobby_t)(EOS_HLobby Handle, const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, EOS_Lobby_OnDestroyLobbyCallback CompletionDelegate);

/** Joins a lobby via a lobby details handle */
typedef void        (EOS_CALL *EOS_Lobby_JoinLobby_t)(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, EOS_Lobby_OnJoinLobbyCallback CompletionDelegate);

/** Leaves a lobby */
typedef void        (EOS_CALL *EOS_Lobby_LeaveLoby_t)(EOS_HLobby Handle, const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, EOS_Lobby_OnLeaveLobbyCallback CompletionDelegate);

/** Creates a lobby modification handle; release with EOS_LobbyModification_Release */
typedef EOS_EResult (EOS_CALL *EOS_Lobby_UpdateLobbyModification_t)(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);

/** Commits a lobby modification to the backend */
typedef void        (EOS_CALL *EOS_Lobby_UpdateLobby_t)(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, EOS_Lobby_OnUpdateLobbyCallback CompletionDelegate);

/** Kicks a member from the lobby */
typedef void        (EOS_CALL *EOS_Lobby_KickMember_t)(EOS_HLobby Handle, const EOS_Lobby_KickMemberOptions* Options, void* ClientData, EOS_Lobby_OnKickMemberCallback CompletionDelegate);

/** Sets the bucket ID on a lobby modification */
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetBucketId_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options);

/** Sets the max member count on a lobby modification */
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetMaxMembers_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options);

/** Sets the permission level on a lobby modification */
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetPermissionLevel_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options);

/** Releases a lobby modification handle */
typedef void        (EOS_CALL *EOS_LobbyModification_Release_t)(EOS_HLobbyModification LobbyModificationHandle);

#ifdef __cplusplus
}
#endif
