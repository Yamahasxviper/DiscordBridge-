// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSStatsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSStats, Log, All);

struct FStatCbData { TWeakObjectPtr<UEOSStatsSubsystem> Sub; FString PUID; FString StatName; };
struct FStatQueryCbData { TWeakObjectPtr<UEOSStatsSubsystem> Sub; FString PUID; EOS_ProductUserId LocalId; EOS_ProductUserId TargetId; };

void UEOSStatsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{ Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSStatsSubsystem::Deinitialize() { StatsCache.Empty(); Super::Deinitialize(); }

EOS_HStats UEOSStatsSubsystem::GetStatsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetStatsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetStatsInterface(Sys->GetPlatformHandle());
}

void UEOSStatsSubsystem::IngestStat(const FString& PUID, const FString& StatName, int32 Amount)
{
    TMap<FString,int32> M; M.Add(StatName, Amount);
    IngestStats(PUID, M);
}

void UEOSStatsSubsystem::IngestStats(const FString& PUID, const TMap<FString,int32>& Stats)
{
    if (Stats.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HStats H = GetStatsHandle();
    if (!H || !SDK.fp_EOS_Stats_IngestStat) return;

    auto* Conn   = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_ProductUserId TargetId = (SDK.fp_EOS_ProductUserId_FromString && !PUID.IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!TargetId) return;

    TArray<EOS_Stats_IngestData> Data;
    TArray<std::string> NameStorage;
    for (const auto& KV : Stats)
    {
        NameStorage.Add(TCHAR_TO_UTF8(*KV.Key));
        EOS_Stats_IngestData D = {}; D.ApiVersion = EOS_STATS_INGESTDATA_API_LATEST;
        D.StatName = NameStorage.Last().c_str(); D.IngestAmount = KV.Value;
        Data.Add(D);
    }

    EOS_Stats_IngestStatOptions O = {};
    O.ApiVersion    = EOS_STATS_INGESTSTAT_API_LATEST;
    O.LocalUserId   = LocalId ? LocalId : TargetId;
    O.TargetUserId  = TargetId;
    O.Stats         = Data.GetData();
    O.StatsCount    = (uint32_t)Data.Num();

    FString FirstName = Stats.begin().Key(); // first key for callback display
    auto* Cb = new FStatCbData{ this, PUID, FirstName };
    SDK.fp_EOS_Stats_IngestStat(H, &O, Cb, &UEOSStatsSubsystem::OnIngestCb);
}

void UEOSStatsSubsystem::QueryStats(const FString& PUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HStats H = GetStatsHandle();
    if (!H || !SDK.fp_EOS_Stats_QueryStats) return;
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID())) : nullptr;
    EOS_ProductUserId TargetId = (SDK.fp_EOS_ProductUserId_FromString && !PUID.IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!TargetId) return;
    EOS_Stats_QueryStatsOptions O = {};
    O.ApiVersion   = EOS_STATS_QUERYSTATS_API_LATEST;
    O.LocalUserId  = LocalId ? LocalId : TargetId;
    O.StartTime    = EOS_STATS_TIME_UNDEFINED;
    O.EndTime      = EOS_STATS_TIME_UNDEFINED;
    O.StatNames    = nullptr; O.StatNamesCount = 0;
    O.TargetUserId = TargetId;
    auto* Cb = new FStatQueryCbData{ this, PUID, LocalId, TargetId };
    SDK.fp_EOS_Stats_QueryStats(H, &O, Cb, &UEOSStatsSubsystem::OnQueryCb);
}

TArray<FEOSStatInfo> UEOSStatsSubsystem::GetCachedStats(const FString& PUID) const
{ if (const auto* P = StatsCache.Find(PUID)) return *P; return {}; }

int32 UEOSStatsSubsystem::GetCachedStatValue(const FString& PUID, const FString& StatName) const
{
    if (const auto* P = StatsCache.Find(PUID))
        for (const FEOSStatInfo& S : *P) if (S.Name.Equals(StatName)) return S.Value;
    return 0;
}

void EOS_CALL UEOSStatsSubsystem::OnIngestCb(const EOS_Stats_IngestStatCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FStatCbData*>(D->ClientData);
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSStats, Log, TEXT("IngestStat %s result: %s"), Cb ? *Cb->StatName : TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb) { if (Cb->Sub.IsValid()) Cb->Sub->OnStatIngested.Broadcast(Cb->StatName, bOK); delete Cb; }
}

void EOS_CALL UEOSStatsSubsystem::OnQueryCb(const EOS_Stats_OnQueryStatsCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FStatQueryCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSStatsSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString PUID = Cb->PUID;
    EOS_ProductUserId TargetId = Cb->TargetId;
    delete Cb;

    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSStats, Warning, TEXT("QueryStats failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HStats H = Self->GetStatsHandle();
    if (!H || !SDK.fp_EOS_Stats_GetStatsCount || !SDK.fp_EOS_Stats_CopyStatByIndex) return;

    EOS_Stats_GetStatCountOptions CO = {}; CO.ApiVersion = EOS_STATS_GETSTATCOUNT_API_LATEST; CO.TargetUserId = TargetId;
    uint32_t Count = SDK.fp_EOS_Stats_GetStatsCount(H, &CO);
    TArray<FEOSStatInfo> Stats;
    for (uint32_t i = 0; i < Count; ++i)
    {
        EOS_Stats_CopyStatByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_STATS_COPYSTATBYINDEX_API_LATEST; CopyO.TargetUserId = TargetId; CopyO.StatIndex = i;
        EOS_Stats_Stat* S = nullptr;
        if (SDK.fp_EOS_Stats_CopyStatByIndex(H, &CopyO, &S) == EOS_Success && S)
        {
            FEOSStatInfo Info; Info.Name = S->Name ? UTF8_TO_TCHAR(S->Name) : TEXT(""); Info.Value = S->Value;
            Stats.Add(Info);
            if (SDK.fp_EOS_Stats_Stat_Release) SDK.fp_EOS_Stats_Stat_Release(S);
        }
    }
    Self->StatsCache.Add(PUID, Stats);
    UE_LOG(LogEOSStats, Log, TEXT("QueryStats for %s: %d stats"), *PUID, Stats.Num());
}
