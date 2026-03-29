// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSReportsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSReportsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void SendReport(const FString& ReporterPUID, const FString& ReportedPUID, const FString& Category, const FString& Message, const FString& Context);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportCheating    (const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportVerbalAbuse (const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportScamming    (const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportExploiting  (const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportSpamming    (const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);
    UFUNCTION(BlueprintCallable, Category="EOS|Reports") void ReportOffensiveProfile(const FString& ReporterPUID, const FString& ReportedPUID, const FString& Message);

    UPROPERTY(BlueprintAssignable, Category="EOS|Reports") FOnEOSReportSent OnReportSent;

private:
    EOS_HReports GetReportsHandle() const;
    static EOS_EPlayerReportsCategory ParseCategory(const FString& Cat);
    static void EOS_CALL OnReportSentCb(const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* D);
};
