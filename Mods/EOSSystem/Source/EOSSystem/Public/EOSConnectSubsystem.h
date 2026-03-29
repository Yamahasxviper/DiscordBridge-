// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSConnectSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSConnectSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // Server device-auth login
    UFUNCTION(BlueprintCallable, Category="EOS|Connect") void LoginAsServer();

    // Enumerate all EOS-logged-in PUIDs
    UFUNCTION(BlueprintPure,     Category="EOS|Connect") TArray<FString> GetLoggedInPUIDs() const;
    UFUNCTION(BlueprintPure,     Category="EOS|Connect") FString         GetLocalServerPUID() const { return LocalServerPUID; }

    // Manually track player PUIDs (complement to FGameModeEvents auto-tracking)
    UFUNCTION(BlueprintCallable, Category="EOS|Connect") void RegisterPlayerPUID(const FString& PUID);
    UFUNCTION(BlueprintCallable, Category="EOS|Connect") void UnregisterPlayerPUID(const FString& PUID);
    UFUNCTION(BlueprintPure,     Category="EOS|Connect") TArray<FString> GetTrackedPlayerPUIDs() const { return TrackedPUIDs; }

    // ── PUID lookups: external account ID → EOS PUID ─────────────────────
    // Queries EOS to map an external account ID (e.g. Steam64 decimal string) to an EOS PUID.
    // ExternalAccountType: 1=Steam, 4=Discord, 0=Epic, 5=GOG, etc. (matches EOS_EExternalAccountType enum)
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void LookupPUIDByExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType);

    // After QueryExternalAccountMappings completes, retrieve the cached PUID for an external account.
    UFUNCTION(BlueprintPure, Category="EOS|Connect|Lookup")
    FString GetCachedPUIDForExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType) const;

    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPlayerRegistered   OnPlayerPUIDRegistered;
    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPlayerUnregistered OnPlayerPUIDUnregistered;
    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPUIDLookupComplete OnPUIDLookupComplete;

    EOS_HConnect GetConnectHandle() const;

private:
    TArray<FString> TrackedPUIDs;
    FString         LocalServerPUID;
    EOS_NotificationId AuthExpiryNotif = EOS_INVALID_NOTIFICATIONID;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;

    void PerformConnectLogin();
    void HandlePostLogin(AGameModeBase* GM, APlayerController* PC);
    void HandleLogout(AGameModeBase* GM, AController* C);

    static void EOS_CALL OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data);
    static void EOS_CALL OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
    static void EOS_CALL OnAuthExpirationCallback(const EOS_Connect_AuthExpirationCallbackInfo* Data);
    static void EOS_CALL OnQueryExternalMappingsCallback(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data);
};
