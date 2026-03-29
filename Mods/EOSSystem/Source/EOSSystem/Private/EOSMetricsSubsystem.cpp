// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSMetricsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSSystemConfig.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSMetrics, Log, All);

void UEOSMetricsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);
}
void UEOSMetricsSubsystem::Deinitialize()
{
    // End all active sessions on shutdown
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HMetrics H = GetMetricsHandle();
    if (H && SDK.fp_EOS_Metrics_EndPlayerSession)
        for (const FString& P : ActiveSessions)
        {
            EOS_ProductUserId Id = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*P)) : nullptr;
            if (!Id) continue;
            EOS_Metrics_EndPlayerSessionOptions O = {}; O.ApiVersion = EOS_METRICS_ENDPLAYERSESSION_API_LATEST; O.LocalUserId = Id; O.AccountId = nullptr;
            SDK.fp_EOS_Metrics_EndPlayerSession(H, &O);
        }
    ActiveSessions.Empty();
    Super::Deinitialize();
}

EOS_HMetrics UEOSMetricsSubsystem::GetMetricsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetMetricsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetMetricsInterface(Sys->GetPlatformHandle());
}

bool UEOSMetricsSubsystem::BeginPlayerSession(const FString& PUID, const FString& DisplayName, const FString& GameSessionId)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HMetrics H = GetMetricsHandle();
    if (!H || !SDK.fp_EOS_Metrics_BeginPlayerSession) return false;
    EOS_ProductUserId Id = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!Id) return false;
    EOS_Metrics_BeginPlayerSessionOptions O = {};
    O.ApiVersion    = EOS_METRICS_BEGINPLAYERSESSION_API_LATEST;
    O.LocalUserId   = Id;
    O.AccountId     = nullptr;
    O.DisplayName   = TCHAR_TO_UTF8(*DisplayName);
    O.ControllerType= EOS_UCT_Unknown;
    O.ServerIp      = nullptr;
    O.GameSessionId = GameSessionId.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*GameSessionId);
    EOS_EResult R   = SDK.fp_EOS_Metrics_BeginPlayerSession(H, &O);
    bool bOK        = (R == EOS_Success);
    if (bOK) ActiveSessions.Add(PUID);
    UE_LOG(LogEOSMetrics, Log, TEXT("BeginPlayerSession %s: %s"), *PUID, *FEOSSDKLoader::ResultToString(R));
    OnBeginPlayerSession.Broadcast(PUID, bOK);
    return bOK;
}

bool UEOSMetricsSubsystem::EndPlayerSession(const FString& PUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HMetrics H = GetMetricsHandle();
    if (!H || !SDK.fp_EOS_Metrics_EndPlayerSession) return false;
    EOS_ProductUserId Id = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    if (!Id) return false;
    EOS_Metrics_EndPlayerSessionOptions O = {}; O.ApiVersion = EOS_METRICS_ENDPLAYERSESSION_API_LATEST; O.LocalUserId = Id; O.AccountId = nullptr;
    EOS_EResult R = SDK.fp_EOS_Metrics_EndPlayerSession(H, &O);
    bool bOK = (R == EOS_Success);
    if (bOK) ActiveSessions.Remove(PUID);
    UE_LOG(LogEOSMetrics, Log, TEXT("EndPlayerSession %s: %s"), *PUID, *FEOSSDKLoader::ResultToString(R));
    OnEndPlayerSession.Broadcast(PUID, bOK);
    return bOK;
}

bool UEOSMetricsSubsystem::IsPlayerSessionActive(const FString& PUID) const { return ActiveSessions.Contains(PUID); }
TArray<FString> UEOSMetricsSubsystem::GetActiveSessionPUIDs() const { return ActiveSessions.Array(); }
