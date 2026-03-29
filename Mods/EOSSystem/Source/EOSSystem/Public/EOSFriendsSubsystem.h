// Copyright Yamahasxviper. All Rights Reserved.
#pragma once
#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSFriendsSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSFriendsSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="EOS|Friends") void QueryFriends(const FString& EpicAccountId);
    UFUNCTION(BlueprintPure,     Category="EOS|Friends") int32 GetFriendsCount(const FString& EpicAccountId) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Friends") FString GetFriendAtIndex(const FString& EpicAccountId, int32 Index) const;
    UFUNCTION(BlueprintPure,     Category="EOS|Friends") FString GetFriendStatus(const FString& EpicAccountId, const FString& FriendEpicAccountId) const;
    UFUNCTION(BlueprintCallable, Category="EOS|Friends") void SendFriendInvite(const FString& LocalEpicAccountId, const FString& TargetEpicAccountId);
    UFUNCTION(BlueprintCallable, Category="EOS|Friends") void AcceptFriendInvite(const FString& LocalEpicAccountId, const FString& TargetEpicAccountId);
    UFUNCTION(BlueprintCallable, Category="EOS|Friends") void RejectFriendInvite(const FString& LocalEpicAccountId, const FString& TargetEpicAccountId);

    UPROPERTY(BlueprintAssignable, Category="EOS|Friends") FOnEOSFriendsQueried OnFriendsQueried;

private:
    EOS_HFriends GetFriendsHandle() const;
    TMap<FString, EOS_EpicAccountId> AccountIdCache;
    EOS_NotificationId FriendUpdateNotif = EOS_INVALID_NOTIFICATIONID;
    static void EOS_CALL OnQueryFriendsCb(const EOS_Friends_QueryFriendsCallbackInfo* D);
    static void EOS_CALL OnSendInviteCb(const EOS_Friends_SendInviteCallbackInfo* D);
    static void EOS_CALL OnAcceptInviteCb(const EOS_Friends_AcceptInviteCallbackInfo* D);
    static void EOS_CALL OnRejectInviteCb(const EOS_Friends_RejectInviteCallbackInfo* D);
    static void EOS_CALL OnFriendsUpdateCb(const EOS_Friends_OnFriendsUpdateInfo* D);
};
