// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSAchievementsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSAchievements, Log, All);

struct FAchUnlockCbData { TWeakObjectPtr<UEOSAchievementsSubsystem> Sub; FString PUID; };
struct FAchQueryCbData  { TWeakObjectPtr<UEOSAchievementsSubsystem> Sub; FString PUID; EOS_ProductUserId LocalId; EOS_ProductUserId TargetId; };

void UEOSAchievementsSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSAchievementsSubsystem::Deinitialize() { Cache.Empty(); Super::Deinitialize(); }

EOS_HAchievements UEOSAchievementsSubsystem::GetAchievementsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetAchievementsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetAchievementsInterface(Sys->GetPlatformHandle());
}

void UEOSAchievementsSubsystem::UnlockAchievements(const FString& PUID, const TArray<FString>& AchievementIds)
{
    if (AchievementIds.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAchievements H = GetAchievementsHandle();
    if (!H || !SDK.fp_EOS_Achievements_UnlockAchievements) return;
    EOS_ProductUserId UserId = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!UserId) return;
    TArray<const char*> Ids; TArray<std::string> Storage;
    for (const FString& A : AchievementIds) { Storage.Add(TCHAR_TO_UTF8(*A)); Ids.Add(Storage.Last().c_str()); }
    EOS_Achievements_UnlockAchievementsOptions O = {};
    O.ApiVersion = EOS_ACHIEVEMENTS_UNLOCKACHIEVEMENTS_API_LATEST; O.UserId = UserId;
    O.AchievementIds = Ids.GetData(); O.AchievementsCount = (uint32_t)Ids.Num();
    auto* Cb = new FAchUnlockCbData{ this, PUID };
    SDK.fp_EOS_Achievements_UnlockAchievements(H, &O, Cb, &UEOSAchievementsSubsystem::OnUnlockCb);
}

void UEOSAchievementsSubsystem::QueryPlayerAchievements(const FString& PUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAchievements H = GetAchievementsHandle();
    if (!H || !SDK.fp_EOS_Achievements_QueryPlayerAchievements) return;
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_ProductUserId TargetId = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!TargetId) return;
    EOS_Achievements_QueryPlayerAchievementsOptions O = {};
    O.ApiVersion = EOS_ACHIEVEMENTS_QUERYPLAYERACHIEVEMENTS_API_LATEST;
    O.LocalUserId = LocalId ? LocalId : TargetId; O.TargetUserId = TargetId;
    auto* Cb = new FAchQueryCbData{ this, PUID, LocalId, TargetId };
    SDK.fp_EOS_Achievements_QueryPlayerAchievements(H, &O, Cb, &UEOSAchievementsSubsystem::OnQueryCb);
}

TArray<FEOSAchievementInfo> UEOSAchievementsSubsystem::GetCachedAchievements(const FString& PUID) const
{ if (const auto* P = Cache.Find(PUID)) return *P; return {}; }

bool UEOSAchievementsSubsystem::HasUnlockedAchievement(const FString& PUID, const FString& Id) const
{
    if (const auto* P = Cache.Find(PUID)) for (const auto& A : *P) if (A.AchievementId == Id && A.Progress >= 1.f) return true;
    return false;
}

void EOS_CALL UEOSAchievementsSubsystem::OnUnlockCb(const EOS_Achievements_OnUnlockAchievementsCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FAchUnlockCbData*>(D->ClientData);
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSAchievements, Log, TEXT("UnlockAchievements (%u) for %s: %s"), D->AchievementsCount, Cb ? *Cb->PUID : TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb) { if (Cb->Sub.IsValid()) Cb->Sub->OnAchievementUnlocked.Broadcast(Cb->PUID, bOK); delete Cb; }
}

void EOS_CALL UEOSAchievementsSubsystem::OnQueryCb(const EOS_Achievements_OnQueryPlayerAchievementsCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FAchQueryCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSAchievementsSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString PUID = Cb->PUID; EOS_ProductUserId LocalId = Cb->LocalId; EOS_ProductUserId TargetId = Cb->TargetId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAchievements H = Self->GetAchievementsHandle();
    if (!H || !SDK.fp_EOS_Achievements_GetPlayerAchievementCount || !SDK.fp_EOS_Achievements_CopyPlayerAchievementByIndex) return;
    EOS_Achievements_GetPlayerAchievementCountOptions CO = {}; CO.ApiVersion = EOS_ACHIEVEMENTS_GETPLAYERACHIEVEMENTCOUNT_API_LATEST; CO.UserId = TargetId;
    uint32_t Count = SDK.fp_EOS_Achievements_GetPlayerAchievementCount(H, &CO);
    TArray<FEOSAchievementInfo> List;
    for (uint32_t i = 0; i < Count; ++i)
    {
        EOS_Achievements_CopyPlayerAchievementByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_ACHIEVEMENTS_COPYPLAYERACHIEVEMENTBYINDEX_API_LATEST;
        CopyO.LocalUserId = LocalId ? LocalId : TargetId; CopyO.TargetUserId = TargetId; CopyO.AchievementIndex = i;
        EOS_Achievements_PlayerAchievement* A = nullptr;
        if (SDK.fp_EOS_Achievements_CopyPlayerAchievementByIndex(H, &CopyO, &A) == EOS_Success && A)
        {
            FEOSAchievementInfo Info; Info.AchievementId = A->AchievementId ? UTF8_TO_TCHAR(A->AchievementId) : TEXT("");
            Info.Progress = (float)A->Progress; Info.UnlockTime = A->UnlockTime;
            List.Add(Info);
            if (SDK.fp_EOS_Achievements_PlayerAchievement_Release) SDK.fp_EOS_Achievements_PlayerAchievement_Release(A);
        }
    }
    Self->Cache.Add(PUID, List);
    UE_LOG(LogEOSAchievements, Log, TEXT("QueryAchievements for %s: %d found"), *PUID, List.Num());
}
