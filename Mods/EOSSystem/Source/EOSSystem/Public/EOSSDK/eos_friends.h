// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_friends.h — delegates to the real EOS SDK eos_friends.h, with fallback
// definitions for the CSS UE5.3.2 engine, then adds _t function-pointer
// typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_friends.h>)
#  include <eos_friends.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EFriendsStatus value constants (matching EOS SDK 1.15.x).
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_FS_NotFriends
#  define EOS_FS_NotFriends           ((EOS_EFriendsStatus)0)
#  define EOS_FS_InviteSent           ((EOS_EFriendsStatus)1)
#  define EOS_FS_InviteReceived       ((EOS_EFriendsStatus)2)
#  define EOS_FS_Friends              ((EOS_EFriendsStatus)3)
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Friends interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void              (EOS_CALL *EOS_Friends_QueryFriends_t)(EOS_HFriends Handle, const EOS_Friends_QueryFriendsOptions* Options, void* ClientData, EOS_Friends_OnQueryFriendsCallback CompletionDelegate);
typedef int32_t           (EOS_CALL *EOS_Friends_GetFriendsCount_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendsCountOptions* Options);
typedef EOS_EpicAccountId (EOS_CALL *EOS_Friends_GetFriendAtIndex_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendAtIndexOptions* Options);
typedef EOS_EFriendsStatus (EOS_CALL *EOS_Friends_GetStatus_t)(EOS_HFriends Handle, const EOS_Friends_GetStatusOptions* Options);
typedef void              (EOS_CALL *EOS_Friends_SendInvite_t)(EOS_HFriends Handle, const EOS_Friends_SendInviteOptions* Options, void* ClientData, EOS_Friends_OnSendInviteCallback CompletionDelegate);
typedef void              (EOS_CALL *EOS_Friends_AcceptInvite_t)(EOS_HFriends Handle, const EOS_Friends_AcceptInviteOptions* Options, void* ClientData, EOS_Friends_OnAcceptInviteCallback CompletionDelegate);
typedef void              (EOS_CALL *EOS_Friends_RejectInvite_t)(EOS_HFriends Handle, const EOS_Friends_RejectInviteOptions* Options, void* ClientData, EOS_Friends_OnRejectInviteCallback CompletionDelegate);
typedef EOS_NotificationId (EOS_CALL *EOS_Friends_AddNotifyFriendsUpdate_t)(EOS_HFriends Handle, const EOS_Friends_AddNotifyFriendsUpdateOptions* Options, void* ClientData, EOS_Friends_OnFriendsUpdateCallback FriendsUpdateHandler);
typedef void              (EOS_CALL *EOS_Friends_RemoveNotifyFriendsUpdate_t)(EOS_HFriends Handle, EOS_NotificationId NotificationId);
