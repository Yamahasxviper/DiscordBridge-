// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSStorageSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSStorage, Log, All);

struct FPDSQueryCbData { TWeakObjectPtr<UEOSStorageSubsystem> Sub; FString PUID; EOS_ProductUserId UserId; };
struct FPDSDeleteCbData { TWeakObjectPtr<UEOSStorageSubsystem> Sub; FString PUID; FString Filename; };

void UEOSStorageSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSStorageSubsystem::Deinitialize() { PlayerFilesCache.Empty(); TitleFilesCache.Empty(); Super::Deinitialize(); }

EOS_HPlayerDataStorage UEOSStorageSubsystem::GetPDSHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetPlayerDataStorageInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetPlayerDataStorageInterface(Sys->GetPlatformHandle());
}
EOS_HTitleStorage UEOSStorageSubsystem::GetTSHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetTitleStorageInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetTitleStorageInterface(Sys->GetPlatformHandle());
}

void UEOSStorageSubsystem::QueryPlayerFileList(const FString& PUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HPlayerDataStorage H = GetPDSHandle();
    if (!H || !SDK.fp_EOS_PlayerDataStorage_QueryFileList) return;
    EOS_ProductUserId UserId = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!UserId) return;
    EOS_PlayerDataStorage_QueryFileListOptions O = {}; O.ApiVersion = EOS_PLAYERDATASTORAGE_QUERYFILELIST_API_LATEST; O.LocalUserId = UserId;
    auto* Cb = new FPDSQueryCbData{ this, PUID, UserId };
    SDK.fp_EOS_PlayerDataStorage_QueryFileList(H, &O, Cb, &UEOSStorageSubsystem::OnQueryPlayerFileListCb);
}
void UEOSStorageSubsystem::DeletePlayerFile(const FString& PUID, const FString& Filename)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HPlayerDataStorage H = GetPDSHandle();
    if (!H || !SDK.fp_EOS_PlayerDataStorage_DeleteFile) return;
    EOS_ProductUserId UserId = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!UserId) return;
    EOS_PlayerDataStorage_DeleteFileOptions O = {}; O.ApiVersion = EOS_PLAYERDATASTORAGE_DELETEFILE_API_LATEST;
    O.LocalUserId = UserId; O.Filename = TCHAR_TO_UTF8(*Filename);
    auto* Cb = new FPDSDeleteCbData{ this, PUID, Filename };
    SDK.fp_EOS_PlayerDataStorage_DeleteFile(H, &O, Cb, &UEOSStorageSubsystem::OnDeletePlayerFileCb);
}
TArray<FEOSFileInfo> UEOSStorageSubsystem::GetCachedPlayerFiles(const FString& PUID) const
{ if (const auto* P = PlayerFilesCache.Find(PUID)) return *P; return {}; }
int32 UEOSStorageSubsystem::GetPlayerFileCount(const FString& PUID) const
{ if (const auto* P = PlayerFilesCache.Find(PUID)) return P->Num(); return 0; }
void UEOSStorageSubsystem::QueryTitleFileList(const TArray<FString>& Tags)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HTitleStorage H = GetTSHandle();
    if (!H || !SDK.fp_EOS_TitleStorage_QueryFileList) return;
    TArray<const char*> T; TArray<std::string> TS;
    for (const FString& Tag : Tags) { TS.Add(TCHAR_TO_UTF8(*Tag)); T.Add(TS.Last().c_str()); }
    EOS_TitleStorage_QueryFileListOptions O = {}; O.ApiVersion = EOS_TITLESTORAGE_QUERYFILELIST_API_LATEST;
    O.LocalUserId = nullptr; O.ListOfTags = T.IsEmpty() ? nullptr : T.GetData(); O.ListOfTagsCount = (uint32_t)T.Num();
    SDK.fp_EOS_TitleStorage_QueryFileList(H, &O, this, &UEOSStorageSubsystem::OnQueryTitleFileListCb);
}
TArray<FEOSFileInfo> UEOSStorageSubsystem::GetCachedTitleFiles() const { return TitleFilesCache; }
int32 UEOSStorageSubsystem::GetTitleFileCount() const { return TitleFilesCache.Num(); }

void EOS_CALL UEOSStorageSubsystem::OnQueryPlayerFileListCb(const EOS_PlayerDataStorage_QueryFileListCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FPDSQueryCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSStorageSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString PUID = Cb->PUID; EOS_ProductUserId UserId = Cb->UserId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSStorage, Warning, TEXT("QueryPlayerFileList failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HPlayerDataStorage H = Self->GetPDSHandle();
    TArray<FEOSFileInfo> Files;
    if (H && SDK.fp_EOS_PlayerDataStorage_GetFileMetadataCount && SDK.fp_EOS_PlayerDataStorage_CopyFileMetadataByIndex)
    {
        EOS_PlayerDataStorage_GetFileMetadataCountOptions CO = {}; CO.ApiVersion = EOS_PLAYERDATASTORAGE_GETFILEMETADATACOUNT_API_LATEST; CO.LocalUserId = UserId;
        uint32_t Count = SDK.fp_EOS_PlayerDataStorage_GetFileMetadataCount(H, &CO);
        for (uint32_t i = 0; i < Count; ++i)
        {
            EOS_PlayerDataStorage_CopyFileMetadataByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_PLAYERDATASTORAGE_COPYFILEMETADATABYINDEX_API_LATEST; CopyO.LocalUserId = UserId; CopyO.Index = i;
            EOS_PlayerDataStorage_FileMetadata* M = nullptr;
            if (SDK.fp_EOS_PlayerDataStorage_CopyFileMetadataByIndex(H, &CopyO, &M) == EOS_Success && M)
            {
                FEOSFileInfo F; F.Filename = M->Filename ? UTF8_TO_TCHAR(M->Filename) : TEXT(""); F.FileSizeBytes = (int32)M->FileSizeBytes;
                F.MD5Hash = M->MD5Hash ? UTF8_TO_TCHAR(M->MD5Hash) : TEXT("");
                Files.Add(F);
                if (SDK.fp_EOS_PlayerDataStorage_FileMetadata_Release) SDK.fp_EOS_PlayerDataStorage_FileMetadata_Release(M);
            }
        }
    }
    Self->PlayerFilesCache.Add(PUID, Files);
    UE_LOG(LogEOSStorage, Log, TEXT("Player storage for %s: %d files"), *PUID, Files.Num());
    Self->OnPlayerStorageQueried.Broadcast(PUID, true);
}
void EOS_CALL UEOSStorageSubsystem::OnDeletePlayerFileCb(const EOS_PlayerDataStorage_DeleteFileCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FPDSDeleteCbData*>(D->ClientData);
    UE_LOG(LogEOSStorage, Log, TEXT("DeletePlayerFile '%s' for %s: %s"), Cb ? *Cb->Filename : TEXT("?"), Cb ? *Cb->PUID : TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb) delete Cb;
}
void EOS_CALL UEOSStorageSubsystem::OnQueryTitleFileListCb(const EOS_TitleStorage_QueryFileListCallbackInfo* D)
{
    if (!D) return;
    auto* Self = static_cast<UEOSStorageSubsystem*>(D->ClientData);
    if (!Self || D->ResultCode != EOS_Success) { UE_LOG(LogEOSStorage, Warning, TEXT("QueryTitleFileList: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HTitleStorage H = Self->GetTSHandle();
    if (!H || !SDK.fp_EOS_TitleStorage_GetFileMetadataCount || !SDK.fp_EOS_TitleStorage_CopyFileMetadataByIndex) return;
    EOS_TitleStorage_GetFileMetadataCountOptions CO = {}; CO.ApiVersion = EOS_TITLESTORAGE_GETFILEMETADATACOUNT_API_LATEST; CO.LocalUserId = nullptr;
    uint32_t Count = SDK.fp_EOS_TitleStorage_GetFileMetadataCount(H, &CO);
    Self->TitleFilesCache.Empty();
    for (uint32_t i = 0; i < Count; ++i)
    {
        EOS_TitleStorage_CopyFileMetadataByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_TITLESTORAGE_COPYFILEMETADATABYINDEX_API_LATEST; CopyO.Index = i; CopyO.LocalUserId = nullptr;
        EOS_TitleStorage_FileMetadata* M = nullptr;
        if (SDK.fp_EOS_TitleStorage_CopyFileMetadataByIndex(H, &CopyO, &M) == EOS_Success && M)
        {
            FEOSFileInfo F; F.Filename = M->Filename ? UTF8_TO_TCHAR(M->Filename) : TEXT(""); F.FileSizeBytes = (int32)M->FileSizeBytes;
            Self->TitleFilesCache.Add(F);
            if (SDK.fp_EOS_TitleStorage_FileMetadata_Release) SDK.fp_EOS_TitleStorage_FileMetadata_Release(M);
        }
    }
    UE_LOG(LogEOSStorage, Log, TEXT("Title storage: %d files"), Self->TitleFilesCache.Num());
    Self->OnTitleStorageQueried.Broadcast(TEXT("TitleStorage"), true);
}
