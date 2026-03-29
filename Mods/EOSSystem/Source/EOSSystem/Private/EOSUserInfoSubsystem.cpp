// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSUserInfoSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSUserInfo, Log, All);

struct FUserInfoCbData { TWeakObjectPtr<UEOSUserInfoSubsystem> Sub; FString LocalId; FString TargetId; EOS_EpicAccountId LocalAccountId; EOS_EpicAccountId TargetAccountId; };

void UEOSUserInfoSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSUserInfoSubsystem::Deinitialize() { UserInfoCache.Empty(); Super::Deinitialize(); }

EOS_HUserInfo UEOSUserInfoSubsystem::GetUserInfoHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetUserInfoInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetUserInfoInterface(Sys->GetPlatformHandle());
}

static EOS_EpicAccountId UserInfo_AccountFromStr(const FString& S) { FEOSSDKLoader& SDK = FEOSSDKLoader::Get(); return SDK.fp_EOS_EpicAccountId_FromString ? SDK.fp_EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*S)) : nullptr; }

void UEOSUserInfoSubsystem::QueryUserInfo(const FString& Local, const FString& Target)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HUserInfo H = GetUserInfoHandle();
    if (!H || !SDK.fp_EOS_UserInfo_QueryUserInfo) return;
    EOS_EpicAccountId LocalId = UserInfo_AccountFromStr(Local); EOS_EpicAccountId TargetId = UserInfo_AccountFromStr(Target);
    if (!LocalId || !TargetId) return;
    EOS_UserInfo_QueryUserInfoOptions O = {}; O.ApiVersion = EOS_USERINFO_QUERYUSERINFO_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    auto* Cb = new FUserInfoCbData{ this, Local, Target, LocalId, TargetId };
    SDK.fp_EOS_UserInfo_QueryUserInfo(H, &O, Cb, &UEOSUserInfoSubsystem::OnQueryUserInfoCb);
}

void UEOSUserInfoSubsystem::QueryUserInfoByDisplayName(const FString& Local, const FString& DisplayName)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HUserInfo H = GetUserInfoHandle();
    if (!H || !SDK.fp_EOS_UserInfo_QueryUserInfoByDisplayName) return;
    EOS_EpicAccountId LocalId = UserInfo_AccountFromStr(Local); if (!LocalId) return;
    EOS_UserInfo_QueryUserInfoByDisplayNameOptions O = {}; O.ApiVersion = EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST;
    O.LocalUserId = LocalId; O.DisplayName = TCHAR_TO_UTF8(*DisplayName);
    auto* Cb = new FUserInfoCbData{ this, Local, DisplayName, LocalId, nullptr };
    SDK.fp_EOS_UserInfo_QueryUserInfoByDisplayName(H, &O, Cb, &UEOSUserInfoSubsystem::OnQueryByDisplayNameCb);
}

FEOSUserInfo UEOSUserInfoSubsystem::GetCachedUserInfo(const FString& TargetEpicAccountId) const
{ if (const auto* P = UserInfoCache.Find(TargetEpicAccountId)) return *P; return FEOSUserInfo{}; }
bool UEOSUserInfoSubsystem::HasCachedUserInfo(const FString& TargetEpicAccountId) const { return UserInfoCache.Contains(TargetEpicAccountId); }

static void CopyAndCacheUserInfo(UEOSUserInfoSubsystem* Self, EOS_HUserInfo H, const FString& LocalStr, const FString& TargetStr, EOS_EpicAccountId LocalId, EOS_EpicAccountId TargetId)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (!SDK.fp_EOS_UserInfo_CopyUserInfo) return;
    EOS_UserInfo_CopyUserInfoOptions CO = {}; CO.ApiVersion = EOS_USERINFO_COPYUSERINFO_API_LATEST; CO.LocalUserId = LocalId; CO.TargetUserId = TargetId;
    EOS_UserInfo* Info = nullptr;
    if (SDK.fp_EOS_UserInfo_CopyUserInfo(H, &CO, &Info) == EOS_Success && Info)
    {
        FEOSUserInfo UInfo;
        UInfo.DisplayName       = Info->DisplayName       ? UTF8_TO_TCHAR(Info->DisplayName)       : TEXT("");
        UInfo.Nickname          = Info->Nickname          ? UTF8_TO_TCHAR(Info->Nickname)          : TEXT("");
        UInfo.PreferredLanguage = Info->PreferredLanguage ? UTF8_TO_TCHAR(Info->PreferredLanguage) : TEXT("");
        UInfo.Country           = Info->Country           ? UTF8_TO_TCHAR(Info->Country)           : TEXT("");
        Self->UserInfoCache.Add(TargetStr, UInfo);
        UE_LOG(LogTemp, Log, TEXT("UserInfo cached for %s: %s"), *TargetStr, *UInfo.DisplayName);
        if (SDK.fp_EOS_UserInfo_Release) SDK.fp_EOS_UserInfo_Release(Info);
    }
    Self->OnUserInfoQueried.Broadcast(TargetStr, true);
}

void EOS_CALL UEOSUserInfoSubsystem::OnQueryUserInfoCb(const EOS_UserInfo_QueryUserInfoCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FUserInfoCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSUserInfoSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString Local = Cb->LocalId; FString Target = Cb->TargetId;
    EOS_EpicAccountId LocalId = Cb->LocalAccountId; EOS_EpicAccountId TargetId = Cb->TargetAccountId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSUserInfo, Warning, TEXT("QueryUserInfo failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    CopyAndCacheUserInfo(Self, Self->GetUserInfoHandle(), Local, Target, LocalId, TargetId);
}
void EOS_CALL UEOSUserInfoSubsystem::OnQueryByDisplayNameCb(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FUserInfoCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSUserInfoSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString Local = Cb->LocalId; FString Target = Cb->TargetId;
    EOS_EpicAccountId LocalId = Cb->LocalAccountId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSUserInfo, Warning, TEXT("QueryByDisplayName failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    // TargetUserId is in the callback
    if (D->TargetUserId)
    {
        FString TargetStr; char Buf[64]={}; int32_t Len=64;
        FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
        if (SDK.fp_EOS_EpicAccountId_ToString && SDK.fp_EOS_EpicAccountId_ToString(D->TargetUserId, Buf, &Len)==EOS_Success) TargetStr = UTF8_TO_TCHAR(Buf);
        CopyAndCacheUserInfo(Self, Self->GetUserInfoHandle(), Local, TargetStr.IsEmpty() ? Target : TargetStr, LocalId, D->TargetUserId);
    }
}
