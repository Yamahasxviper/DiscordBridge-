// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sessions.h — delegates to the real EOS SDK eos_sessions.h, with fallback
// constants for the CSS UE5.3.2 engine, then adds _t function-pointer typedefs
// required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_sessions.h>)
#  include <eos_sessions.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Sessions interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef EOS_EResult (EOS_CALL *EOS_Sessions_CreateSessionModification_t)(EOS_HSessions Handle, const EOS_Sessions_CreateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle);
typedef EOS_EResult (EOS_CALL *EOS_Sessions_UpdateSessionModification_t)(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle);
typedef void        (EOS_CALL *EOS_Sessions_UpdateSession_t)(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionOptions* Options, void* ClientData, EOS_Sessions_OnUpdateSessionCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_DestroySession_t)(EOS_HSessions Handle, const EOS_Sessions_DestroySessionOptions* Options, void* ClientData, EOS_Sessions_OnDestroySessionCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_JoinSession_t)(EOS_HSessions Handle, const EOS_Sessions_JoinSessionOptions* Options, void* ClientData, EOS_Sessions_OnJoinSessionCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_StartSession_t)(EOS_HSessions Handle, const EOS_Sessions_StartSessionOptions* Options, void* ClientData, EOS_Sessions_OnStartSessionCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_EndSession_t)(EOS_HSessions Handle, const EOS_Sessions_EndSessionOptions* Options, void* ClientData, EOS_Sessions_OnEndSessionCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_RegisterPlayers_t)(EOS_HSessions Handle, const EOS_Sessions_RegisterPlayersOptions* Options, void* ClientData, EOS_Sessions_OnRegisterPlayersCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Sessions_UnregisterPlayers_t)(EOS_HSessions Handle, const EOS_Sessions_UnregisterPlayersOptions* Options, void* ClientData, EOS_Sessions_OnUnregisterPlayersCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetBucketId_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetBucketIdOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetHostAddress_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetHostAddressOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetMaxPlayers_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetMaxPlayersOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetJoinInProgressAllowed_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetJoinInProgressAllowedOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetPermissionLevel_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetPermissionLevelOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_AddAttribute_t)(EOS_HSessionModification Handle, const EOS_SessionModification_AddAttributeOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_RemoveAttribute_t)(EOS_HSessionModification Handle, const EOS_SessionModification_RemoveAttributeOptions* Options);
typedef void        (EOS_CALL *EOS_SessionModification_Release_t)(EOS_HSessionModification SessionModificationHandle);
