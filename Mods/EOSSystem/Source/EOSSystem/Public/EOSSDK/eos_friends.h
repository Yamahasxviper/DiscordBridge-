// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_friends.h — EOS SDK Friends interface, written from scratch using only
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
//  Forward declare the Friends handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_FriendsHandle;
typedef struct EOS_FriendsHandle* EOS_HFriends;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EFriendsStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EFriendsStatus
{
    EOS_FS_NotFriends      = 0,
    EOS_FS_InviteSent      = 1,
    EOS_FS_InviteReceived  = 2,
    EOS_FS_Friends         = 3
} EOS_EFriendsStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_QueryFriendsOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_QUERYFRIENDS_API_LATEST 1

typedef struct EOS_Friends_QueryFriendsOptions
{
    /** API version: must be EOS_FRIENDS_QUERYFRIENDS_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user whose friends list to query */
    EOS_EpicAccountId LocalUserId;
} EOS_Friends_QueryFriendsOptions;

typedef struct EOS_Friends_QueryFriendsCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
} EOS_Friends_QueryFriendsCallbackInfo;

typedef void (EOS_CALL *EOS_Friends_OnQueryFriendsCallback)(const EOS_Friends_QueryFriendsCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_GetFriendsCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST 1

typedef struct EOS_Friends_GetFriendsCountOptions
{
    /** API version: must be EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
} EOS_Friends_GetFriendsCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_GetFriendAtIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST 1

typedef struct EOS_Friends_GetFriendAtIndexOptions
{
    /** API version: must be EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Index of the friend to retrieve */
    int32_t           Index;
} EOS_Friends_GetFriendAtIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_GetStatusOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_GETSTATUS_API_LATEST 1

typedef struct EOS_Friends_GetStatusOptions
{
    /** API version: must be EOS_FRIENDS_GETSTATUS_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the target user */
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_GetStatusOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_SendInviteOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_SENDINVITE_API_LATEST 1

typedef struct EOS_Friends_SendInviteOptions
{
    /** API version: must be EOS_FRIENDS_SENDINVITE_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the user sending the invite */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the user to invite */
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_SendInviteOptions;

typedef struct EOS_Friends_SendInviteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_SendInviteCallbackInfo;

typedef void (EOS_CALL *EOS_Friends_OnSendInviteCallback)(const EOS_Friends_SendInviteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_AcceptInviteOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_ACCEPTINVITE_API_LATEST 1

typedef struct EOS_Friends_AcceptInviteOptions
{
    /** API version: must be EOS_FRIENDS_ACCEPTINVITE_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user accepting the invite */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the user whose invite to accept */
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_AcceptInviteOptions;

typedef struct EOS_Friends_AcceptInviteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_AcceptInviteCallbackInfo;

typedef void (EOS_CALL *EOS_Friends_OnAcceptInviteCallback)(const EOS_Friends_AcceptInviteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_RejectInviteOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_FRIENDS_REJECTINVITE_API_LATEST 1

typedef struct EOS_Friends_RejectInviteOptions
{
    /** API version: must be EOS_FRIENDS_REJECTINVITE_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user rejecting the invite */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the user whose invite to reject */
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_RejectInviteOptions;

typedef struct EOS_Friends_RejectInviteCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
} EOS_Friends_RejectInviteCallbackInfo;

typedef void (EOS_CALL *EOS_Friends_OnRejectInviteCallback)(const EOS_Friends_RejectInviteCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Friends_FriendsUpdateInfo / AddNotifyFriendsUpdate
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_Friends_FriendsUpdateInfo
{
    /** Context that was passed to AddNotifyFriendsUpdate */
    void*              ClientData;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId  LocalUserId;
    /** Epic Account ID of the user whose friendship status changed */
    EOS_EpicAccountId  TargetUserId;
    /** The previous friendship status */
    EOS_EFriendsStatus PreviousStatus;
    /** The new friendship status */
    EOS_EFriendsStatus CurrentStatus;
} EOS_Friends_FriendsUpdateInfo;

typedef void (EOS_CALL *EOS_Friends_OnFriendsUpdateCallback)(const EOS_Friends_FriendsUpdateInfo* Data);

#define EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST 1

typedef struct EOS_Friends_AddNotifyFriendsUpdateOptions
{
    /** API version: must be EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST */
    int32_t ApiVersion;
} EOS_Friends_AddNotifyFriendsUpdateOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  Friends interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Fetches the friends list for the local user */
typedef void               (EOS_CALL *EOS_Friends_QueryFriends_t)(EOS_HFriends Handle, const EOS_Friends_QueryFriendsOptions* Options, void* ClientData, EOS_Friends_OnQueryFriendsCallback CompletionDelegate);

/** Returns the number of friends in the local cache */
typedef int32_t            (EOS_CALL *EOS_Friends_GetFriendsCount_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendsCountOptions* Options);

/** Returns the Epic Account ID at the given index */
typedef EOS_EpicAccountId  (EOS_CALL *EOS_Friends_GetFriendAtIndex_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendAtIndexOptions* Options);

/** Returns the friendship status between two users */
typedef EOS_EFriendsStatus (EOS_CALL *EOS_Friends_GetStatus_t)(EOS_HFriends Handle, const EOS_Friends_GetStatusOptions* Options);

/** Sends a friend invite */
typedef void               (EOS_CALL *EOS_Friends_SendInvite_t)(EOS_HFriends Handle, const EOS_Friends_SendInviteOptions* Options, void* ClientData, EOS_Friends_OnSendInviteCallback CompletionDelegate);

/** Accepts a friend invite */
typedef void               (EOS_CALL *EOS_Friends_AcceptInvite_t)(EOS_HFriends Handle, const EOS_Friends_AcceptInviteOptions* Options, void* ClientData, EOS_Friends_OnAcceptInviteCallback CompletionDelegate);

/** Rejects a friend invite */
typedef void               (EOS_CALL *EOS_Friends_RejectInvite_t)(EOS_HFriends Handle, const EOS_Friends_RejectInviteOptions* Options, void* ClientData, EOS_Friends_OnRejectInviteCallback CompletionDelegate);

/** Registers a callback for friends-status change notifications */
typedef EOS_NotificationId (EOS_CALL *EOS_Friends_AddNotifyFriendsUpdate_t)(EOS_HFriends Handle, const EOS_Friends_AddNotifyFriendsUpdateOptions* Options, void* ClientData, EOS_Friends_OnFriendsUpdateCallback CompletionDelegate);

/** Removes a friends-update notification previously registered */
typedef void               (EOS_CALL *EOS_Friends_RemoveNotifyFriendsUpdate_t)(EOS_HFriends Handle, EOS_NotificationId InId);

#ifdef __cplusplus
}
#endif
