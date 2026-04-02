// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_presence.h — delegates to the real EOS SDK eos_presence.h, then adds
// _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_presence.h>)
#  include <eos_presence.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Presence interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Presence_QueryPresence_t)(EOS_HPresence Handle, const EOS_Presence_QueryPresenceOptions* Options, void* ClientData, EOS_Presence_OnQueryPresenceCompleteCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_Presence_CreatePresenceModification_t)(EOS_HPresence Handle, const EOS_Presence_CreatePresenceModificationOptions* Options, EOS_HPresenceModification* OutPresenceModificationHandle);
typedef void        (EOS_CALL *EOS_Presence_SetPresence_t)(EOS_HPresence Handle, const EOS_Presence_SetPresenceOptions* Options, void* ClientData, EOS_Presence_SetPresenceCompleteCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetStatus_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetStatusOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetRawRichText_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetRawRichTextOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetData_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetDataOptions* Options);
typedef void        (EOS_CALL *EOS_PresenceModification_Release_t)(EOS_HPresenceModification PresenceModificationHandle);
