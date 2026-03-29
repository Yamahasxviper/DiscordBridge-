// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSStatsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSStatsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Stats") void IngestStat(const FString& PUID, const FString& StatName, int32 Amount);
    UFUNCTION(BlueprintCallable, Category="EOS|Stats") void IngestStats(const FString& PUID, const TMap<FString,int32>& Stats);
    UFUNCTION(BlueprintCallable, Category="EOS|Stats") void QueryStats(const FString& PUID);
    UFUNCTION(BlueprintPure,     Category="EOS|Stats") TArray<FEOSStatInfo> GetCachedStats(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Stats") int32 GetCachedStatValue(const FString& PUID, const FString& StatName) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Stats") FOnEOSStatIngested OnStatIngested;

private:
    EOS_HStats GetStatsHandle() const;
    TMap<FString, TArray<FEOSStatInfo>> StatsCache;
    static void EOS_CALL OnIngestCb(const EOS_Stats_IngestStatCompleteCallbackInfo* D);
    static void EOS_CALL OnQueryCb(const EOS_Stats_OnQueryStatsCompleteCallbackInfo* D);
};
