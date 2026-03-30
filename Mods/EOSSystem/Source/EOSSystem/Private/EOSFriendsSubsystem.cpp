// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSFriendsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSFriends, Log, All);

struct FFriendQueryCbData { TWeakObjectPtr<UEOSFriendsSubsystem> Sub; FString EpicAccountId; EOS_EpicAccountId AccountId; };
struct FFriendInviteCbData { FString Op; FString TargetId; };

void UEOSFriendsSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSFriendsSubsystem::Deinitialize()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (H && FriendUpdateNotif != EOS_INVALID_NOTIFICATIONID && SDK.fp_EOS_Friends_RemoveNotifyFriendsUpdate)
        SDK.fp_EOS_Friends_RemoveNotifyFriendsUpdate(H, FriendUpdateNotif);
    Super::Deinitialize();
}

EOS_HFriends UEOSFriendsSubsystem::GetFriendsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetFriendsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetFriendsInterface(Sys->GetPlatformHandle());
}

static EOS_EpicAccountId Friends_AccountFromStr(const FString& S) { FEOSSDKLoader& SDK = FEOSSDKLoader::Get(); return SDK.fp_EOS_EpicAccountId_FromString ? SDK.fp_EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*S)) : nullptr; }
static FString AccountToStr(EOS_EpicAccountId Id) { FEOSSDKLoader& SDK = FEOSSDKLoader::Get(); if (!Id || !SDK.fp_EOS_EpicAccountId_ToString) return FString(); char Buf[64]={}; int32_t Len=64; if (SDK.fp_EOS_EpicAccountId_ToString(Id, Buf, &Len)==EOS_Success) return UTF8_TO_TCHAR(Buf); return FString(); }

void UEOSFriendsSubsystem::QueryFriends(const FString& EpicAccountId)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_QueryFriends) return;
    EOS_EpicAccountId AccountId = Friends_AccountFromStr(EpicAccountId);
    if (!AccountId) return;
    EOS_Friends_QueryFriendsOptions O = {}; O.ApiVersion = EOS_FRIENDS_QUERYFRIENDS_API_LATEST; O.LocalUserId = AccountId;
    auto* Cb = new FFriendQueryCbData{ this, EpicAccountId, AccountId };
    SDK.fp_EOS_Friends_QueryFriends(H, &O, Cb, &UEOSFriendsSubsystem::OnQueryFriendsCb);
    if (FriendUpdateNotif == EOS_INVALID_NOTIFICATIONID && SDK.fp_EOS_Friends_AddNotifyFriendsUpdate)
    {
        EOS_Friends_AddNotifyFriendsUpdateOptions NO = {}; NO.ApiVersion = EOS_FRIENDS_ADDNOTIFYFRIENDSUPDATE_API_LATEST;
        FriendUpdateNotif = SDK.fp_EOS_Friends_AddNotifyFriendsUpdate(H, &NO, this, &UEOSFriendsSubsystem::OnFriendsUpdateCb);
    }
}

int32 UEOSFriendsSubsystem::GetFriendsCount(const FString& EpicAccountId) const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_GetFriendsCount) return 0;
    EOS_EpicAccountId AccountId = Friends_AccountFromStr(EpicAccountId);
    if (!AccountId) return 0;
    EOS_Friends_GetFriendsCountOptions O = {}; O.ApiVersion = EOS_FRIENDS_GETFRIENDSCOUNT_API_LATEST; O.LocalUserId = AccountId;
    return SDK.fp_EOS_Friends_GetFriendsCount(H, &O);
}

FString UEOSFriendsSubsystem::GetFriendAtIndex(const FString& EpicAccountId, int32 Index) const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_GetFriendAtIndex) return FString();
    EOS_EpicAccountId AccountId = Friends_AccountFromStr(EpicAccountId); if (!AccountId) return FString();
    EOS_Friends_GetFriendAtIndexOptions O = {}; O.ApiVersion = EOS_FRIENDS_GETFRIENDATINDEX_API_LATEST; O.LocalUserId = AccountId; O.Index = (int32_t)Index;
    return AccountToStr(SDK.fp_EOS_Friends_GetFriendAtIndex(H, &O));
}

FString UEOSFriendsSubsystem::GetFriendStatus(const FString& Local, const FString& Target) const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_GetStatus) return TEXT("Unknown");
    EOS_EpicAccountId LocalId = Friends_AccountFromStr(Local); EOS_EpicAccountId TargetId = Friends_AccountFromStr(Target);
    if (!LocalId || !TargetId) return TEXT("Unknown");
    EOS_Friends_GetStatusOptions O = {}; O.ApiVersion = EOS_FRIENDS_GETSTATUS_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    EOS_EFriendsStatus S = SDK.fp_EOS_Friends_GetStatus(H, &O);
    switch (S)
    {
        case EOS_FS_NotFriends:    return TEXT("NotFriends");
        case EOS_FS_InviteSent:    return TEXT("InviteSent");
        case EOS_FS_InviteReceived:return TEXT("InviteReceived");
        case EOS_FS_Friends:       return TEXT("Friends");
        default:                   return TEXT("Unknown");
    }
}

void UEOSFriendsSubsystem::SendFriendInvite(const FString& Local, const FString& Target)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_SendInvite) return;
    EOS_EpicAccountId LocalId = Friends_AccountFromStr(Local); EOS_EpicAccountId TargetId = Friends_AccountFromStr(Target);
    if (!LocalId || !TargetId) return;
    EOS_Friends_SendInviteOptions O = {}; O.ApiVersion = EOS_FRIENDS_SENDINVITE_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    auto* Cb = new FFriendInviteCbData{TEXT("Send"), Target};
    SDK.fp_EOS_Friends_SendInvite(H, &O, Cb, &UEOSFriendsSubsystem::OnSendInviteCb);
}
void UEOSFriendsSubsystem::AcceptFriendInvite(const FString& Local, const FString& Target)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_AcceptInvite) return;
    EOS_EpicAccountId LocalId = Friends_AccountFromStr(Local); EOS_EpicAccountId TargetId = Friends_AccountFromStr(Target);
    if (!LocalId || !TargetId) return;
    EOS_Friends_AcceptInviteOptions O = {}; O.ApiVersion = EOS_FRIENDS_ACCEPTINVITE_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    auto* Cb = new FFriendInviteCbData{TEXT("Accept"), Target};
    SDK.fp_EOS_Friends_AcceptInvite(H, &O, Cb, &UEOSFriendsSubsystem::OnAcceptInviteCb);
}
void UEOSFriendsSubsystem::RejectFriendInvite(const FString& Local, const FString& Target)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HFriends H = GetFriendsHandle();
    if (!H || !SDK.fp_EOS_Friends_RejectInvite) return;
    EOS_EpicAccountId LocalId = Friends_AccountFromStr(Local); EOS_EpicAccountId TargetId = Friends_AccountFromStr(Target);
    if (!LocalId || !TargetId) return;
    EOS_Friends_RejectInviteOptions O = {}; O.ApiVersion = EOS_FRIENDS_REJECTINVITE_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    auto* Cb = new FFriendInviteCbData{TEXT("Reject"), Target};
    SDK.fp_EOS_Friends_RejectInvite(H, &O, Cb, &UEOSFriendsSubsystem::OnRejectInviteCb);
}

void EOS_CALL UEOSFriendsSubsystem::OnQueryFriendsCb(const EOS_Friends_QueryFriendsCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FFriendQueryCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSFriendsSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString AccountId = Cb->EpicAccountId; delete Cb;
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSFriends, Log, TEXT("QueryFriends for %s: %s"), *AccountId, *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Self) Self->OnFriendsQueried.Broadcast(AccountId, bOK);
}
void EOS_CALL UEOSFriendsSubsystem::OnSendInviteCb(const EOS_Friends_SendInviteCallbackInfo* D)
{ if (D) { auto* Cb = static_cast<FFriendInviteCbData*>(D->ClientData); UE_LOG(LogEOSFriends, Log, TEXT("SendInvite to %s: %s"), Cb?*Cb->TargetId:TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode)); if (Cb) delete Cb; } }
void EOS_CALL UEOSFriendsSubsystem::OnAcceptInviteCb(const EOS_Friends_AcceptInviteCallbackInfo* D)
{ if (D) { auto* Cb = static_cast<FFriendInviteCbData*>(D->ClientData); UE_LOG(LogEOSFriends, Log, TEXT("AcceptInvite for %s: %s"), Cb?*Cb->TargetId:TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode)); if (Cb) delete Cb; } }
void EOS_CALL UEOSFriendsSubsystem::OnRejectInviteCb(const EOS_Friends_RejectInviteCallbackInfo* D)
{ if (D) { auto* Cb = static_cast<FFriendInviteCbData*>(D->ClientData); UE_LOG(LogEOSFriends, Log, TEXT("RejectInvite for %s: %s"), Cb?*Cb->TargetId:TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode)); if (Cb) delete Cb; } }
void EOS_CALL UEOSFriendsSubsystem::OnFriendsUpdateCb(const EOS_Friends_OnFriendsUpdateInfo* D)
{ if (D) UE_LOG(LogEOSFriends, Log, TEXT("Friends list updated (prev=%d new=%d)"), (int)D->PreviousStatus, (int)D->CurrentStatus); }
