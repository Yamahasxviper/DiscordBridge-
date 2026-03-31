// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_storage.h — delegates to the real EOS SDK eos_playerdatastorage.h and
// eos_titlestorage.h, then adds _t function-pointer typedefs required by
// FEOSSDKLoader.

#pragma once

// Ensure EOS_EResult, EOS_CALL, and other primitive EOS types are available
// even when the real SDK headers omit eos_common.h from their transitive
// includes (as seen in some CSS UE5.3.2 engine EOSSDK distributions).
#include "eos_common.h"

#if defined(__has_include) && __has_include(<eos_playerdatastorage.h>)
#  include <eos_playerdatastorage.h>
#endif

#if defined(__has_include) && __has_include(<eos_titlestorage.h>)
#  include <eos_titlestorage.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback forward declarations for CSS engine EOS SDK.
//  The CSS UE5.3.2 engine may ship an EOS SDK that omits the FileMetadata struct
//  definitions used as out-parameters in the CopyFileMetadataByIndex functions.
//  Guard on the API version constants defined alongside those structs in the
//  real SDK so that we forward-declare only when the structs are absent.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_PLAYERDATASTORAGE_FILEMETADATA_API_LATEST
struct EOS_PlayerDataStorage_FileMetadata;
#endif // EOS_PLAYERDATASTORAGE_FILEMETADATA_API_LATEST

#ifndef EOS_TITLESTORAGE_FILEMETADATA_API_LATEST
struct EOS_TitleStorage_FileMetadata;
#endif // EOS_TITLESTORAGE_FILEMETADATA_API_LATEST

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback forward declarations for CopyFileMetadataByIndex options structs.
//  The CSS UE5.3.2 engine may ship an EOS SDK that omits the CopyFileMetadataByIndex
//  function and its associated options struct (added in later EOS SDK versions).
//  Guard on the API version constant defined alongside those options structs in
//  the real SDK so that we forward-declare only when the types are absent.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYINDEX_API_LATEST
struct EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions;
#endif // EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYINDEX_API_LATEST

#ifndef EOS_TITLESTORAGE_COPYFILEMETADATABYINDEX_API_LATEST
struct EOS_TitleStorage_CopyFileMetadataByIndexOptions;
#endif // EOS_TITLESTORAGE_COPYFILEMETADATABYINDEX_API_LATEST

// ─────────────────────────────────────────────────────────────────────────────
//  PlayerDataStorage interface function pointer typedefs
//  These _t aliases are used by FEOSSDKLoader for dynamic DLL symbol loading.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_PlayerDataStorage_QueryFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnQueryFileCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_PlayerDataStorage_QueryFileList_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_QueryFileListOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnQueryFileListCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_PlayerDataStorage_GetFileMetadataCount_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_GetFileMetadataCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_PlayerDataStorage_CopyFileMetadataByIndex_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions* Options, EOS_PlayerDataStorage_FileMetadata** OutMetadata);
typedef void        (EOS_CALL *EOS_PlayerDataStorage_FileMetadata_Release_t)(EOS_PlayerDataStorage_FileMetadata* FileMetadata);
typedef EOS_HPlayerDataStorageFileTransferRequest (EOS_CALL *EOS_PlayerDataStorage_ReadFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_ReadFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnReadFileCompleteCallback CompletionDelegate);
typedef EOS_HPlayerDataStorageFileTransferRequest (EOS_CALL *EOS_PlayerDataStorage_WriteFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_WriteFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnWriteFileCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_PlayerDataStorage_DeleteFile_t)(EOS_HPlayerDataStorage Handle, const EOS_PlayerDataStorage_DeleteFileOptions* Options, void* ClientData, EOS_PlayerDataStorage_OnDeleteFileCompleteCallback CompletionDelegate);

// ─────────────────────────────────────────────────────────────────────────────
//  TitleStorage interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_TitleStorage_QueryFile_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_QueryFileOptions* Options, void* ClientData, EOS_TitleStorage_OnQueryFileCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_TitleStorage_QueryFileList_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_QueryFileListOptions* Options, void* ClientData, EOS_TitleStorage_OnQueryFileListCompleteCallback CompletionDelegate);
typedef uint32_t    (EOS_CALL *EOS_TitleStorage_GetFileMetadataCount_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_GetFileMetadataCountOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_TitleStorage_CopyFileMetadataByIndex_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_CopyFileMetadataByIndexOptions* Options, EOS_TitleStorage_FileMetadata** OutMetadata);
typedef EOS_HTitleStorageFileTransferRequest (EOS_CALL *EOS_TitleStorage_ReadFile_t)(EOS_HTitleStorage Handle, const EOS_TitleStorage_ReadFileOptions* Options, void* ClientData, EOS_TitleStorage_OnReadFileCompleteCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_TitleStorage_FileMetadata_Release_t)(EOS_TitleStorage_FileMetadata* FileMetadata);
