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
    // Queries EOS to map a single external account ID to an EOS PUID.
    // Use the typed EEOSExternalAccountType overload from Blueprint; the int32 overload is
    // provided for code callers who already have the raw enum value.
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void LookupPUIDByExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType);

    // Typed Blueprint overload — no magic numbers needed in BP graphs.
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void LookupPUIDByExternalAccountTyped(const FString& ExternalAccountId, EEOSExternalAccountType AccountType);

    // Convenience: look up an EOS PUID from a Steam64 ID string (decimal or hex).
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void LookupPUIDBySteam64(const FString& Steam64Id);

    // Batch: look up multiple external accounts of the same type in a single EOS call (≤128 IDs).
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void LookupPUIDBatch(const TArray<FString>& ExternalAccountIds, EEOSExternalAccountType AccountType);

    // After QueryExternalAccountMappings completes, retrieve the cached PUID for one external account.
    UFUNCTION(BlueprintPure, Category="EOS|Connect|Lookup")
    FString GetCachedPUIDForExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType) const;

    // Typed Blueprint version of GetCachedPUIDForExternalAccount.
    UFUNCTION(BlueprintPure, Category="EOS|Connect|Lookup")
    FString GetCachedPUIDForExternalAccountTyped(const FString& ExternalAccountId, EEOSExternalAccountType AccountType) const;

    // ── Reverse lookup: EOS PUID → external account IDs ──────────────────
    // Queries EOS to discover which external accounts (Steam, Epic, etc.) are
    // linked to the given PUID.  Results fire OnReverseLookupComplete per account.
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void QueryExternalAccountsForPUID(const FString& PUID);

    // Batch reverse lookup: look up external accounts for multiple PUIDs in one call (≤128).
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Lookup")
    void QueryExternalAccountsForPUIDBatch(const TArray<FString>& PUIDs);

    // Returns all cached external account infos linked to the given PUID (populated after
    // QueryExternalAccountsForPUID completes).
    UFUNCTION(BlueprintPure, Category="EOS|Connect|Lookup")
    TArray<FEOSExternalAccountInfo> GetCachedExternalAccountsForPUID(const FString& PUID) const;

    // Convenience: return the Steam64 ID (decimal string) linked to a PUID, or "" if not cached.
    UFUNCTION(BlueprintPure, Category="EOS|Connect|Lookup")
    FString GetCachedSteam64ForPUID(const FString& PUID) const;

    // ── ID cache persistence ──────────────────────────────────────────────
    // The reverse-lookup cache (PUID → external accounts) is automatically
    // saved to disk on Deinitialize and loaded back on Initialize so that
    // Steam64↔PUID mappings survive server restarts without re-querying EOS.
    // Call this explicitly if you need an immediate flush (e.g. after a bulk
    // reverse lookup finishes and you want to guarantee persistence).
    UFUNCTION(BlueprintCallable, Category="EOS|Connect|Cache")
    void SaveIdCache();

    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPlayerRegistered   OnPlayerPUIDRegistered;
    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPlayerUnregistered OnPlayerPUIDUnregistered;
    // Forward lookup: fires with (ExternalAccountId, PUID) when LookupPUIDByExternalAccount* completes.
    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSPUIDLookupComplete  OnPUIDLookupComplete;
    // Reverse lookup: fires with (PUID, ExternalAccountId, AccountType) once per linked account.
    UPROPERTY(BlueprintAssignable, Category="EOS|Connect") FOnEOSReverseLookupComplete OnReverseLookupComplete;

    EOS_HConnect GetConnectHandle() const;

private:
    TArray<FString> TrackedPUIDs;
    FString         LocalServerPUID;
    EOS_NotificationId AuthExpiryNotif = EOS_INVALID_NOTIFICATIONID;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;
    // Cache: PUID → linked external accounts (populated by QueryExternalAccountsForPUID)
    TMap<FString, TArray<FEOSExternalAccountInfo>> ReverseLookupCache;

    void PerformConnectLogin();
    void HandlePostLogin(AGameModeBase* GM, APlayerController* PC);
    void HandleLogout(AGameModeBase* GM, AController* C);
    // Internal registration: broadcasts OnPlayerPUIDRegistered with the associated
    // PlayerController (may be nullptr when called from the public UFUNCTION path).
    void RegisterPlayerPUIDInternal(const FString& PUID, APlayerController* PC);
    void InternalLookupBatch(const TArray<FString>& ExternalAccountIds, EOS_EExternalAccountType AccountIdType);
    void InternalReverseLookupBatch(const TArray<FString>& PUIDs);
    // JSON cache persistence helpers (use Json + JsonUtilities modules)
    void SaveCacheToDisk() const;
    void LoadCacheFromDisk();

    static void EOS_CALL OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data);
    static void EOS_CALL OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data);
    static void EOS_CALL OnAuthExpirationCallback(const EOS_Connect_AuthExpirationCallbackInfo* Data);
    static void EOS_CALL OnQueryExternalMappingsCallback(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data);
    static void EOS_CALL OnQueryProductUserIdMappingsCallback(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data);
};
