// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSUserInfoSubsystem.generated.h"

USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSUserInfo
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category="EOS|UserInfo") FString DisplayName;
    UPROPERTY(BlueprintReadWrite, Category="EOS|UserInfo") FString Nickname;
    UPROPERTY(BlueprintReadWrite, Category="EOS|UserInfo") FString PreferredLanguage;
    UPROPERTY(BlueprintReadWrite, Category="EOS|UserInfo") FString Country;
};

UCLASS()
class EOSSYSTEM_API UEOSUserInfoSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|UserInfo") void QueryUserInfo(const FString& LocalEpicAccountId, const FString& TargetEpicAccountId);
    UFUNCTION(BlueprintCallable, Category="EOS|UserInfo") void QueryUserInfoByDisplayName(const FString& LocalEpicAccountId, const FString& DisplayName);
    UFUNCTION(BlueprintPure,     Category="EOS|UserInfo") FEOSUserInfo GetCachedUserInfo(const FString& TargetEpicAccountId) const;
    UFUNCTION(BlueprintPure,     Category="EOS|UserInfo") bool HasCachedUserInfo(const FString& TargetEpicAccountId) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|UserInfo") FOnEOSUserInfoQueried OnUserInfoQueried;

private:
    EOS_HUserInfo GetUserInfoHandle() const;
    TMap<FString, FEOSUserInfo> UserInfoCache;
    static void CopyAndCacheUserInfo(UEOSUserInfoSubsystem* Self, EOS_HUserInfo H, const FString& LocalStr, const FString& TargetStr, EOS_EpicAccountId LocalId, EOS_EpicAccountId TargetId);
    static void EOS_CALL OnQueryUserInfoCb(const EOS_UserInfo_QueryUserInfoCallbackInfo* D);
    static void EOS_CALL OnQueryByDisplayNameCb(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* D);
};
