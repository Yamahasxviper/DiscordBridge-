// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSStorageSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSStorageSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Player Data Storage ───────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="EOS|Storage|Player") void QueryPlayerFileList(const FString& PUID);
    UFUNCTION(BlueprintCallable, Category="EOS|Storage|Player") void DeletePlayerFile(const FString& PUID, const FString& Filename);
    UFUNCTION(BlueprintPure,     Category="EOS|Storage|Player") TArray<FEOSFileInfo> GetCachedPlayerFiles(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Storage|Player") int32 GetPlayerFileCount(const FString& PUID) const;

    // ── Title Storage ─────────────────────────────────────────────────────
    UFUNCTION(BlueprintCallable, Category="EOS|Storage|Title") void QueryTitleFileList(const TArray<FString>& Tags);
    UFUNCTION(BlueprintPure,     Category="EOS|Storage|Title") TArray<FEOSFileInfo> GetCachedTitleFiles() const;
    UFUNCTION(BlueprintPure,     Category="EOS|Storage|Title") int32 GetTitleFileCount() const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Storage") FOnEOSStorageQueried OnPlayerStorageQueried;
    UPROPERTY(BlueprintAssignable, Category="EOS|Storage") FOnEOSStorageQueried OnTitleStorageQueried;

private:
    EOS_HPlayerDataStorage GetPDSHandle() const;
    EOS_HTitleStorage      GetTSHandle()  const;
    TMap<FString, TArray<FEOSFileInfo>> PlayerFilesCache;
    TArray<FEOSFileInfo>   TitleFilesCache;
    static void EOS_CALL OnQueryPlayerFileListCb(const EOS_PlayerDataStorage_QueryFileListCallbackInfo* D);
    static void EOS_CALL OnDeletePlayerFileCb(const EOS_PlayerDataStorage_DeleteFileCallbackInfo* D);
    static void EOS_CALL OnQueryTitleFileListCb(const EOS_TitleStorage_QueryFileListCallbackInfo* D);
};
