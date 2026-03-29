// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_storage.h — EOS SDK PlayerDataStorage and TitleStorage interfaces,
// written from scratch using only public EOS SDK documentation
// (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ═════════════════════════════════════════════════════════════════════════════
//  PLAYER DATA STORAGE
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the PlayerDataStorage handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PlayerDataStorageHandleDetails;
typedef struct EOS_PlayerDataStorageHandleDetails* EOS_HPlayerDataStorage;

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque file transfer request handle
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PlayerDataStorageFileTransferRequestDetails;
typedef struct EOS_PlayerDataStorageFileTransferRequestDetails* EOS_HPlayerDataStorageFileTransferRequest;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EPlayerDataStorageEoseWriteResult
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EPlayerDataStorageEoseWriteResult
{
    EOS_PDSEWR_ContinueWriting = 0,
    EOS_PDSEWR_CompleteRequest = 1,
    EOS_PDSEWR_FailRequest     = 2,
    EOS_PDSEWR_CancelRequest   = 3
} EOS_EPlayerDataStorageEoseWriteResult;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EPlayerDataStorageReadResult
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EPlayerDataStorageReadResult
{
    EOS_PDSRR_ContinueReading = 0,
    EOS_PDSRR_FailRequest     = 1,
    EOS_PDSRR_CancelRequest   = 2
} EOS_EPlayerDataStorageReadResult;

// ─────────────────────────────────────────────────────────────────────────────
//  Callback info structs for the streaming callbacks
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_PlayerDataStorage_ReadFileDataCallbackInfo
{
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
    uint32_t          TotalFileSizeBytes;
    EOS_Bool          bIsLastChunk;
    uint32_t          DataChunkLengthBytes;
    const void*       DataChunk;
} EOS_PlayerDataStorage_ReadFileDataCallbackInfo;

typedef struct EOS_PlayerDataStorage_WriteFileDataCallbackInfo
{
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
    uint32_t          DataBufferLengthBytes;
} EOS_PlayerDataStorage_WriteFileDataCallbackInfo;

typedef struct EOS_PlayerDataStorage_FileTransferProgressCallbackInfo
{
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
    uint32_t          BytesTransferred;
    uint32_t          TotalFileSizeBytes;
} EOS_PlayerDataStorage_FileTransferProgressCallbackInfo;

// ─────────────────────────────────────────────────────────────────────────────
//  Streaming callback typedefs
// ─────────────────────────────────────────────────────────────────────────────
typedef EOS_EPlayerDataStorageReadResult       (EOS_CALL *EOS_PlayerDataStorage_OnReadFileDataCallback)(const EOS_PlayerDataStorage_ReadFileDataCallbackInfo* Data);
typedef EOS_EPlayerDataStorageEoseWriteResult  (EOS_CALL *EOS_PlayerDataStorage_OnWriteFileDataCallback)(const EOS_PlayerDataStorage_WriteFileDataCallbackInfo* Data, void* OutDataBuffer, uint32_t* OutDataWritten);
typedef void                                   (EOS_CALL *EOS_PlayerDataStorage_OnFileTransferProgressCallback)(const EOS_PlayerDataStorage_FileTransferProgressCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_FileMetadata
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_FILEMETADATA_API_LATEST 2

typedef struct EOS_PlayerDataStorage_FileMetadata
{
    /** API version: must be EOS_PLAYERDATASTORAGE_FILEMETADATA_API_LATEST */
    int32_t     ApiVersion;
    /** Total size of the file in bytes */
    uint32_t    FileSizeBytes;
    /** MD5 hash of the file contents (hex string) */
    const char* MD5Hash;
    /** Name of the file */
    const char* Filename;
    /** UTC Unix timestamp of the last modification */
    int64_t     LastModifiedTime;
    /** Size of the file data before encryption */
    uint32_t    UnencryptedDataSizeBytes;
} EOS_PlayerDataStorage_FileMetadata;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_QueryFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_QUERYFILE_API_LATEST 1

typedef struct EOS_PlayerDataStorage_QueryFileOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_QUERYFILE_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
    /** Name of the file to query */
    const char*       Filename;
} EOS_PlayerDataStorage_QueryFileOptions;

typedef struct EOS_PlayerDataStorage_QueryFileCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
} EOS_PlayerDataStorage_QueryFileCallbackInfo;

typedef void (EOS_CALL *EOS_PlayerDataStorage_OnQueryFileCompleteCallback)(const EOS_PlayerDataStorage_QueryFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_QueryFileListOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_QUERYFILELIST_API_LATEST 1

typedef struct EOS_PlayerDataStorage_QueryFileListOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_QUERYFILELIST_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
} EOS_PlayerDataStorage_QueryFileListOptions;

typedef struct EOS_PlayerDataStorage_QueryFileListCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    /** Number of files in the file list */
    uint32_t          FileCount;
} EOS_PlayerDataStorage_QueryFileListCallbackInfo;

typedef void (EOS_CALL *EOS_PlayerDataStorage_OnQueryFileListCompleteCallback)(const EOS_PlayerDataStorage_QueryFileListCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_GetFileMetadataCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_GETFILEMETADATACOUNT_API_LATEST 1

typedef struct EOS_PlayerDataStorage_GetFileMetadataCountOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_GETFILEMETADATACOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
} EOS_PlayerDataStorage_GetFileMetadataCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYINDEX_API_LATEST 1

typedef struct EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
    /** Zero-based index of the file metadata entry to copy */
    uint32_t          Index;
} EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_ReadFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_READFILE_API_LATEST 1

typedef struct EOS_PlayerDataStorage_ReadFileOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_READFILE_API_LATEST */
    int32_t                                          ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId                                LocalUserId;
    /** Name of the file to read */
    const char*                                      Filename;
    /** Preferred size in bytes for each data chunk delivered to the read callback */
    uint32_t                                         ReadChunkLengthBytes;
    /** Called for each chunk of file data */
    EOS_PlayerDataStorage_OnReadFileDataCallback     ReadFileDataCallback;
    /** Called periodically to report transfer progress (may be NULL) */
    EOS_PlayerDataStorage_OnFileTransferProgressCallback FileTransferProgressCallback;
} EOS_PlayerDataStorage_ReadFileOptions;

typedef struct EOS_PlayerDataStorage_ReadFileCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
} EOS_PlayerDataStorage_ReadFileCallbackInfo;

typedef void (EOS_CALL *EOS_PlayerDataStorage_OnReadFileCompleteCallback)(const EOS_PlayerDataStorage_ReadFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_WriteFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_WRITEFILE_API_LATEST 1

typedef struct EOS_PlayerDataStorage_WriteFileOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_WRITEFILE_API_LATEST */
    int32_t                                          ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId                                LocalUserId;
    /** Name of the file to write */
    const char*                                      Filename;
    /** Maximum size in bytes of each chunk that will be requested */
    uint32_t                                         ChunkLengthBytes;
    /** Called to supply each chunk of data to write */
    EOS_PlayerDataStorage_OnWriteFileDataCallback    WriteFileDataCallback;
    /** Called periodically to report transfer progress (may be NULL) */
    EOS_PlayerDataStorage_OnFileTransferProgressCallback FileTransferProgressCallback;
} EOS_PlayerDataStorage_WriteFileOptions;

typedef struct EOS_PlayerDataStorage_WriteFileCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
} EOS_PlayerDataStorage_WriteFileCallbackInfo;

typedef void (EOS_CALL *EOS_PlayerDataStorage_OnWriteFileCompleteCallback)(const EOS_PlayerDataStorage_WriteFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_PlayerDataStorage_DeleteFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_PLAYERDATASTORAGE_DELETEFILE_API_LATEST 1

typedef struct EOS_PlayerDataStorage_DeleteFileOptions
{
    /** API version: must be EOS_PLAYERDATASTORAGE_DELETEFILE_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
    /** Name of the file to delete */
    const char*       Filename;
} EOS_PlayerDataStorage_DeleteFileOptions;

typedef struct EOS_PlayerDataStorage_DeleteFileCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_ProductUserId LocalUserId;
    const char*       Filename;
} EOS_PlayerDataStorage_DeleteFileCallbackInfo;

typedef void (EOS_CALL *EOS_PlayerDataStorage_OnDeleteFileCompleteCallback)(const EOS_PlayerDataStorage_DeleteFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  PlayerDataStorage interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries metadata for a single file */
typedef void                                  (EOS_CALL *EOS_PlayerDataStorage_QueryFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnQueryFileCompleteCallback CompletionDelegate);

/** Queries metadata for all files owned by the user */
typedef void                                  (EOS_CALL *EOS_PlayerDataStorage_QueryFileList_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileListOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnQueryFileListCompleteCallback CompletionDelegate);

/** Returns the number of cached file metadata entries */
typedef uint32_t                              (EOS_CALL *EOS_PlayerDataStorage_GetFileMetadataCount_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_GetFileMetadataCountOptions* Options);

/** Copies file metadata by index; release with EOS_PlayerDataStorage_FileMetadata_Release */
typedef EOS_EResult                           (EOS_CALL *EOS_PlayerDataStorage_CopyFileMetadataByIndex_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions* Options, EOS_PlayerDataStorage_FileMetadata** OutMetadata);

/** Releases memory allocated by EOS_PlayerDataStorage_CopyFileMetadataByIndex */
typedef void                                  (EOS_CALL *EOS_PlayerDataStorage_FileMetadata_Release_t)(EOS_PlayerDataStorage_FileMetadata* FileMetadata);

/** Begins an asynchronous file read; returns a transfer request handle */
typedef EOS_HPlayerDataStorageFileTransferRequest (EOS_CALL *EOS_PlayerDataStorage_ReadFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_ReadFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnReadFileCompleteCallback CompletionDelegate);

/** Begins an asynchronous file write; returns a transfer request handle */
typedef EOS_HPlayerDataStorageFileTransferRequest (EOS_CALL *EOS_PlayerDataStorage_WriteFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_WriteFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnWriteFileCompleteCallback CompletionDelegate);

/** Deletes a file from cloud storage */
typedef void                                  (EOS_CALL *EOS_PlayerDataStorage_DeleteFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_DeleteFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnDeleteFileCompleteCallback CompletionDelegate);

// ═════════════════════════════════════════════════════════════════════════════
//  TITLE STORAGE  (read-only, developer-managed files)
// ═════════════════════════════════════════════════════════════════════════════

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the TitleStorage handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_TitleStorageHandleDetails;
typedef struct EOS_TitleStorageHandleDetails* EOS_HTitleStorage;

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque title-storage file transfer request handle
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_TitleStorageFileTransferRequestDetails;
typedef struct EOS_TitleStorageFileTransferRequestDetails* EOS_HTitleStorageFileTransferRequest;

// ─────────────────────────────────────────────────────────────────────────────
//  TitleStorage streaming callback info structs
// ─────────────────────────────────────────────────────────────────────────────
typedef struct EOS_TitleStorage_ReadFileDataCallbackInfo
{
    void*             ClientData;
    const char*       Filename;
    uint32_t          TotalFileSizeBytes;
    EOS_Bool          bIsLastChunk;
    uint32_t          DataChunkLengthBytes;
    const void*       DataChunk;
} EOS_TitleStorage_ReadFileDataCallbackInfo;

typedef struct EOS_TitleStorage_FileTransferProgressCallbackInfo
{
    void*       ClientData;
    const char* Filename;
    uint32_t    BytesTransferred;
    uint32_t    TotalFileSizeBytes;
} EOS_TitleStorage_FileTransferProgressCallbackInfo;

typedef EOS_EPlayerDataStorageReadResult (EOS_CALL *EOS_TitleStorage_OnReadFileDataCallback)(const EOS_TitleStorage_ReadFileDataCallbackInfo* Data);
typedef void                             (EOS_CALL *EOS_TitleStorage_OnFileTransferProgressCallback)(const EOS_TitleStorage_FileTransferProgressCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_FileMetadata
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_FILEMETADATA_API_LATEST 1

typedef struct EOS_TitleStorage_FileMetadata
{
    /** API version: must be EOS_TITLESTORAGE_FILEMETADATA_API_LATEST */
    int32_t     ApiVersion;
    /** Total size of the file in bytes */
    uint32_t    FileSizeBytes;
    /** MD5 hash of the file contents (hex string) */
    const char* MD5Hash;
    /** Name of the file */
    const char* Filename;
} EOS_TitleStorage_FileMetadata;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_QueryFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_QUERYFILE_API_LATEST 1

typedef struct EOS_TitleStorage_QueryFileOptions
{
    /** API version: must be EOS_TITLESTORAGE_QUERYFILE_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user making the request */
    EOS_ProductUserId LocalUserId;
    /** Name of the file to query */
    const char*       Filename;
} EOS_TitleStorage_QueryFileOptions;

typedef struct EOS_TitleStorage_QueryFileCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    const char*       Filename;
} EOS_TitleStorage_QueryFileCallbackInfo;

typedef void (EOS_CALL *EOS_TitleStorage_OnQueryFileCompleteCallback)(const EOS_TitleStorage_QueryFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_QueryFileListOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_QUERYFILELIST_API_LATEST 1

typedef struct EOS_TitleStorage_QueryFileListOptions
{
    /** API version: must be EOS_TITLESTORAGE_QUERYFILELIST_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user making the request */
    EOS_ProductUserId LocalUserId;
    /** Array of tag strings to filter files (NULL = no tag filter) */
    const char**      ListOfTags;
    /** Number of entries in ListOfTags (0 if ListOfTags is NULL) */
    uint32_t          ListOfTagsCount;
} EOS_TitleStorage_QueryFileListOptions;

typedef struct EOS_TitleStorage_QueryFileListCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    uint32_t    FileCount;
} EOS_TitleStorage_QueryFileListCallbackInfo;

typedef void (EOS_CALL *EOS_TitleStorage_OnQueryFileListCompleteCallback)(const EOS_TitleStorage_QueryFileListCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_GetFileMetadataCountOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_GETFILEMETADATACOUNT_API_LATEST 1

typedef struct EOS_TitleStorage_GetFileMetadataCountOptions
{
    /** API version: must be EOS_TITLESTORAGE_GETFILEMETADATACOUNT_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
} EOS_TitleStorage_GetFileMetadataCountOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_CopyFileMetadataByIndexOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_COPYFILEMETADATABYINDEX_API_LATEST 1

typedef struct EOS_TitleStorage_CopyFileMetadataByIndexOptions
{
    /** API version: must be EOS_TITLESTORAGE_COPYFILEMETADATABYINDEX_API_LATEST */
    int32_t           ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId LocalUserId;
    /** Zero-based index of the file metadata entry to copy */
    uint32_t          Index;
} EOS_TitleStorage_CopyFileMetadataByIndexOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_TitleStorage_ReadFileOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_TITLESTORAGE_READFILE_API_LATEST 1

typedef struct EOS_TitleStorage_ReadFileOptions
{
    /** API version: must be EOS_TITLESTORAGE_READFILE_API_LATEST */
    int32_t                                      ApiVersion;
    /** Product User ID of the local user */
    EOS_ProductUserId                            LocalUserId;
    /** Name of the file to read */
    const char*                                  Filename;
    /** Preferred size in bytes for each data chunk */
    uint32_t                                     ReadChunkLengthBytes;
    /** Called for each chunk of data */
    EOS_TitleStorage_OnReadFileDataCallback      ReadFileDataCallback;
    /** Called periodically to report progress (may be NULL) */
    EOS_TitleStorage_OnFileTransferProgressCallback FileTransferProgressCallback;
} EOS_TitleStorage_ReadFileOptions;

typedef struct EOS_TitleStorage_ReadFileCallbackInfo
{
    EOS_EResult ResultCode;
    void*       ClientData;
    const char* Filename;
} EOS_TitleStorage_ReadFileCallbackInfo;

typedef void (EOS_CALL *EOS_TitleStorage_OnReadFileCompleteCallback)(const EOS_TitleStorage_ReadFileCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  TitleStorage interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries metadata for a single title storage file */
typedef void                              (EOS_CALL *EOS_TitleStorage_QueryFile_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_QueryFileOptions* Options, void* ClientData, EOS_TitleStorage_OnQueryFileCompleteCallback CompletionDelegate);

/** Queries metadata for all title storage files matching the optional tag list */
typedef void                              (EOS_CALL *EOS_TitleStorage_QueryFileList_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_QueryFileListOptions* Options, void* ClientData, EOS_TitleStorage_OnQueryFileListCompleteCallback CompletionDelegate);

/** Returns the number of cached title-storage file metadata entries */
typedef uint32_t                          (EOS_CALL *EOS_TitleStorage_GetFileMetadataCount_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_GetFileMetadataCountOptions* Options);

/** Copies a title-storage file metadata entry by index; release with EOS_TitleStorage_FileMetadata_Release */
typedef EOS_EResult                       (EOS_CALL *EOS_TitleStorage_CopyFileMetadataByIndex_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_CopyFileMetadataByIndexOptions* Options, EOS_TitleStorage_FileMetadata** OutMetadata);

/** Begins an asynchronous title-storage file read; returns a transfer request handle */
typedef EOS_HTitleStorageFileTransferRequest (EOS_CALL *EOS_TitleStorage_ReadFile_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_ReadFileOptions* Options, void* ClientData, EOS_TitleStorage_OnReadFileCompleteCallback CompletionDelegate);

/** Releases memory allocated by EOS_TitleStorage_CopyFileMetadataByIndex */
typedef void                              (EOS_CALL *EOS_TitleStorage_FileMetadata_Release_t)(EOS_TitleStorage_FileMetadata* FileMetadata);

#ifdef __cplusplus
}
#endif
