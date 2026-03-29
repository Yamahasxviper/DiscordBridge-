// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSSessionsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSSessionsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void CreateOrUpdateSession();
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void StartSession();
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void EndSession();
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void DestroySession();
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void RegisterPlayers(const TArray<FString>& PUIDs);
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void UnregisterPlayers(const TArray<FString>& PUIDs);
    UFUNCTION(BlueprintCallable, Category="EOS|Sessions") void SetAttribute(const FString& Key, const FString& Value);
    UFUNCTION(BlueprintPure,     Category="EOS|Sessions") FEOSSessionInfo GetCurrentSessionInfo() const { return CurrentSession; }
    UFUNCTION(BlueprintPure,     Category="EOS|Sessions") bool HasActiveSession() const { return bSessionExists; }

    UPROPERTY(BlueprintAssignable, Category="EOS|Sessions") FOnEOSSessionUpdated OnSessionUpdated;

private:
    EOS_HSessions GetSessionsHandle() const;
    FEOSSessionInfo CurrentSession;
    bool bSessionExists = false;
    TArray<TPair<FString,FString>> PendingAttributes;
    static void EOS_CALL OnUpdateSessionCb(const EOS_Sessions_UpdateSessionCallbackInfo*);
    static void EOS_CALL OnDestroySessionCb(const EOS_Sessions_DestroySessionCallbackInfo*);
    static void EOS_CALL OnStartSessionCb(const EOS_Sessions_StartSessionCallbackInfo*);
    static void EOS_CALL OnEndSessionCb(const EOS_Sessions_EndSessionCallbackInfo*);
    static void EOS_CALL OnRegisterPlayersCb(const EOS_Sessions_RegisterPlayersCallbackInfo*);
    static void EOS_CALL OnUnregisterPlayersCb(const EOS_Sessions_UnregisterPlayersCallbackInfo*);
};
