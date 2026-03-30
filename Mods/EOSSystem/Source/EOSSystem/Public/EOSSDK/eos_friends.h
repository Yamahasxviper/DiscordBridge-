// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_friends.h — delegates to the real EOS SDK eos_friends.h, with fallback
// definitions for the CSS UE5.3.2 engine which may ship a minimal EOSSDK that
// omits EOS_EFriendsStatus enum value constants (EOS_FS_NotFriends etc.).
//
// See eos_common.h for the rationale behind the #ifndef-per-value fallback
// pattern (handles cases where the real SDK defines types-only, without values).
// Delegating here prevents C3431/C5257 conflicts when EOSShared.h (included
// via EOSDirectSDK) tries to redeclare EOS_EFriendsStatus as a scoped enum
// after our unscoped definition was processed first.

#pragma once

#if defined(__has_include) && __has_include(<eos_friends.h>)
#  include <eos_friends.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EFriendsStatus value constants (matching EOS SDK 1.15.x).
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_FS_NotFriends
#  define EOS_FS_NotFriends        0
#  define EOS_FS_InviteSent        1
#  define EOS_FS_InviteReceived    2
#  define EOS_FS_Friends           3
#endif // EOS_FS_NotFriends

// ─────────────────────────────────────────────────────────────────────────────
//  Friends interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_friends.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void               (EOS_CALL *EOS_Friends_QueryFriends_t)(EOS_HFriends Handle, const EOS_Friends_QueryFriendsOptions* Options, void* ClientData, EOS_Friends_OnQueryFriendsCallback CompletionDelegate);
typedef int32_t            (EOS_CALL *EOS_Friends_GetFriendsCount_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendsCountOptions* Options);
typedef EOS_EpicAccountId  (EOS_CALL *EOS_Friends_GetFriendAtIndex_t)(EOS_HFriends Handle, const EOS_Friends_GetFriendAtIndexOptions* Options);
typedef EOS_EFriendsStatus (EOS_CALL *EOS_Friends_GetStatus_t)(EOS_HFriends Handle, const EOS_Friends_GetStatusOptions* Options);
typedef void               (EOS_CALL *EOS_Friends_SendInvite_t)(EOS_HFriends Handle, const EOS_Friends_SendInviteOptions* Options, void* ClientData, EOS_Friends_OnSendInviteCallback CompletionDelegate);
typedef void               (EOS_CALL *EOS_Friends_AcceptInvite_t)(EOS_HFriends Handle, const EOS_Friends_AcceptInviteOptions* Options, void* ClientData, EOS_Friends_OnAcceptInviteCallback CompletionDelegate);
typedef void               (EOS_CALL *EOS_Friends_RejectInvite_t)(EOS_HFriends Handle, const EOS_Friends_RejectInviteOptions* Options, void* ClientData, EOS_Friends_OnRejectInviteCallback CompletionDelegate);
typedef EOS_NotificationId (EOS_CALL *EOS_Friends_AddNotifyFriendsUpdate_t)(EOS_HFriends Handle, const EOS_Friends_AddNotifyFriendsUpdateOptions* Options, void* ClientData, EOS_Friends_OnFriendsUpdateCallback NotificationFn);
typedef void               (EOS_CALL *EOS_Friends_RemoveNotifyFriendsUpdate_t)(EOS_HFriends Handle, EOS_NotificationId InId);

