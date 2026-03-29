// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSReportsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSReports, Log, All);

struct FReportCbData { TWeakObjectPtr<UEOSReportsSubsystem> Sub; FString ReportedPUID; };

void UEOSReportsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);
}
void UEOSReportsSubsystem::Deinitialize() { Super::Deinitialize(); }

EOS_HReports UEOSReportsSubsystem::GetReportsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetReportsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetReportsInterface(Sys->GetPlatformHandle());
}

EOS_EPlayerReportsCategory UEOSReportsSubsystem::ParseCategory(const FString& Cat)
{
    if (Cat.Equals(TEXT("Cheating"),           ESearchCase::IgnoreCase)) return EOS_PRC_Cheating;
    if (Cat.Equals(TEXT("Exploiting"),         ESearchCase::IgnoreCase)) return EOS_PRC_Exploiting;
    if (Cat.Equals(TEXT("OffensiveProfile"),   ESearchCase::IgnoreCase)) return EOS_PRC_OffensiveProfile;
    if (Cat.Equals(TEXT("VerbalAbuse"),        ESearchCase::IgnoreCase)) return EOS_PRC_VerbalAbuse;
    if (Cat.Equals(TEXT("Scamming"),           ESearchCase::IgnoreCase)) return EOS_PRC_Scamming;
    if (Cat.Equals(TEXT("Spamming"),           ESearchCase::IgnoreCase)) return EOS_PRC_Spamming;
    return EOS_PRC_Other;
}

void UEOSReportsSubsystem::SendReport(const FString& ReporterPUID, const FString& ReportedPUID, const FString& Category, const FString& Message, const FString& Context)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HReports H = GetReportsHandle();
    if (!H || !SDK.fp_EOS_Reports_SendPlayerBehaviorReport) { UE_LOG(LogEOSReports, Warning, TEXT("Reports interface not ready")); return; }
    EOS_ProductUserId Reporter = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*ReporterPUID)) : nullptr;
    EOS_ProductUserId Reported  = SDK.fp_EOS_ProductUserId_FromString ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*ReportedPUID)) : nullptr;
    if (!Reporter || !Reported) { UE_LOG(LogEOSReports, Warning, TEXT("SendReport: invalid PUID(s)")); return; }
    EOS_Reports_SendPlayerBehaviorReportOptions O = {};
    O.ApiVersion    = EOS_REPORTS_SENDPLAYERBEHAVIORREPORT_API_LATEST;
    O.ReporterUserId= Reporter;
    O.ReportedUserId= Reported;
    O.Category      = ParseCategory(Category);
    O.Message       = Message.IsEmpty()  ? nullptr : TCHAR_TO_UTF8(*Message);
    O.Context       = Context.IsEmpty()  ? nullptr : TCHAR_TO_UTF8(*Context);
    auto* Cb        = new FReportCbData{ this, ReportedPUID };
    SDK.fp_EOS_Reports_SendPlayerBehaviorReport(H, &O, Cb, &UEOSReportsSubsystem::OnReportSentCb);
}

void UEOSReportsSubsystem::ReportCheating(const FString& R, const FString& T, const FString& M)       { SendReport(R, T, TEXT("Cheating"),         M, TEXT("")); }
void UEOSReportsSubsystem::ReportVerbalAbuse(const FString& R, const FString& T, const FString& M)    { SendReport(R, T, TEXT("VerbalAbuse"),       M, TEXT("")); }
void UEOSReportsSubsystem::ReportScamming(const FString& R, const FString& T, const FString& M)       { SendReport(R, T, TEXT("Scamming"),          M, TEXT("")); }
void UEOSReportsSubsystem::ReportExploiting(const FString& R, const FString& T, const FString& M)     { SendReport(R, T, TEXT("Exploiting"),        M, TEXT("")); }
void UEOSReportsSubsystem::ReportSpamming(const FString& R, const FString& T, const FString& M)       { SendReport(R, T, TEXT("Spamming"),          M, TEXT("")); }
void UEOSReportsSubsystem::ReportOffensiveProfile(const FString& R, const FString& T, const FString& M){ SendReport(R, T, TEXT("OffensiveProfile"),  M, TEXT("")); }

void EOS_CALL UEOSReportsSubsystem::OnReportSentCb(const EOS_Reports_SendPlayerBehaviorReportCompleteCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FReportCbData*>(D->ClientData);
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSReports, Log, TEXT("Report result for %s: %s"), Cb ? *Cb->ReportedPUID : TEXT("?"), *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb)
    {
        if (Cb->Sub.IsValid()) Cb->Sub->OnReportSent.Broadcast(Cb->ReportedPUID, bOK);
        delete Cb;
    }
}
