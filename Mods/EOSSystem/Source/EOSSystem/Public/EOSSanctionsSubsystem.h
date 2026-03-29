// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSSanctionsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSSanctionsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Sanctions") void QuerySanctions(const FString& TargetPUID);
    UFUNCTION(BlueprintPure,     Category="EOS|Sanctions") TArray<FEOSSanctionInfo> GetCachedSanctions(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Sanctions") bool HasActiveSanction(const FString& PUID) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Sanctions") int32 GetSanctionCount(const FString& PUID) const;
    UFUNCTION(BlueprintCallable, Category="EOS|Sanctions") void ClearCachedSanctions(const FString& PUID);

    UPROPERTY(BlueprintAssignable, Category="EOS|Sanctions") FOnEOSSanctionsQueried OnSanctionsQueried;

private:
    EOS_HSanctions GetSanctionsHandle() const;
    TMap<FString, TArray<FEOSSanctionInfo>> SanctionCache;
    static void EOS_CALL OnQueryCb(const EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo* D);
};
