// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sessions.h — delegates to the real EOS SDK eos_sessions.h, with fallback
// definitions for the CSS UE5.3.2 engine which may ship a minimal EOSSDK that
// omits EOS_EAttributeType and EOS_ESessionAttributeAdvertisementType constants.
//
// See eos_common.h for the rationale behind the #ifndef-per-value fallback
// pattern (handles cases where the real SDK defines types-only, without values).
// Delegating here prevents C2011 conflicts for EOS_EAttributeType and
// EOS_EOnlineComparisonOp (which the CSS engine places in eos_common.h)
// when BanSystem includes both EOSSystem and EOSDirectSDK headers.

#pragma once

#if defined(__has_include) && __has_include(<eos_sessions.h>)
#  include <eos_sessions.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EAttributeType value constants (matching EOS SDK 1.15.x).
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_AT_BOOLEAN
#  define EOS_AT_BOOLEAN    0
#  define EOS_AT_INT64      1
#  define EOS_AT_DOUBLE     2
#  define EOS_AT_STRING     3
#endif // EOS_AT_BOOLEAN

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_ESessionAttributeAdvertisementType value constants.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_SAAT_DontAdvertise
#  define EOS_SAAT_DontAdvertise  0
#  define EOS_SAAT_Advertise      1
#endif // EOS_SAAT_DontAdvertise

// ─────────────────────────────────────────────────────────────────────────────
//  Sessions interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_sessions.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
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

