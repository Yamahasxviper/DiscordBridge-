// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSMetricsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSMetricsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Metrics") bool BeginPlayerSession(const FString& PUID, const FString& DisplayName, const FString& GameSessionId);
    UFUNCTION(BlueprintCallable, Category="EOS|Metrics") bool EndPlayerSession(const FString& PUID);
    UFUNCTION(BlueprintPure,     Category="EOS|Metrics") bool IsPlayerSessionActive(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Metrics") TArray<FString> GetActiveSessionPUIDs() const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Metrics") FOnEOSMetricsSession OnBeginPlayerSession;
    UPROPERTY(BlueprintAssignable, Category="EOS|Metrics") FOnEOSMetricsSession OnEndPlayerSession;

private:
    EOS_HMetrics GetMetricsHandle() const;
    TSet<FString> ActiveSessions;
};
