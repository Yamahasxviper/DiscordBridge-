// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_anticheat.h — EOS SDK AntiCheatServer interface, written from scratch
// using only public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque client handle used by the anti-cheat server interface
// ─────────────────────────────────────────────────────────────────────────────
typedef void* EOS_AntiCheatCommon_ClientHandle;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAntiCheatClientAuthStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAntiCheatClientAuthStatus
{
    EOS_ACCS_Invalid              = 0,
    EOS_ACCS_LocalAuthRequired    = 1,
    EOS_ACCS_LocalAuthComplete    = 2,
    EOS_ACCS_RemoteAuthRequired   = 3,
    EOS_ACCS_RemoteAuthComplete   = 4
} EOS_EAntiCheatClientAuthStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAntiCheatCommonClientPlatform
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAntiCheatCommonClientPlatform
{
    EOS_ACCCP_Unknown      = 0,
    EOS_ACCCP_Windows      = 1,
    EOS_ACCCP_Mac          = 2,
    EOS_ACCCP_Linux        = 3,
    EOS_ACCCP_PlayStation  = 4,
    EOS_ACCCP_Xbox         = 5,
    EOS_ACCCP_Switch       = 6,
    EOS_ACCCP_iOS          = 7,
    EOS_ACCCP_Android      = 8
} EOS_EAntiCheatCommonClientPlatform;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAntiCheatCommonClientType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAntiCheatCommonClientType
{
    EOS_ACCT_ManagedByEOS          = 0,
    EOS_ACCT_ManagedByGame         = 1,
    EOS_ACCT_UnmanagedThirdParty   = 2
} EOS_EAntiCheatCommonClientType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAntiCheatCommonEventType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAntiCheatCommonEventType
{
    EOS_ACCET_Unknown     = 0,
    EOS_ACCET_AdminAction = 1
} EOS_EAntiCheatCommonEventType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAntiCheatCommonPlayerAction
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAntiCheatCommonPlayerAction
{
    EOS_ACCPA_Unknown         = 0,
    EOS_ACCPA_RemoveFromMatch = 1,
    EOS_ACCPA_PerformKick     = 2,
    EOS_ACCPA_PerformBan      = 3
} EOS_EAntiCheatCommonPlayerAction;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_BeginSessionOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_BEGINSESSION_API_LATEST 4

typedef struct EOS_AntiCheatServer_BeginSessionOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_BEGINSESSION_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local server user */
    EOS_ProductUserId LocalUserId;
    /** Human-readable server name */
    const char*       ServerName;
    /** EOS_TRUE to enable gameplay data collection */
    EOS_Bool          bEnableGameplayData;
    /** Maximum number of connected clients */
    uint32_t          MaxClients;
} EOS_AntiCheatServer_BeginSessionOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_EndSessionOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_ENDSESSION_API_LATEST 1

typedef struct EOS_AntiCheatServer_EndSessionOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_ENDSESSION_API_LATEST */
    int32_t ApiVersion;
} EOS_AntiCheatServer_EndSessionOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_RegisterConnectedClientOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_REGISTERCONNECTEDCLIENT_API_LATEST 2

typedef struct EOS_AntiCheatServer_RegisterConnectedClientOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_REGISTERCONNECTEDCLIENT_API_LATEST */
    int32_t                             ApiVersion;
    /** Opaque client handle identifying this connection */
    EOS_AntiCheatCommon_ClientHandle    ClientHandle;
    /** Client type (EOS-managed, game-managed, unmanaged) */
    EOS_EAntiCheatCommonClientType      ClientType;
    /** Platform the client is running on */
    EOS_EAntiCheatCommonClientPlatform  ClientPlatform;
    /** Deprecated account ID field — pass NULL */
    const char*                         AccountId_DEPRECATED;
    /** Optional client IP address string (may be NULL) */
    const char*                         IpAddress;
    /** Product User ID of the connecting client */
    EOS_ProductUserId                   UserId;
} EOS_AntiCheatServer_RegisterConnectedClientOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_UnregisterConnectedClientOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_UNREGISTERCONNECTEDCLIENT_API_LATEST 1

typedef struct EOS_AntiCheatServer_UnregisterConnectedClientOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_UNREGISTERCONNECTEDCLIENT_API_LATEST */
    int32_t                          ApiVersion;
    /** Client handle that was previously registered */
    EOS_AntiCheatCommon_ClientHandle ClientHandle;
} EOS_AntiCheatServer_UnregisterConnectedClientOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_ReceiveMessageFromClientOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_RECEIVEMESSAGEFROMCLIENT_API_LATEST 1

typedef struct EOS_AntiCheatServer_ReceiveMessageFromClientOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_RECEIVEMESSAGEFROMCLIENT_API_LATEST */
    int32_t                          ApiVersion;
    /** Client handle that sent this message */
    EOS_AntiCheatCommon_ClientHandle ClientHandle;
    /** Size in bytes of the data buffer */
    uint32_t                         DataLengthBytes;
    /** Pointer to the raw message data */
    const void*                      Data;
} EOS_AntiCheatServer_ReceiveMessageFromClientOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_MessageToClientCallbackInfo / AddNotifyMessageToClient
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_AntiCheatServer_MessageToClientCallbackInfo
{
    /** Context that was passed to AddNotifyMessageToClient */
    void*                            ClientData;
    /** Destination client handle */
    EOS_AntiCheatCommon_ClientHandle ClientHandle;
    /** Size in bytes of MessageData */
    uint32_t                         MessageDataSizeBytes;
    /** Pointer to message data to forward to the client */
    const void*                      MessageData;
} EOS_AntiCheatServer_MessageToClientCallbackInfo;

typedef void (EOS_CALL *EOS_AntiCheatServer_OnMessageToClientCallback)(const EOS_AntiCheatServer_MessageToClientCallbackInfo* Data);

#define EOS_ANTICHEATSERVER_ADDNOTIFYMESSAGETOCLIENT_API_LATEST 1

typedef struct EOS_AntiCheatServer_AddNotifyMessageToClientOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_ADDNOTIFYMESSAGETOCLIENT_API_LATEST */
    int32_t ApiVersion;
} EOS_AntiCheatServer_AddNotifyMessageToClientOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_ClientActionRequiredCallbackInfo / AddNotifyClientActionRequired
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_AntiCheatServer_ClientActionRequiredCallbackInfo
{
    /** Context that was passed to AddNotifyClientActionRequired */
    void*                             ClientData;
    /** Client handle that requires action */
    EOS_AntiCheatCommon_ClientHandle  ClientHandle;
    /** Recommended action code */
    EOS_EAntiCheatCommonPlayerAction  ActionReasonCode;
    /** Human-readable reason string (may be NULL) */
    const char*                       ActionReasonDetailsString;
} EOS_AntiCheatServer_ClientActionRequiredCallbackInfo;

typedef void (EOS_CALL *EOS_AntiCheatServer_OnClientActionRequiredCallback)(const EOS_AntiCheatServer_ClientActionRequiredCallbackInfo* Data);

#define EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTACTIONREQUIRED_API_LATEST 1

typedef struct EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTACTIONREQUIRED_API_LATEST */
    int32_t ApiVersion;
} EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_GETPROTECTMESSAGEOUTPUTLENGTH_API_LATEST 1

typedef struct EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_GETPROTECTMESSAGEOUTPUTLENGTH_API_LATEST */
    int32_t  ApiVersion;
    /** Size in bytes of the plaintext data to be protected */
    uint32_t DataLengthBytes;
} EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_ProtectMessageOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_PROTECTMESSAGE_API_LATEST 1

typedef struct EOS_AntiCheatServer_ProtectMessageOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_PROTECTMESSAGE_API_LATEST */
    int32_t                          ApiVersion;
    /** Destination client handle */
    EOS_AntiCheatCommon_ClientHandle ClientHandle;
    /** Size in bytes of Data */
    uint32_t                         DataLengthBytes;
    /** Plaintext data to protect */
    const void*                      Data;
    /** Capacity of the caller's output buffer */
    uint32_t                         OutBufferSizeBytes;
} EOS_AntiCheatServer_ProtectMessageOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_AntiCheatServer_UnprotectMessageOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_ANTICHEATSERVER_UNPROTECTMESSAGE_API_LATEST 1

typedef struct EOS_AntiCheatServer_UnprotectMessageOptions
{
    /** API version: must be EOS_ANTICHEATSERVER_UNPROTECTMESSAGE_API_LATEST */
    int32_t                          ApiVersion;
    /** Source client handle */
    EOS_AntiCheatCommon_ClientHandle ClientHandle;
    /** Size in bytes of Data */
    uint32_t                         DataLengthBytes;
    /** Protected data to unprotect */
    const void*                      Data;
    /** Capacity of the caller's output buffer */
    uint32_t                         OutBufferSizeBytes;
} EOS_AntiCheatServer_UnprotectMessageOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  AntiCheatServer interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Begins an anti-cheat server session */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_BeginSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_BeginSessionOptions* Options);

/** Ends the anti-cheat server session */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_EndSession_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_EndSessionOptions* Options);

/** Registers a newly connected client */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_RegisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_RegisterConnectedClientOptions* Options);

/** Unregisters a disconnected client */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_UnregisterConnectedClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnregisterConnectedClientOptions* Options);

/** Passes a message received from a game client to the anti-cheat system */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_ReceiveMessageFromClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ReceiveMessageFromClientOptions* Options);

/** Registers a callback for outgoing messages to be forwarded to clients */
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyMessageToClientOptions* Options, void* ClientData, EOS_AntiCheatServer_OnMessageToClientCallback NotificationFn);

/** Removes a message-to-client notification */
typedef void               (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyMessageToClient_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);

/** Registers a callback for required client actions (kick, ban, etc.) */
typedef EOS_NotificationId (EOS_CALL *EOS_AntiCheatServer_AddNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions* Options, void* ClientData, EOS_AntiCheatServer_OnClientActionRequiredCallback NotificationFn);

/** Removes a client-action-required notification */
typedef void               (EOS_CALL *EOS_AntiCheatServer_RemoveNotifyClientActionRequired_t)(EOS_HAntiCheatServer Handle, EOS_NotificationId NotificationId);

/** Returns the required output buffer size for EOS_AntiCheatServer_ProtectMessage */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_GetProtectMessageOutputLength_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions* Options, uint32_t* OutBufferLengthBytes);

/** Protects (encrypts/authenticates) a message to be sent to a client */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_ProtectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_ProtectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);

/** Unprotects (decrypts/validates) a message received from a client */
typedef EOS_EResult        (EOS_CALL *EOS_AntiCheatServer_UnprotectMessage_t)(EOS_HAntiCheatServer Handle, const EOS_AntiCheatServer_UnprotectMessageOptions* Options, void* OutBuffer, uint32_t* OutBytesWritten);

#ifdef __cplusplus
}
#endif
