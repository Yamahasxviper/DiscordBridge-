// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSPresenceSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSPresenceSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Presence") void QueryPresence(const FString& LocalEpicAccountId, const FString& TargetEpicAccountId);
    UFUNCTION(BlueprintCallable, Category="EOS|Presence") void SetStatus(const FString& LocalEpicAccountId, const FString& Status);
    UFUNCTION(BlueprintCallable, Category="EOS|Presence") void SetRichText(const FString& LocalEpicAccountId, const FString& RichText);
    UFUNCTION(BlueprintCallable, Category="EOS|Presence") void SetData(const FString& LocalEpicAccountId, const TMap<FString,FString>& KeyValues);
    UFUNCTION(BlueprintCallable, Category="EOS|Presence") void SetStatusAndCommit(const FString& LocalEpicAccountId, const FString& Status, const FString& RichText, const TMap<FString,FString>& Data);

    UPROPERTY(BlueprintAssignable, Category="EOS|Presence") FOnEOSPresenceSet OnPresenceSet;
    UPROPERTY(BlueprintAssignable, Category="EOS|Presence") FOnEOSUserInfoQueried OnPresenceQueried;

private:
    EOS_HPresence GetPresenceHandle() const;
    static void EOS_CALL OnQueryPresenceCb(const EOS_Presence_QueryPresenceCallbackInfo* D);
    static void EOS_CALL OnSetPresenceCb(const EOS_Presence_SetPresenceCallbackInfo* D);
};
