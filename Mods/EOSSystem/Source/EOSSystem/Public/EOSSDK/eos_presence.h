// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_presence.h — EOS SDK Presence interface, written from scratch using only
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
//  Forward declare the Presence handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PresenceHandle;
typedef struct EOS_PresenceHandle* EOS_HPresence;

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque PresenceModification handle
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PresenceModificationDetails;
typedef struct EOS_PresenceModificationDetails* EOS_HPresenceModification;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EPresenceModificationStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EPresenceModificationStatus
{
    EOS_PMS_Offline        = 0,
    EOS_PMS_Online         = 1,
    EOS_PMS_Away           = 2,
    EOS_PMS_ExtendedAway   = 3,
    EOS_PMS_DoNotDisturb   = 4
} EOS_EPresenceModificationStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Presence_DataRecord
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCE_DATARECORD_API_LATEST 1

typedef struct EOS_Presence_DataRecord
{
    /** API version: must be EOS_PRESENCE_DATARECORD_API_LATEST */
    int32_t     ApiVersion;
    /** UTF-8 key for this data record */
    const char* Key;
    /** UTF-8 value for this data record */
    const char* Value;
} EOS_Presence_DataRecord;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PresenceModification_SetStatusOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST 1

typedef struct EOS_PresenceModification_SetStatusOptions
{
    /** API version: must be EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST */
    int32_t                         ApiVersion;
    /** The new online status */
    EOS_EPresenceModificationStatus Status;
} EOS_PresenceModification_SetStatusOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PresenceModification_SetRawRichTextOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST 1

typedef struct EOS_PresenceModification_SetRawRichTextOptions
{
    /** API version: must be EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST */
    int32_t     ApiVersion;
    /** Raw rich-text status string */
    const char* RichText;
} EOS_PresenceModification_SetRawRichTextOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PresenceModification_SetDataOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCEMODIFICATION_SETDATA_API_LATEST 1

typedef struct EOS_PresenceModification_SetDataOptions
{
    /** API version: must be EOS_PRESENCEMODIFICATION_SETDATA_API_LATEST */
    int32_t                       ApiVersion;
    /** Number of records in the Records array */
    uint32_t                      RecordsCount;
    /** Array of data records to set */
    const EOS_Presence_DataRecord* Records;
} EOS_PresenceModification_SetDataOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PresenceModification_DeleteData_Record  (key-only struct for deletion)
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_PresenceModification_DataRecordId
{
    /** API version of this record ID entry */
    int32_t     ApiVersion;
    /** Key of the record to delete */
    const char* Key;
} EOS_PresenceModification_DataRecordId;

#define EOS_PRESENCEMODIFICATION_DATARECORDID_API_LATEST 1

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PresenceModification_DeleteDataOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCEMODIFICATION_DELETEDATA_API_LATEST 1

typedef struct EOS_PresenceModification_DeleteDataOptions
{
    /** API version: must be EOS_PRESENCEMODIFICATION_DELETEDATA_API_LATEST */
    int32_t                                       ApiVersion;
    /** Number of records to delete */
    uint32_t                                      RecordsCount;
    /** Array of record IDs (keys) to delete */
    const EOS_PresenceModification_DataRecordId*  Records;
} EOS_PresenceModification_DeleteDataOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Presence_CreatePresenceModificationOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST 1

typedef struct EOS_Presence_CreatePresenceModificationOptions
{
    /** API version: must be EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
} EOS_Presence_CreatePresenceModificationOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Presence_SetPresenceOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCE_SETPRESENCE_API_LATEST 1

typedef struct EOS_Presence_SetPresenceOptions
{
    /** API version: must be EOS_PRESENCE_SETPRESENCE_API_LATEST */
    int32_t                  ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId        LocalUserId;
    /** Handle to a modification created via EOS_Presence_CreatePresenceModification */
    EOS_HPresenceModification PresenceModificationHandle;
} EOS_Presence_SetPresenceOptions;

typedef struct EOS_Presence_SetPresenceCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
} EOS_Presence_SetPresenceCallbackInfo;

typedef void (EOS_CALL *EOS_Presence_SetPresenceCompleteCallback)(const EOS_Presence_SetPresenceCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Presence_QueryPresenceOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PRESENCE_QUERYPRESENCE_API_LATEST 1

typedef struct EOS_Presence_QueryPresenceOptions
{
    /** API version: must be EOS_PRESENCE_QUERYPRESENCE_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user making the query */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the target user */
    EOS_EpicAccountId TargetUserId;
} EOS_Presence_QueryPresenceOptions;

typedef struct EOS_Presence_QueryPresenceCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
} EOS_Presence_QueryPresenceCallbackInfo;

typedef void (EOS_CALL *EOS_Presence_OnQueryPresenceCompleteCallback)(const EOS_Presence_QueryPresenceCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  Presence interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries presence for the target user */
typedef void        (EOS_CALL *EOS_Presence_QueryPresence_t)(EOS_HPresence Handle, const EOS_Presence_QueryPresenceOptions* Options, void* ClientData, EOS_Presence_OnQueryPresenceCompleteCallback CompletionDelegate);

/** Creates a presence modification handle; release with EOS_PresenceModification_Release */
typedef EOS_EResult (EOS_CALL *EOS_Presence_CreatePresenceModification_t)(EOS_HPresence Handle, const EOS_Presence_CreatePresenceModificationOptions* Options, EOS_HPresenceModification* OutPresenceModificationHandle);

/** Commits presence modifications to the backend */
typedef void        (EOS_CALL *EOS_Presence_SetPresence_t)(EOS_HPresence Handle, const EOS_Presence_SetPresenceOptions* Options, void* ClientData, EOS_Presence_SetPresenceCompleteCallback CompletionDelegate);

/** Sets the online status in a presence modification */
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetStatus_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetStatusOptions* Options);

/** Sets the rich-text string in a presence modification */
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetRawRichText_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetRawRichTextOptions* Options);

/** Sets data records in a presence modification */
typedef EOS_EResult (EOS_CALL *EOS_PresenceModification_SetData_t)(EOS_HPresenceModification Handle, const EOS_PresenceModification_SetDataOptions* Options);

/** Releases a presence modification handle */
typedef void        (EOS_CALL *EOS_PresenceModification_Release_t)(EOS_HPresenceModification PresenceModificationHandle);

#ifdef __cplusplus
}
#endif
