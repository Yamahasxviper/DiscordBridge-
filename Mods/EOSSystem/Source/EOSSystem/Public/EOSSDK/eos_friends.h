// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_friends.h — delegates to the real EOS SDK eos_friends.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C3431/C5257 conflicts when EOSShared.h (included
// via EOSDirectSDK) tries to redeclare EOS_EFriendsStatus as a scoped enum
// after our unscoped definition was processed first.

#pragma once

// Delegate to the real EOS SDK's eos_friends.h via the EOSSDK module include path.
#include <eos_friends.h>

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

