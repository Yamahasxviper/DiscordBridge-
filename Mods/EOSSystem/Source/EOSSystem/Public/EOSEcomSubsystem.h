// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSEcomSubsystem.generated.h"

USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSEntitlement
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, Category="EOS|Ecom") FString EntitlementName;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Ecom") FString EntitlementId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Ecom") FString ItemId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Ecom") bool    bRedeemed = false;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Ecom") int32   EndTimestamp = -1;
};

UCLASS()
class EOSSYSTEM_API UEOSEcomSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Ecom") void QueryOwnership(const FString& EpicAccountId, const TArray<FString>& CatalogItemIds);
    UFUNCTION(BlueprintCallable, Category="EOS|Ecom") void QueryEntitlements(const FString& EpicAccountId);
    UFUNCTION(BlueprintPure,     Category="EOS|Ecom") TArray<FEOSEntitlement> GetCachedEntitlements(const FString& EpicAccountId) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Ecom") int32 GetEntitlementCount(const FString& EpicAccountId) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Ecom") bool OwnsItem(const FString& EpicAccountId, const FString& CatalogItemId) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Ecom") FOnEOSOwnershipQueried OnOwnershipQueried;

private:
    EOS_HEcom GetEcomHandle() const;
    TMap<FString, TArray<FEOSEntitlement>> EntitlementsCache;
    TMap<FString, TSet<FString>> OwnershipCache; // EpicAccountId -> owned CatalogItemIds
    static void EOS_CALL OnOwnershipCb(const EOS_Ecom_QueryOwnershipCallbackInfo* D);
    static void EOS_CALL OnEntitlementsCb(const EOS_Ecom_QueryEntitlementsCallbackInfo* D);
};
