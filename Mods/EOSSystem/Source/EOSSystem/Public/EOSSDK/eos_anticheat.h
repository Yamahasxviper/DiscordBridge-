// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_anticheat.h — delegates to the real EOS SDK eos_anticheatserver.h, then
// adds _t function-pointer typedefs required by FEOSSDKLoader.

#pragma once

#if defined(__has_include) && __has_include(<eos_anticheatserver.h>)
#  include <eos_anticheatserver.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  AntiCheatServer interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_BeginSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_BeginSessionOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_EndSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_EndSessionOptions* Options);
typedef void        (EOS_CALL *EOS_AntiCheatServer_RegisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_RegisterConnectedClientOptions* Options, void* ClientData, EOS_AntiCheatServer_OnClientConnectedCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_AntiCheatServer_UnregisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnregisterConnectedClientOptions* Options, void* ClientData, EOS_AntiCheatServer_OnClientDisconnectedCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_ReceiveMessageFromClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ReceiveMessageFromClientOptions* Options);
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyMessageToClientOptions* Options, void* ClientData, EOS_AntiCheatServer_OnMessageToClientCallback NotificationFn);
typedef void        (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions* Options, void* ClientData, EOS_AntiCheatServer_OnClientActionRequiredCallback NotificationFn);
typedef void        (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);
typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_GetProtectMessageOutputLength_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions* Options, uint32_t* OutBufferSizeBytes);
typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_ProtectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ProtectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);
typedef EOS_EResult (EOS_CALL *EOS_AntiCheatServer_UnprotectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnprotectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);
