// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_anticheat.h — delegates to the real EOS SDK eos_anticheatserver.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_anticheatserver.h via the EOSSDK module include path.
#include <eos_anticheatserver.h>

// ─────────────────────────────────────────────────────────────────────────────
//  AntiCheat Server interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
//  The real EOS SDK eos_anticheatserver.h declares the functions but does not
//  provide these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_BeginSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_BeginSessionOptions* Options);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_EndSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_EndSessionOptions* Options);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_RegisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_RegisterConnectedClientOptions* Options);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_UnregisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnregisterConnectedClientOptions* Options);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_ReceiveMessageFromClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ReceiveMessageFromClientOptions* Options);
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyMessageToClientOptions* Options, void* ClientData, EOS_AntiCheatServer_OnMessageToClientCallback NotificationFn);
typedef void               (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions* Options, void* ClientData, EOS_AntiCheatServer_OnClientActionRequiredCallback NotificationFn);
typedef void               (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_GetProtectMessageOutputLength_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions* Options, uint32_t* OutBufferLengthBytes);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_ProtectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ProtectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_UnprotectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnprotectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);
