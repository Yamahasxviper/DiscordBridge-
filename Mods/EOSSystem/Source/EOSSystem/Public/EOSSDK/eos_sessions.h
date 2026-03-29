// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_sessions.h — EOS SDK Sessions interface, written from scratch using only
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
//  EOS_EOnlineSessionState
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EOnlineSessionState
{
    EOS_OSS_NoSession  = 0,
    EOS_OSS_Creating   = 1,
    EOS_OSS_Pending    = 2,
    EOS_OSS_Starting   = 3,
    EOS_OSS_InProgress = 4,
    EOS_OSS_Ending     = 5,
    EOS_OSS_Ended      = 6,
    EOS_OSS_Destroying = 7
} EOS_EOnlineSessionState;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ESessionAttributeAdvertisementType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ESessionAttributeAdvertisementType
{
    EOS_SAAT_DontAdvertise = 0,
    EOS_SAAT_Advertise     = 1
} EOS_ESessionAttributeAdvertisementType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EOnlineComparisonOp
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EOnlineComparisonOp
{
    EOS_CO_EQUAL             = 0,
    EOS_CO_NOTEQUAL          = 1,
    EOS_CO_GREATERTHAN       = 2,
    EOS_CO_GREATERTHANEQUALS = 3,
    EOS_CO_LESSTHAN          = 4,
    EOS_CO_LESSTHANEQUALS    = 5,
    EOS_CO_DISTANCE          = 6,
    EOS_CO_IN                = 7,
    EOS_CO_NOTIN             = 8,
    EOS_CO_ANYOF             = 9,
    EOS_CO_NOTANYOF          = 10
} EOS_EOnlineComparisonOp;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EAttributeType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EAttributeType
{
    EOS_AT_BOOLEAN = 0,
    EOS_AT_INT64   = 1,
    EOS_AT_DOUBLE  = 2,
    EOS_AT_STRING  = 3
} EOS_EAttributeType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_AttributeData_Value  (union)
// ─────────────────────────────────────────────────────────────────────────────
typedef union EOS_Sessions_AttributeData_Value
{
    /** Integer (int64) value */
    int64_t     AsInt64;
    /** Double-precision float value */
    double      AsDouble;
    /** Boolean value */
    EOS_Bool    AsBool;
    /** UTF-8 string pointer */
    const char* AsUtf8;
} EOS_Sessions_AttributeData_Value;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_AttributeData
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST 1

typedef struct EOS_Sessions_AttributeData
{
    /** API version: must be EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST */
    int32_t                          ApiVersion;
    /** Attribute key string */
    const char*                      Key;
    /** Attribute value (interpret based on ValueType) */
    EOS_Sessions_AttributeData_Value Value;
    /** Discriminator for the Value union */
    EOS_EAttributeType               ValueType;
} EOS_Sessions_AttributeData;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_AddAttributeOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST 1

typedef struct EOS_SessionModification_AddAttributeOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST */
    int32_t                                    ApiVersion;
    /** Attribute data to add */
    const EOS_Sessions_AttributeData*          SessionAttribute;
    /** Whether to advertise this attribute in search results */
    EOS_ESessionAttributeAdvertisementType     AdvertisementType;
} EOS_SessionModification_AddAttributeOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_RemoveAttributeOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_REMOVEATTRIBUTE_API_LATEST 1

typedef struct EOS_SessionModification_RemoveAttributeOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_REMOVEATTRIBUTE_API_LATEST */
    int32_t     ApiVersion;
    /** Key of the attribute to remove */
    const char* Key;
} EOS_SessionModification_RemoveAttributeOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_SetBucketIdOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_SETBUCKETID_API_LATEST 1

typedef struct EOS_SessionModification_SetBucketIdOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_SETBUCKETID_API_LATEST */
    int32_t     ApiVersion;
    /** Bucket ID string for matchmaking categorisation */
    const char* BucketId;
} EOS_SessionModification_SetBucketIdOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_SetHostAddressOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST 1

typedef struct EOS_SessionModification_SetHostAddressOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST */
    int32_t     ApiVersion;
    /** Host address string (e.g., IP:port) */
    const char* HostAddress;
} EOS_SessionModification_SetHostAddressOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_SetMaxPlayersOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_SETMAXPLAYERS_API_LATEST 1

typedef struct EOS_SessionModification_SetMaxPlayersOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_SETMAXPLAYERS_API_LATEST */
    int32_t  ApiVersion;
    /** Maximum number of players allowed in the session */
    uint32_t MaxPlayers;
} EOS_SessionModification_SetMaxPlayersOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_SetJoinInProgressAllowedOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST 1

typedef struct EOS_SessionModification_SetJoinInProgressAllowedOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST */
    int32_t  ApiVersion;
    /** EOS_TRUE to allow players to join while the session is in progress */
    EOS_Bool bAllowJoinInProgress;
} EOS_SessionModification_SetJoinInProgressAllowedOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EOnlineSessionPermissionLevel (inline enum for SetPermissionLevel)
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EOnlineSessionPermissionLevel
{
    EOS_OSPF_PublicAdvertised   = 0,
    EOS_OSPF_JoinViaPresence    = 1,
    EOS_OSPF_InviteOnly         = 2
} EOS_EOnlineSessionPermissionLevel;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_SessionModification_SetPermissionLevelOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST 1

typedef struct EOS_SessionModification_SetPermissionLevelOptions
{
    /** API version: must be EOS_SESSIONMODIFICATION_SETPERMISSIONLEVEL_API_LATEST */
    int32_t                         ApiVersion;
    /** The permission level for this session */
    EOS_EOnlineSessionPermissionLevel PermissionLevel;
} EOS_SessionModification_SetPermissionLevelOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_CreateSessionModificationOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST 3

typedef struct EOS_Sessions_CreateSessionModificationOptions
{
    /** API version: must be EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST */
    int32_t           ApiVersion;
    /** Locally unique name for this session */
    const char*       SessionName;
    /** Bucket ID for matchmaking categorisation */
    const char*       BucketId;
    /** Maximum number of players */
    uint32_t          MaxPlayers;
    /** Product User ID of the local user creating the session */
    EOS_ProductUserId LocalUserId;
    /** EOS_TRUE to link presence with this session */
    EOS_Bool          bPresenceEnabled;
    /** Optional override session ID (NULL = auto-generate) */
    const char*       SessionId;
    /** EOS_TRUE to enable sanctions checks when registering players */
    EOS_Bool          bSanctionsEnabled;
} EOS_Sessions_CreateSessionModificationOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_UpdateSessionModificationOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_LATEST 1

typedef struct EOS_Sessions_UpdateSessionModificationOptions
{
    /** API version: must be EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_LATEST */
    int32_t     ApiVersion;
    /** Locally unique session name to update */
    const char* SessionName;
} EOS_Sessions_UpdateSessionModificationOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_UpdateSessionOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_UPDATESESSION_API_LATEST 1

typedef struct EOS_Sessions_UpdateSessionOptions
{
    /** API version: must be EOS_SESSIONS_UPDATESESSION_API_LATEST */
    int32_t                  ApiVersion;
    /** Handle to a session modification object */
    EOS_HSessionModification SessionModificationHandle;
} EOS_Sessions_UpdateSessionOptions;

typedef struct EOS_Sessions_UpdateSessionCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* SessionName;
    const char* SessionId;
} EOS_Sessions_UpdateSessionCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnUpdateSessionCallback)(const EOS_Sessions_UpdateSessionCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_DestroySessionOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_DESTROYSESSION_API_LATEST 1

typedef struct EOS_Sessions_DestroySessionOptions
{
    /** API version: must be EOS_SESSIONS_DESTROYSESSION_API_LATEST */
    int32_t     ApiVersion;
    /** Locally unique session name to destroy */
    const char* SessionName;
} EOS_Sessions_DestroySessionOptions;

typedef struct EOS_Sessions_DestroySessionCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* SessionName;
} EOS_Sessions_DestroySessionCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnDestroySessionCallback)(const EOS_Sessions_DestroySessionCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_JoinSessionOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_JOINSESSION_API_LATEST 1

typedef struct EOS_Sessions_JoinSessionOptions
{
    /** API version: must be EOS_SESSIONS_JOINSESSION_API_LATEST */
    int32_t            ApiVersion;
    /** Locally unique session name to use after joining */
    const char*        SessionName;
    /** Handle to a session details object describing the session to join */
    EOS_HSessionDetails SessionHandle;
    /** Product User ID of the local user joining */
    EOS_ProductUserId  LocalUserId;
    /** EOS_TRUE to link presence with this session */
    EOS_Bool           bPresenceEnabled;
} EOS_Sessions_JoinSessionOptions;

typedef struct EOS_Sessions_JoinSessionCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* SessionName;
} EOS_Sessions_JoinSessionCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnJoinSessionCallback)(const EOS_Sessions_JoinSessionCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_StartSessionOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_STARTSESSION_API_LATEST 1

typedef struct EOS_Sessions_StartSessionOptions
{
    /** API version: must be EOS_SESSIONS_STARTSESSION_API_LATEST */
    int32_t     ApiVersion;
    /** Locally unique session name to start */
    const char* SessionName;
} EOS_Sessions_StartSessionOptions;

typedef struct EOS_Sessions_StartSessionCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* SessionName;
} EOS_Sessions_StartSessionCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnStartSessionCallback)(const EOS_Sessions_StartSessionCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_EndSessionOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_ENDSESSION_API_LATEST 1

typedef struct EOS_Sessions_EndSessionOptions
{
    /** API version: must be EOS_SESSIONS_ENDSESSION_API_LATEST */
    int32_t     ApiVersion;
    /** Locally unique session name to end */
    const char* SessionName;
} EOS_Sessions_EndSessionOptions;

typedef struct EOS_Sessions_EndSessionCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* SessionName;
} EOS_Sessions_EndSessionCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnEndSessionCallback)(const EOS_Sessions_EndSessionCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_RegisterPlayersOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_REGISTERPLAYERS_API_LATEST 1

typedef struct EOS_Sessions_RegisterPlayersOptions
{
    /** API version: must be EOS_SESSIONS_REGISTERPLAYERS_API_LATEST */
    int32_t                  ApiVersion;
    /** Session name to register players into */
    const char*              SessionName;
    /** Array of Product User IDs to register */
    const EOS_ProductUserId* PlayersToRegister;
    /** Number of entries in PlayersToRegister */
    uint32_t                 PlayersToRegisterCount;
} EOS_Sessions_RegisterPlayersOptions;

typedef struct EOS_Sessions_RegisterPlayersCallbackInfo
{
    EOS_EResult              ResultCode;
    void*                    ClientData;
    const char*              SessionName;
    /** Players that were successfully registered */
    const EOS_ProductUserId* RegisteredPlayers;
    uint32_t                 RegisteredPlayersCount;
    /** Players that were sanctioned and blocked from joining */
    const EOS_ProductUserId* SanctionedPlayers;
    uint32_t                 SanctionedPlayersCount;
} EOS_Sessions_RegisterPlayersCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnRegisterPlayersCallback)(const EOS_Sessions_RegisterPlayersCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Sessions_UnregisterPlayersOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_SESSIONS_UNREGISTERPLAYERS_API_LATEST 1

typedef struct EOS_Sessions_UnregisterPlayersOptions
{
    /** API version: must be EOS_SESSIONS_UNREGISTERPLAYERS_API_LATEST */
    int32_t                  ApiVersion;
    /** Session name to unregister players from */
    const char*              SessionName;
    /** Array of Product User IDs to unregister */
    const EOS_ProductUserId* PlayersToUnregister;
    /** Number of entries in PlayersToUnregister */
    uint32_t                 PlayersToUnregisterCount;
} EOS_Sessions_UnregisterPlayersOptions;

typedef struct EOS_Sessions_UnregisterPlayersCallbackInfo
{
    EOS_EResult              ResultCode;
    void*                    ClientData;
    const char*              SessionName;
    /** Players that were successfully unregistered */
    const EOS_ProductUserId* UnregisteredPlayers;
    uint32_t                 UnregisteredPlayersCount;
} EOS_Sessions_UnregisterPlayersCallbackInfo;

typedef void (EOS_CALL *EOS_Sessions_OnUnregisterPlayersCallback)(const EOS_Sessions_UnregisterPlayersCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  Sessions interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Creates a modification handle for a new session */
typedef EOS_EResult (EOS_CALL *EOS_Sessions_CreateSessionModification_t)(EOS_HSessions Handle, const EOS_Sessions_CreateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle);

/** Creates a modification handle for an existing session */
typedef EOS_EResult (EOS_CALL *EOS_Sessions_UpdateSessionModification_t)(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionModificationOptions* Options, EOS_HSessionModification* OutSessionModificationHandle);

/** Commits a session modification to the backend */
typedef void        (EOS_CALL *EOS_Sessions_UpdateSession_t)(EOS_HSessions Handle, const EOS_Sessions_UpdateSessionOptions* Options, void* ClientData, EOS_Sessions_OnUpdateSessionCallback CompletionDelegate);

/** Destroys an active session */
typedef void        (EOS_CALL *EOS_Sessions_DestroySession_t)(EOS_HSessions Handle, const EOS_Sessions_DestroySessionOptions* Options, void* ClientData, EOS_Sessions_OnDestroySessionCallback CompletionDelegate);

/** Joins an existing session described by a session details handle */
typedef void        (EOS_CALL *EOS_Sessions_JoinSession_t)(EOS_HSessions Handle, const EOS_Sessions_JoinSessionOptions* Options, void* ClientData, EOS_Sessions_OnJoinSessionCallback CompletionDelegate);

/** Marks a session as in-progress */
typedef void        (EOS_CALL *EOS_Sessions_StartSession_t)(EOS_HSessions Handle, const EOS_Sessions_StartSessionOptions* Options, void* ClientData, EOS_Sessions_OnStartSessionCallback CompletionDelegate);

/** Marks a session as ended */
typedef void        (EOS_CALL *EOS_Sessions_EndSession_t)(EOS_HSessions Handle, const EOS_Sessions_EndSessionOptions* Options, void* ClientData, EOS_Sessions_OnEndSessionCallback CompletionDelegate);

/** Registers players in a session (for anti-cheat and sanctions) */
typedef void        (EOS_CALL *EOS_Sessions_RegisterPlayers_t)(EOS_HSessions Handle, const EOS_Sessions_RegisterPlayersOptions* Options, void* ClientData, EOS_Sessions_OnRegisterPlayersCallback CompletionDelegate);

/** Unregisters players from a session */
typedef void        (EOS_CALL *EOS_Sessions_UnregisterPlayers_t)(EOS_HSessions Handle, const EOS_Sessions_UnregisterPlayersOptions* Options, void* ClientData, EOS_Sessions_OnUnregisterPlayersCallback CompletionDelegate);

/** Sets the bucket ID on a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetBucketId_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetBucketIdOptions* Options);

/** Sets the host address on a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetHostAddress_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetHostAddressOptions* Options);

/** Sets the max player count on a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetMaxPlayers_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetMaxPlayersOptions* Options);

/** Sets join-in-progress flag on a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetJoinInProgressAllowed_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetJoinInProgressAllowedOptions* Options);

/** Sets the permission level on a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_SetPermissionLevel_t)(EOS_HSessionModification Handle, const EOS_SessionModification_SetPermissionLevelOptions* Options);

/** Adds an attribute to a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_AddAttribute_t)(EOS_HSessionModification Handle, const EOS_SessionModification_AddAttributeOptions* Options);

/** Removes an attribute from a session modification */
typedef EOS_EResult (EOS_CALL *EOS_SessionModification_RemoveAttribute_t)(EOS_HSessionModification Handle, const EOS_SessionModification_RemoveAttributeOptions* Options);

/** Releases a session modification handle */
typedef void        (EOS_CALL *EOS_SessionModification_Release_t)(EOS_HSessionModification SessionModificationHandle);

#ifdef __cplusplus
}
#endif
