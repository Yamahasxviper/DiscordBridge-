// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSAntiCheatSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSAntiCheatSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") bool BeginSession();
    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") void EndSession();
    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") void RegisterClient(const FString& PUID, const FString& IpAddress);
    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") void UnregisterClient(const FString& PUID);
    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") bool ProtectMessage(const FString& PUID, const TArray<uint8>& In, TArray<uint8>& Out);
    UFUNCTION(BlueprintCallable, Category="EOS|AntiCheat") bool UnprotectMessage(const FString& PUID, const TArray<uint8>& In, TArray<uint8>& Out);
    UFUNCTION(BlueprintPure,     Category="EOS|AntiCheat") bool IsSessionActive() const { return bSessionActive; }
    UFUNCTION(BlueprintPure,     Category="EOS|AntiCheat") TArray<FString> GetRegisteredClients() const;

    UPROPERTY(BlueprintAssignable, Category="EOS|AntiCheat") FOnEOSPlayerRegistered   OnClientRegistered;
    UPROPERTY(BlueprintAssignable, Category="EOS|AntiCheat") FOnEOSPlayerUnregistered OnClientUnregistered;

private:
    EOS_HAntiCheatServer GetACHandle() const;
    bool bSessionActive = false;
    TMap<FString, EOS_AntiCheatCommon_ClientHandle> RegisteredClients;
    int32 NextHandleVal = 1;
    EOS_NotificationId MsgNotifId    = EOS_INVALID_NOTIFICATIONID;
    EOS_NotificationId ActionNotifId = EOS_INVALID_NOTIFICATIONID;
    static void EOS_CALL OnMessageToClient(const EOS_AntiCheatServer_MessageToClientCallbackInfo*);
    static void EOS_CALL OnClientActionRequired(const EOS_AntiCheatServer_ClientActionRequiredCallbackInfo*);
};
