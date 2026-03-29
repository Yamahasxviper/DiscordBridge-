// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSSessionsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSSystemConfig.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSessions, Log, All);

static EOS_ProductUserId PUIDFromStr(const FString& S)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (S.IsEmpty() || !SDK.fp_EOS_ProductUserId_FromString) return nullptr;
    return SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*S));
}

void UEOSSessionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (Cfg && Cfg->bEnableSessions) CreateOrUpdateSession();
}

void UEOSSessionsSubsystem::Deinitialize()
{
    if (bSessionExists) DestroySession();
    Super::Deinitialize();
}

EOS_HSessions UEOSSessionsSubsystem::GetSessionsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    UEOSSystemSubsystem* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetSessionsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetSessionsInterface(Sys->GetPlatformHandle());
}

void UEOSSessionsSubsystem::CreateOrUpdateSession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess) { UE_LOG(LogEOSSessions, Warning, TEXT("No sessions interface")); return; }
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (!Cfg) return;

    EOS_HSessionModification Mod = nullptr;
    EOS_EResult Res = EOS_InvalidParameters;

    if (!bSessionExists)
    {
        EOS_Sessions_CreateSessionModificationOptions O = {};
        O.ApiVersion        = EOS_SESSIONS_CREATESESSIONMODIFICATION_API_LATEST;
        O.SessionName       = TCHAR_TO_UTF8(*Cfg->SessionName);
        O.BucketId          = TCHAR_TO_UTF8(*Cfg->BucketId);
        O.MaxPlayers        = Cfg->MaxPlayers;
        O.LocalUserId       = nullptr;
        O.bPresenceEnabled  = EOS_FALSE;
        O.SessionId         = nullptr;
        O.bSanctionsEnabled = Cfg->bSanctionsEnabled ? EOS_TRUE : EOS_FALSE;
        if (SDK.fp_EOS_Sessions_CreateSessionModification)
            Res = SDK.fp_EOS_Sessions_CreateSessionModification(Sess, &O, &Mod);
    }
    else
    {
        EOS_Sessions_UpdateSessionModificationOptions O = {};
        O.ApiVersion  = EOS_SESSIONS_UPDATESESSIONMODIFICATION_API_LATEST;
        O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
        if (SDK.fp_EOS_Sessions_UpdateSessionModification)
            Res = SDK.fp_EOS_Sessions_UpdateSessionModification(Sess, &O, &Mod);
    }

    if (Res != EOS_Success || !Mod)
    {
        UE_LOG(LogEOSSessions, Error, TEXT("CreateSessionModification failed: %s"), *FEOSSDKLoader::ResultToString(Res));
        return;
    }

    if (SDK.fp_EOS_SessionModification_SetBucketId)
    {
        EOS_SessionModification_SetBucketIdOptions B = {};
        B.ApiVersion = EOS_SESSIONMODIFICATION_SETBUCKETID_API_LATEST;
        B.BucketId   = TCHAR_TO_UTF8(*Cfg->BucketId);
        SDK.fp_EOS_SessionModification_SetBucketId(Mod, &B);
    }
    if (SDK.fp_EOS_SessionModification_SetMaxPlayers)
    {
        EOS_SessionModification_SetMaxPlayersOptions M = {};
        M.ApiVersion = EOS_SESSIONMODIFICATION_SETMAXPLAYERS_API_LATEST;
        M.MaxPlayers = Cfg->MaxPlayers;
        SDK.fp_EOS_SessionModification_SetMaxPlayers(Mod, &M);
    }
    if (!Cfg->ServerIpAddress.IsEmpty() && SDK.fp_EOS_SessionModification_SetHostAddress)
    {
        EOS_SessionModification_SetHostAddressOptions H = {};
        H.ApiVersion  = EOS_SESSIONMODIFICATION_SETHOSTADDRESS_API_LATEST;
        H.HostAddress = TCHAR_TO_UTF8(*Cfg->ServerIpAddress);
        SDK.fp_EOS_SessionModification_SetHostAddress(Mod, &H);
    }
    if (SDK.fp_EOS_SessionModification_SetJoinInProgressAllowed)
    {
        EOS_SessionModification_SetJoinInProgressAllowedOptions J = {};
        J.ApiVersion           = EOS_SESSIONMODIFICATION_SETJOININPROGRESSALLOWED_API_LATEST;
        J.bAllowJoinInProgress = Cfg->bJoinInProgressAllowed ? EOS_TRUE : EOS_FALSE;
        SDK.fp_EOS_SessionModification_SetJoinInProgressAllowed(Mod, &J);
    }
    for (const TPair<FString,FString>& A : PendingAttributes)
    {
        if (!SDK.fp_EOS_SessionModification_AddAttribute) break;
        // Keep char* alive for the duration of the call
        const std::string KeyStr = TCHAR_TO_UTF8(*A.Key);
        const std::string ValStr = TCHAR_TO_UTF8(*A.Value);
        EOS_Sessions_AttributeData AD = {};
        AD.ApiVersion   = EOS_SESSIONS_SESSIONATTRIBUTEDATA_API_LATEST;
        AD.Key          = KeyStr.c_str();
        AD.Value.AsUtf8 = ValStr.c_str();
        AD.ValueType    = EOS_AT_STRING;
        EOS_SessionModification_AddAttributeOptions AO = {};
        AO.ApiVersion        = EOS_SESSIONMODIFICATION_ADDATTRIBUTE_API_LATEST;
        AO.SessionAttribute  = &AD;
        AO.AdvertisementType = EOS_SAAT_Advertise;
        SDK.fp_EOS_SessionModification_AddAttribute(Mod, &AO);
    }

    if (SDK.fp_EOS_Sessions_UpdateSession)
    {
        EOS_Sessions_UpdateSessionOptions U = {};
        U.ApiVersion                = EOS_SESSIONS_UPDATESESSION_API_LATEST;
        U.SessionModificationHandle = Mod;
        SDK.fp_EOS_Sessions_UpdateSession(Sess, &U, this, &UEOSSessionsSubsystem::OnUpdateSessionCb);
    }
    if (SDK.fp_EOS_SessionModification_Release) SDK.fp_EOS_SessionModification_Release(Mod);
}

void UEOSSessionsSubsystem::StartSession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess || !SDK.fp_EOS_Sessions_StartSession) return;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>(); if (!Cfg) return;
    EOS_Sessions_StartSessionOptions O = {};
    O.ApiVersion = EOS_SESSIONS_STARTSESSION_API_LATEST; O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
    SDK.fp_EOS_Sessions_StartSession(Sess, &O, this, &UEOSSessionsSubsystem::OnStartSessionCb);
}

void UEOSSessionsSubsystem::EndSession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess || !SDK.fp_EOS_Sessions_EndSession) return;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>(); if (!Cfg) return;
    EOS_Sessions_EndSessionOptions O = {};
    O.ApiVersion = EOS_SESSIONS_ENDSESSION_API_LATEST; O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
    SDK.fp_EOS_Sessions_EndSession(Sess, &O, this, &UEOSSessionsSubsystem::OnEndSessionCb);
}

void UEOSSessionsSubsystem::DestroySession()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess || !SDK.fp_EOS_Sessions_DestroySession) return;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>(); if (!Cfg) return;
    EOS_Sessions_DestroySessionOptions O = {};
    O.ApiVersion = EOS_SESSIONS_DESTROYSESSION_API_LATEST; O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
    SDK.fp_EOS_Sessions_DestroySession(Sess, &O, this, &UEOSSessionsSubsystem::OnDestroySessionCb);
}

void UEOSSessionsSubsystem::RegisterPlayers(const TArray<FString>& PUIDs)
{
    if (PUIDs.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess || !SDK.fp_EOS_Sessions_RegisterPlayers) return;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>(); if (!Cfg) return;
    TArray<EOS_ProductUserId> Ids;
    for (const FString& P : PUIDs) { if (auto Id = PUIDFromStr(P)) Ids.Add(Id); }
    if (Ids.IsEmpty()) return;
    EOS_Sessions_RegisterPlayersOptions O = {};
    O.ApiVersion = EOS_SESSIONS_REGISTERPLAYERS_API_LATEST;
    O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
    O.PlayersToRegister = Ids.GetData(); O.PlayersToRegisterCount = (uint32_t)Ids.Num();
    SDK.fp_EOS_Sessions_RegisterPlayers(Sess, &O, this, &UEOSSessionsSubsystem::OnRegisterPlayersCb);
}

void UEOSSessionsSubsystem::UnregisterPlayers(const TArray<FString>& PUIDs)
{
    if (PUIDs.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSessions Sess = GetSessionsHandle();
    if (!Sess || !SDK.fp_EOS_Sessions_UnregisterPlayers) return;
    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>(); if (!Cfg) return;
    TArray<EOS_ProductUserId> Ids;
    for (const FString& P : PUIDs) { if (auto Id = PUIDFromStr(P)) Ids.Add(Id); }
    if (Ids.IsEmpty()) return;
    EOS_Sessions_UnregisterPlayersOptions O = {};
    O.ApiVersion = EOS_SESSIONS_UNREGISTERPLAYERS_API_LATEST;
    O.SessionName = TCHAR_TO_UTF8(*Cfg->SessionName);
    O.PlayersToUnregister = Ids.GetData(); O.PlayersToUnregisterCount = (uint32_t)Ids.Num();
    SDK.fp_EOS_Sessions_UnregisterPlayers(Sess, &O, this, &UEOSSessionsSubsystem::OnUnregisterPlayersCb);
}

void UEOSSessionsSubsystem::SetAttribute(const FString& Key, const FString& Value)
{
    for (TPair<FString,FString>& A : PendingAttributes) { if (A.Key == Key) { A.Value = Value; return; } }
    PendingAttributes.Add({Key, Value});
}

void EOS_CALL UEOSSessionsSubsystem::OnUpdateSessionCb(const EOS_Sessions_UpdateSessionCallbackInfo* D)
{
    if (!D) return;
    auto* Self = static_cast<UEOSSessionsSubsystem*>(D->ClientData);
    if (!Self) return;
    if (D->ResultCode == EOS_Success)
    {
        Self->bSessionExists = true;
        Self->CurrentSession.SessionName = D->SessionName ? UTF8_TO_TCHAR(D->SessionName) : TEXT("");
        Self->CurrentSession.SessionId   = D->SessionId   ? UTF8_TO_TCHAR(D->SessionId)   : TEXT("");
        Self->CurrentSession.bIsActive   = true;
        UE_LOG(LogEOSSessions, Log, TEXT("Session updated: %s (%s)"), *Self->CurrentSession.SessionName, *Self->CurrentSession.SessionId);
        Self->OnSessionUpdated.Broadcast(Self->CurrentSession.SessionName, true);
    }
    else { UE_LOG(LogEOSSessions, Error, TEXT("UpdateSession failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); Self->OnSessionUpdated.Broadcast(TEXT(""), false); }
}
void EOS_CALL UEOSSessionsSubsystem::OnDestroySessionCb(const EOS_Sessions_DestroySessionCallbackInfo* D)
{
    if (!D) return;
    auto* Self = static_cast<UEOSSessionsSubsystem*>(D->ClientData);
    if (Self) { Self->bSessionExists = false; Self->CurrentSession = FEOSSessionInfo{}; }
    UE_LOG(LogEOSSessions, Log, TEXT("Session destroyed (result=%d)"), (int32)D->ResultCode);
}
void EOS_CALL UEOSSessionsSubsystem::OnStartSessionCb(const EOS_Sessions_StartSessionCallbackInfo* D)
{ if (D) UE_LOG(LogEOSSessions, Log, TEXT("StartSession: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); }
void EOS_CALL UEOSSessionsSubsystem::OnEndSessionCb(const EOS_Sessions_EndSessionCallbackInfo* D)
{ if (D) UE_LOG(LogEOSSessions, Log, TEXT("EndSession: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); }
void EOS_CALL UEOSSessionsSubsystem::OnRegisterPlayersCb(const EOS_Sessions_RegisterPlayersCallbackInfo* D)
{ if (D) UE_LOG(LogEOSSessions, Log, TEXT("RegisterPlayers: %s (reg=%u, sanct=%u)"), *FEOSSDKLoader::ResultToString(D->ResultCode), D->RegisteredPlayersCount, D->SanctionedPlayersCount); }
void EOS_CALL UEOSSessionsSubsystem::OnUnregisterPlayersCb(const EOS_Sessions_UnregisterPlayersCallbackInfo* D)
{ if (D) UE_LOG(LogEOSSessions, Log, TEXT("UnregisterPlayers: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); }
