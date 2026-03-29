// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSLeaderboardsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSLeaderboards, Log, All);

struct FLBRankCbData { TWeakObjectPtr<UEOSLeaderboardsSubsystem> Sub; FString LeaderboardId; };

void UEOSLeaderboardsSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSLeaderboardsSubsystem::Deinitialize() { RecordsCache.Empty(); Super::Deinitialize(); }

EOS_HLeaderboards UEOSLeaderboardsSubsystem::GetLBHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetLeaderboardsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetLeaderboardsInterface(Sys->GetPlatformHandle());
}

void UEOSLeaderboardsSubsystem::QueryLeaderboardDefinitions()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HLeaderboards H = GetLBHandle();
    if (!H || !SDK.fp_EOS_Leaderboards_QueryLeaderboardDefinitions) return;
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_Leaderboards_QueryLeaderboardDefinitionsOptions O = {};
    O.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDDEFINITIONS_API_LATEST;
    O.StartTime = EOS_LEADERBOARDS_TIME_UNDEFINED; O.EndTime = EOS_LEADERBOARDS_TIME_UNDEFINED; O.LocalUserId = LocalId;
    SDK.fp_EOS_Leaderboards_QueryLeaderboardDefinitions(H, &O, this, &UEOSLeaderboardsSubsystem::OnQueryDefsCb);
}

void UEOSLeaderboardsSubsystem::QueryLeaderboardRanks(const FString& LeaderboardId)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HLeaderboards H = GetLBHandle();
    if (!H || !SDK.fp_EOS_Leaderboards_QueryLeaderboardRanks) return;
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_Leaderboards_QueryLeaderboardRanksOptions O = {};
    O.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDRANKS_API_LATEST;
    O.LeaderboardId = TCHAR_TO_UTF8(*LeaderboardId); O.LocalUserId = LocalId;
    auto* Cb = new FLBRankCbData{ this, LeaderboardId };
    SDK.fp_EOS_Leaderboards_QueryLeaderboardRanks(H, &O, Cb, &UEOSLeaderboardsSubsystem::OnQueryRanksCb);
}

void UEOSLeaderboardsSubsystem::QueryUserScores(const TArray<FString>& PUIDs, const FString& StatName)
{
    if (PUIDs.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HLeaderboards H = GetLBHandle();
    if (!H || !SDK.fp_EOS_Leaderboards_QueryLeaderboardUserScores || !SDK.fp_EOS_ProductUserId_FromString) return;
    TArray<EOS_ProductUserId> Ids;
    for (const FString& P : PUIDs) { auto Id = SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*P)); if (Id) Ids.Add(Id); }
    if (Ids.IsEmpty()) return;
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_Leaderboards_QueryLeaderboardUserScoresOptions O = {};
    O.ApiVersion = EOS_LEADERBOARDS_QUERYLEADERBOARDUSERSCORES_API_LATEST;
    O.UserIds = Ids.GetData(); O.UserIdsCount = (uint32_t)Ids.Num();
    O.StatName = TCHAR_TO_UTF8(*StatName); O.Aggregation = EOS_LBA_Max;
    O.StartTime = EOS_LEADERBOARDS_TIME_UNDEFINED; O.EndTime = EOS_LEADERBOARDS_TIME_UNDEFINED; O.LocalUserId = LocalId;
    SDK.fp_EOS_Leaderboards_QueryLeaderboardUserScores(H, &O, this, &UEOSLeaderboardsSubsystem::OnQueryScoresCb);
}

TArray<FEOSLeaderboardRecord> UEOSLeaderboardsSubsystem::GetCachedRanks(const FString& LeaderboardId) const
{ if (const auto* P = RecordsCache.Find(LeaderboardId)) return *P; return {}; }
int32 UEOSLeaderboardsSubsystem::GetCachedRecordCount(const FString& LeaderboardId) const
{ if (const auto* P = RecordsCache.Find(LeaderboardId)) return P->Num(); return 0; }

void EOS_CALL UEOSLeaderboardsSubsystem::OnQueryRanksCb(const EOS_Leaderboards_OnQueryLeaderboardRanksCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FLBRankCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSLeaderboardsSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString LBId = Cb->LeaderboardId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSLeaderboards, Warning, TEXT("QueryRanks failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HLeaderboards H = Self->GetLBHandle();
    if (!H || !SDK.fp_EOS_Leaderboards_GetLeaderboardRecordCount || !SDK.fp_EOS_Leaderboards_CopyLeaderboardRecordByIndex) return;
    EOS_Leaderboards_GetLeaderboardRecordCountOptions CO = {}; CO.ApiVersion = EOS_LEADERBOARDS_GETLEADERBOARDRECORDCOUNT_API_LATEST;
    uint32_t Count = SDK.fp_EOS_Leaderboards_GetLeaderboardRecordCount(H, &CO);
    TArray<FEOSLeaderboardRecord> Records;
    for (uint32_t i = 0; i < Count; ++i)
    {
        EOS_Leaderboards_CopyLeaderboardRecordByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_LEADERBOARDS_COPYLEADERBOARDRECORDBYINDEX_API_LATEST; CopyO.LeaderboardRecordIndex = i;
        EOS_Leaderboards_LeaderboardRecord* Rec = nullptr;
        if (SDK.fp_EOS_Leaderboards_CopyLeaderboardRecordByIndex(H, &CopyO, &Rec) == EOS_Success && Rec)
        {
            FEOSLeaderboardRecord R;
            if (Rec->UserId && SDK.fp_EOS_ProductUserId_ToString) { char Buf[64]={}; int32_t Len=64; if (SDK.fp_EOS_ProductUserId_ToString(Rec->UserId, Buf, &Len)==EOS_Success) R.UserId = UTF8_TO_TCHAR(Buf); }
            R.Rank = (int32)Rec->Rank; R.Score = Rec->Score;
            R.DisplayName = Rec->UserDisplayName ? UTF8_TO_TCHAR(Rec->UserDisplayName) : TEXT("");
            Records.Add(R);
            if (SDK.fp_EOS_Leaderboards_LeaderboardRecord_Release) SDK.fp_EOS_Leaderboards_LeaderboardRecord_Release(Rec);
        }
    }
    Self->RecordsCache.Add(LBId, Records);
    UE_LOG(LogEOSLeaderboards, Log, TEXT("Leaderboard '%s': %d records"), *LBId, Records.Num());
    Self->OnLeaderboardQueried.Broadcast(LBId, true);
}
void EOS_CALL UEOSLeaderboardsSubsystem::OnQueryDefsCb(const EOS_Leaderboards_OnQueryLeaderboardDefinitionsCompleteCallbackInfo* D)
{ if (D) UE_LOG(LogEOSLeaderboards, Log, TEXT("QueryLeaderboardDefs: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); }
void EOS_CALL UEOSLeaderboardsSubsystem::OnQueryScoresCb(const EOS_Leaderboards_OnQueryLeaderboardUserScoresCompleteCallbackInfo* D)
{ if (D) UE_LOG(LogEOSLeaderboards, Log, TEXT("QueryUserScores: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); }
