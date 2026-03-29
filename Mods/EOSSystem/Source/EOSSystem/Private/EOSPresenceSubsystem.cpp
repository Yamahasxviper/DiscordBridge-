// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSPresenceSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSPresence, Log, All);

struct FPresenceQueryCbData { TWeakObjectPtr<UEOSPresenceSubsystem> Sub; FString AccountId; };
struct FPresenceSetCbData   { TWeakObjectPtr<UEOSPresenceSubsystem> Sub; FString AccountId; };

void UEOSPresenceSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSPresenceSubsystem::Deinitialize() { Super::Deinitialize(); }

EOS_HPresence UEOSPresenceSubsystem::GetPresenceHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetPresenceInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetPresenceInterface(Sys->GetPlatformHandle());
}

static EOS_EpicAccountId Presence_AccountFromStr(const FString& S) { FEOSSDKLoader& SDK = FEOSSDKLoader::Get(); return SDK.fp_EOS_EpicAccountId_FromString ? SDK.fp_EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*S)) : nullptr; }

void UEOSPresenceSubsystem::QueryPresence(const FString& Local, const FString& Target)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HPresence H = GetPresenceHandle();
    if (!H || !SDK.fp_EOS_Presence_QueryPresence) return;
    EOS_EpicAccountId LocalId = Presence_AccountFromStr(Local); EOS_EpicAccountId TargetId = Presence_AccountFromStr(Target);
    if (!LocalId || !TargetId) return;
    EOS_Presence_QueryPresenceOptions O = {}; O.ApiVersion = EOS_PRESENCE_QUERYPRESENCE_API_LATEST; O.LocalUserId = LocalId; O.TargetUserId = TargetId;
    auto* Cb = new FPresenceQueryCbData{ this, Target };
    SDK.fp_EOS_Presence_QueryPresence(H, &O, Cb, &UEOSPresenceSubsystem::OnQueryPresenceCb);
}

void UEOSPresenceSubsystem::SetStatus(const FString& Local, const FString& Status)
{
    SetStatusAndCommit(Local, Status, TEXT(""), {});
}
void UEOSPresenceSubsystem::SetRichText(const FString& Local, const FString& RichText)
{
    SetStatusAndCommit(Local, TEXT(""), RichText, {});
}
void UEOSPresenceSubsystem::SetData(const FString& Local, const TMap<FString,FString>& Data)
{
    SetStatusAndCommit(Local, TEXT(""), TEXT(""), Data);
}

void UEOSPresenceSubsystem::SetStatusAndCommit(const FString& Local, const FString& Status, const FString& RichText, const TMap<FString,FString>& Data)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HPresence H = GetPresenceHandle();
    if (!H || !SDK.fp_EOS_Presence_CreatePresenceModification || !SDK.fp_EOS_Presence_SetPresence) return;
    EOS_EpicAccountId LocalId = Presence_AccountFromStr(Local); if (!LocalId) return;

    EOS_HPresenceModification Mod = nullptr;
    EOS_Presence_CreatePresenceModificationOptions CO = {}; CO.ApiVersion = EOS_PRESENCE_CREATEPRESENCEMODIFICATION_API_LATEST; CO.LocalUserId = LocalId;
    if (SDK.fp_EOS_Presence_CreatePresenceModification(H, &CO, &Mod) != EOS_Success || !Mod) return;

    if (!Status.IsEmpty() && SDK.fp_EOS_PresenceModification_SetStatus)
    {
        EOS_PresenceModification_SetStatusOptions SO = {}; SO.ApiVersion = EOS_PRESENCEMODIFICATION_SETSTATUS_API_LATEST;
        SO.Status = EOS_PMS_Online;
        if (Status.Equals(TEXT("Away"),       ESearchCase::IgnoreCase)) SO.Status = EOS_PMS_Away;
        else if (Status.Equals(TEXT("ExtendedAway"), ESearchCase::IgnoreCase)) SO.Status = EOS_PMS_ExtendedAway;
        else if (Status.Equals(TEXT("DoNotDisturb"), ESearchCase::IgnoreCase)) SO.Status = EOS_PMS_DoNotDisturb;
        else if (Status.Equals(TEXT("Offline"),      ESearchCase::IgnoreCase)) SO.Status = EOS_PMS_Offline;
        SDK.fp_EOS_PresenceModification_SetStatus(Mod, &SO);
    }
    if (!RichText.IsEmpty() && SDK.fp_EOS_PresenceModification_SetRawRichText)
    {
        EOS_PresenceModification_SetRawRichTextOptions RO = {}; RO.ApiVersion = EOS_PRESENCEMODIFICATION_SETRAWRICHTEXT_API_LATEST; RO.RichText = TCHAR_TO_UTF8(*RichText);
        SDK.fp_EOS_PresenceModification_SetRawRichText(Mod, &RO);
    }
    if (!Data.IsEmpty() && SDK.fp_EOS_PresenceModification_SetData)
    {
        TArray<EOS_Presence_DataRecord> Records; TArray<std::string> KS, VS;
        for (const auto& KV : Data) { KS.Add(TCHAR_TO_UTF8(*KV.Key)); VS.Add(TCHAR_TO_UTF8(*KV.Value)); }
        for (int32 i = 0; i < KS.Num(); ++i) { EOS_Presence_DataRecord R = {}; R.ApiVersion = EOS_PRESENCE_DATARECORD_API_LATEST; R.Key = KS[i].c_str(); R.Value = VS[i].c_str(); Records.Add(R); }
        EOS_PresenceModification_SetDataOptions DO = {}; DO.ApiVersion = EOS_PRESENCEMODIFICATION_SETDATA_API_LATEST; DO.Records = Records.GetData(); DO.RecordsCount = (uint32_t)Records.Num();
        SDK.fp_EOS_PresenceModification_SetData(Mod, &DO);
    }
    EOS_Presence_SetPresenceOptions PO = {}; PO.ApiVersion = EOS_PRESENCE_SETPRESENCE_API_LATEST; PO.LocalUserId = LocalId; PO.PresenceModificationHandle = Mod;
    auto* Cb = new FPresenceSetCbData{ this, Local };
    SDK.fp_EOS_Presence_SetPresence(H, &PO, Cb, &UEOSPresenceSubsystem::OnSetPresenceCb);
    if (SDK.fp_EOS_PresenceModification_Release) SDK.fp_EOS_PresenceModification_Release(Mod);
}

void EOS_CALL UEOSPresenceSubsystem::OnQueryPresenceCb(const EOS_Presence_QueryPresenceCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FPresenceQueryCbData*>(D->ClientData);
    if (!Cb) return;
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSPresence, Log, TEXT("QueryPresence for %s: %s"), *Cb->AccountId, *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb->Sub.IsValid()) Cb->Sub->OnPresenceQueried.Broadcast(Cb->AccountId, bOK);
    delete Cb;
}
void EOS_CALL UEOSPresenceSubsystem::OnSetPresenceCb(const EOS_Presence_SetPresenceCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FPresenceSetCbData*>(D->ClientData);
    if (!Cb) return;
    bool bOK = (D->ResultCode == EOS_Success);
    UE_LOG(LogEOSPresence, Log, TEXT("SetPresence for %s: %s"), *Cb->AccountId, *FEOSSDKLoader::ResultToString(D->ResultCode));
    if (Cb->Sub.IsValid()) Cb->Sub->OnPresenceSet.Broadcast(Cb->AccountId, bOK);
    delete Cb;
}
