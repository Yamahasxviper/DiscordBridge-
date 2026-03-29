// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSLeaderboardsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSLeaderboardsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Leaderboards") void QueryLeaderboardDefinitions();
    UFUNCTION(BlueprintCallable, Category="EOS|Leaderboards") void QueryLeaderboardRanks(const FString& LeaderboardId);
    UFUNCTION(BlueprintCallable, Category="EOS|Leaderboards") void QueryUserScores(const TArray<FString>& PUIDs, const FString& StatName);
    UFUNCTION(BlueprintPure,     Category="EOS|Leaderboards") TArray<FEOSLeaderboardRecord> GetCachedRanks(const FString& LeaderboardId) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Leaderboards") int32 GetCachedRecordCount(const FString& LeaderboardId) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Leaderboards") FOnEOSLeaderboardQueried OnLeaderboardQueried;

private:
    EOS_HLeaderboards GetLBHandle() const;
    TMap<FString, TArray<FEOSLeaderboardRecord>> RecordsCache;
    static void EOS_CALL OnQueryRanksCb(const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* D);
    static void EOS_CALL OnQueryDefsCb(const EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo* D);
    static void EOS_CALL OnQueryScoresCb(const EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo* D);
};
