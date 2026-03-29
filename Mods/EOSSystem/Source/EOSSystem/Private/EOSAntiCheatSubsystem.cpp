// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSAntiCheatSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "EOSSystemConfig.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSAntiCheat, Log, All);

void UEOSAntiCheatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (Cfg && Cfg->bEnableAntiCheat) BeginSession();
}
void UEOSAntiCheatSubsystem::Deinitialize() { if (bSessionActive) EndSession(); Super::Deinitialize(); }

EOS_HAntiCheatServer UEOSAntiCheatSubsystem::GetACHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetAntiCheatServerInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetAntiCheatServerInterface(Sys->GetPlatformHandle());
}

bool UEOSAntiCheatSubsystem::BeginSession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    if (!AC || !SDK.fp_EOS_AntiCheatServer_BeginSession) return false;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = nullptr;
    if (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        LocalId = SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID()));
    EOS_AntiCheatServer_BeginSessionOptions O = {};
    O.ApiVersion = EOS_ANTICHEATSERVER_BEGINSESSION_API_LATEST;
    O.LocalUserId = LocalId;
    O.ServerName  = Cfg ? TCHAR_TO_UTF8(*Cfg->AntiCheatServerName) : "EOSServer";
    O.bEnableGameplayData = EOS_TRUE;
    O.MaxClients = Cfg ? Cfg->MaxPlayers : 64;
    EOS_EResult R = SDK.fp_EOS_AntiCheatServer_BeginSession(AC, &O);
    if (R != EOS_Success) { UE_LOG(LogEOSAntiCheat, Error, TEXT("BeginSession failed: %s"), *FEOSSDKLoader::ResultToString(R)); return false; }
    bSessionActive = true;
    if (SDK.fp_EOS_AntiCheatServer_AddNotifyMessageToClient)
    {
        EOS_AntiCheatServer_AddNotifyMessageToClientOptions MO = {}; MO.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYMESSAGETOCLIENT_API_LATEST;
        MsgNotifId = SDK.fp_EOS_AntiCheatServer_AddNotifyMessageToClient(AC, &MO, this, &UEOSAntiCheatSubsystem::OnMessageToClient);
    }
    if (SDK.fp_EOS_AntiCheatServer_AddNotifyClientActionRequired)
    {
        EOS_AntiCheatServer_AddNotifyClientActionRequiredOptions AO = {}; AO.ApiVersion = EOS_ANTICHEATSERVER_ADDNOTIFYCLIENTACTIONREQUIRED_API_LATEST;
        ActionNotifId = SDK.fp_EOS_AntiCheatServer_AddNotifyClientActionRequired(AC, &AO, this, &UEOSAntiCheatSubsystem::OnClientActionRequired);
    }
    UE_LOG(LogEOSAntiCheat, Log, TEXT("AntiCheat session started."));
    return true;
}

void UEOSAntiCheatSubsystem::EndSession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    if (AC)
    {
        if (MsgNotifId    != EOS_INVALID_NOTIFICATIONID && SDK.fp_EOS_AntiCheatServer_RemoveNotifyMessageToClient)       SDK.fp_EOS_AntiCheatServer_RemoveNotifyMessageToClient(AC, MsgNotifId);
        if (ActionNotifId != EOS_INVALID_NOTIFICATIONID && SDK.fp_EOS_AntiCheatServer_RemoveNotifyClientActionRequired) SDK.fp_EOS_AntiCheatServer_RemoveNotifyClientActionRequired(AC, ActionNotifId);
        if (SDK.fp_EOS_AntiCheatServer_EndSession) { EOS_AntiCheatServer_EndSessionOptions O = {}; O.ApiVersion = EOS_ANTICHEATSERVER_ENDSESSION_API_LATEST; SDK.fp_EOS_AntiCheatServer_EndSession(AC, &O); }
    }
    bSessionActive = false; RegisteredClients.Empty();
    MsgNotifId = ActionNotifId = EOS_INVALID_NOTIFICATIONID;
}

void UEOSAntiCheatSubsystem::RegisterClient(const FString& PUID, const FString& IpAddress)
{
    if (!bSessionActive || RegisteredClients.Contains(PUID)) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    if (!AC || !SDK.fp_EOS_AntiCheatServer_RegisterConnectedClient) return;
    EOS_AntiCheatCommon_ClientHandle Handle = reinterpret_cast<EOS_AntiCheatCommon_ClientHandle>(static_cast<intptr_t>(NextHandleVal++));
    EOS_ProductUserId UserId = (SDK.fp_EOS_ProductUserId_FromString && !PUID.IsEmpty()) ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*PUID)) : nullptr;
    EOS_AntiCheatServer_RegisterConnectedClientOptions O = {};
    O.ApiVersion = EOS_ANTICHEATSERVER_REGISTERCONNECTEDCLIENT_API_LATEST;
    O.ClientHandle = Handle; O.ClientType = EOS_ACCT_ManagedByEOS; O.ClientPlatform = EOS_ACCP_Windows;
    O.IpAddress = IpAddress.IsEmpty() ? nullptr : TCHAR_TO_UTF8(*IpAddress); O.UserId = UserId;
    if (SDK.fp_EOS_AntiCheatServer_RegisterConnectedClient(AC, &O) == EOS_Success)
    {
        RegisteredClients.Add(PUID, Handle);
        OnClientRegistered.Broadcast(PUID);
        UE_LOG(LogEOSAntiCheat, Log, TEXT("Registered AC client: %s"), *PUID);
    }
}

void UEOSAntiCheatSubsystem::UnregisterClient(const FString& PUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    auto* H = RegisteredClients.Find(PUID);
    if (!AC || !H || !SDK.fp_EOS_AntiCheatServer_UnregisterConnectedClient) return;
    EOS_AntiCheatServer_UnregisterConnectedClientOptions O = {}; O.ApiVersion = EOS_ANTICHEATSERVER_UNREGISTERCONNECTEDCLIENT_API_LATEST; O.ClientHandle = *H;
    SDK.fp_EOS_AntiCheatServer_UnregisterConnectedClient(AC, &O);
    RegisteredClients.Remove(PUID); OnClientUnregistered.Broadcast(PUID);
}

bool UEOSAntiCheatSubsystem::ProtectMessage(const FString& PUID, const TArray<uint8>& In, TArray<uint8>& Out)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    auto* H = RegisteredClients.Find(PUID);
    if (!AC || !H || !SDK.fp_EOS_AntiCheatServer_ProtectMessage || !SDK.fp_EOS_AntiCheatServer_GetProtectMessageOutputLength) return false;
    EOS_AntiCheatServer_GetProtectMessageOutputLengthOptions LO = {}; LO.ApiVersion = EOS_ANTICHEATSERVER_GETPROTECTMESSAGEOUTPUTLENGTH_API_LATEST; LO.DataLengthBytes = (uint32_t)In.Num();
    uint32_t OutLen = 0; SDK.fp_EOS_AntiCheatServer_GetProtectMessageOutputLength(AC, &LO, &OutLen);
    Out.SetNumUninitialized((int32)OutLen);
    EOS_AntiCheatServer_ProtectMessageOptions O = {}; O.ApiVersion = EOS_ANTICHEATSERVER_PROTECTMESSAGE_API_LATEST;
    O.ClientHandle = *H; O.DataLengthBytes = (uint32_t)In.Num(); O.Data = In.GetData(); O.OutBufferSizeBytes = OutLen;
    uint32_t Written = 0;
    EOS_EResult R = SDK.fp_EOS_AntiCheatServer_ProtectMessage(AC, &O, Out.GetData(), &Written);
    Out.SetNum((int32)Written); return R == EOS_Success;
}

bool UEOSAntiCheatSubsystem::UnprotectMessage(const FString& PUID, const TArray<uint8>& In, TArray<uint8>& Out)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HAntiCheatServer AC = GetACHandle();
    auto* H = RegisteredClients.Find(PUID);
    if (!AC || !H || !SDK.fp_EOS_AntiCheatServer_UnprotectMessage) return false;
    Out.SetNumUninitialized(In.Num());
    EOS_AntiCheatServer_UnprotectMessageOptions O = {}; O.ApiVersion = EOS_ANTICHEATSERVER_UNPROTECTMESSAGE_API_LATEST;
    O.ClientHandle = *H; O.DataLengthBytes = (uint32_t)In.Num(); O.Data = In.GetData(); O.OutBufferSizeBytes = (uint32_t)Out.Num();
    uint32_t Written = 0;
    EOS_EResult R = SDK.fp_EOS_AntiCheatServer_UnprotectMessage(AC, &O, Out.GetData(), &Written);
    Out.SetNum((int32)Written); return R == EOS_Success;
}

TArray<FString> UEOSAntiCheatSubsystem::GetRegisteredClients() const { TArray<FString> K; RegisteredClients.GetKeys(K); return K; }

void EOS_CALL UEOSAntiCheatSubsystem::OnMessageToClient(const EOS_AntiCheatServer_MessageToClientCallbackInfo* D)
{ if (D) UE_LOG(LogEOSAntiCheat, Verbose, TEXT("AC msg to client (%u bytes)"), D->MessageDataSizeBytes); }

void EOS_CALL UEOSAntiCheatSubsystem::OnClientActionRequired(const EOS_AntiCheatServer_ClientActionRequiredCallbackInfo* D)
{
    if (!D) return;
    auto* Self = static_cast<UEOSAntiCheatSubsystem*>(D->ClientData);
    UE_LOG(LogEOSAntiCheat, Warning, TEXT("AC action required: code=%d detail=%s"), (int32)D->ActionReasonCode, D->ActionReasonDetailsString ? UTF8_TO_TCHAR(D->ActionReasonDetailsString) : TEXT(""));
    if (Self) for (const auto& P : Self->RegisteredClients) if (P.Value == D->ClientHandle) { Self->OnClientUnregistered.Broadcast(P.Key); break; }
}
