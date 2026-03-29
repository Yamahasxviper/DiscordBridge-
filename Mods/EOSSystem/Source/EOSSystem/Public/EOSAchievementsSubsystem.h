// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSAchievementsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSAchievementsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Achievements") void UnlockAchievements(const FString& PUID, const TArray<FString>& AchievementIds);
    UFUNCTION(BlueprintCallable, Category="EOS|Achievements") void QueryPlayerAchievements(const FString& PUID);
    UFUNCTION(BlueprintPure,     Category="EOS|Achievements") TArray<FEOSAchievementInfo> GetCachedAchievements(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Achievements") bool HasUnlockedAchievement(const FString& PUID, const FString& AchievementId) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Achievements") FOnEOSAchievementUnlocked OnAchievementUnlocked;

private:
    EOS_HAchievements GetAchievementsHandle() const;
    TMap<FString, TArray<FEOSAchievementInfo>> Cache;
    static void EOS_CALL OnUnlockCb(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* D);
    static void EOS_CALL OnQueryCb(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* D);
};
