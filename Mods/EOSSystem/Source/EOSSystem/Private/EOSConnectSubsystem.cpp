// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSConnectSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSSystemConfig.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogEOSConnect, Log, All);

// ─── helpers ─────────────────────────────────────────────────────────────────
static FString PUIDToStr(EOS_ProductUserId Id)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (!Id || !SDK.fp_EOS_ProductUserId_ToString) return FString();
    char Buf[64] = {}; int32_t Len = 64;
    if (SDK.fp_EOS_ProductUserId_ToString(Id, Buf, &Len) == EOS_Success)
        return UTF8_TO_TCHAR(Buf);
    return FString();
}

static EOS_ProductUserId PUIDFromStr(const FString& S)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (S.IsEmpty() || !SDK.fp_EOS_ProductUserId_FromString) return nullptr;
    return SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*S));
}

// ─── lifecycle ───────────────────────────────────────────────────────────────
void UEOSConnectSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);

    PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(this, &UEOSConnectSubsystem::HandlePostLogin);
    LogoutHandle    = FGameModeEvents::GameModeLogoutEvent.AddUObject(this,    &UEOSConnectSubsystem::HandleLogout);

    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (Cfg && Cfg->bEnableConnect)
        LoginAsServer();
}

void UEOSConnectSubsystem::Deinitialize()
{
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);

    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (H && AuthExpiryNotif != EOS_INVALID_NOTIFICATIONID && SDK.fp_EOS_Connect_RemoveNotifyAuthExpiration)
    {
        SDK.fp_EOS_Connect_RemoveNotifyAuthExpiration(H, AuthExpiryNotif);
        AuthExpiryNotif = EOS_INVALID_NOTIFICATIONID;
    }
    Super::Deinitialize();
}

EOS_HConnect UEOSConnectSubsystem::GetConnectHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    UEOSSystemSubsystem* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetConnectInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetConnectInterface(Sys->GetPlatformHandle());
}

// ─── login ────────────────────────────────────────────────────────────────────
void UEOSConnectSubsystem::LoginAsServer()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_CreateDeviceId) { UE_LOG(LogEOSConnect, Warning, TEXT("LoginAsServer: Connect handle not ready")); return; }

    EOS_Connect_CreateDeviceIdOptions O = {};
    O.ApiVersion  = EOS_CONNECT_CREATEDEVICEID_API_LATEST;
    O.DeviceModel = "DedicatedServer";
    SDK.fp_EOS_Connect_CreateDeviceId(H, &O, this, &UEOSConnectSubsystem::OnCreateDeviceIdCallback);
}

void UEOSConnectSubsystem::PerformConnectLogin()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_Login) return;

    EOS_Connect_Credentials Creds = {};
    Creds.ApiVersion = EOS_CONNECT_CREDENTIALS_API_LATEST;
    Creds.Token      = nullptr;
    Creds.Type       = EOS_ECT_DEVICEID_ACCESS_TOKEN;

    EOS_Connect_UserLoginInfo Info = {};
    Info.ApiVersion  = EOS_CONNECT_USERLOGININFO_API_LATEST;
    Info.DisplayName = "EOSServer";

    EOS_Connect_LoginOptions LoginOpts = {};
    LoginOpts.ApiVersion    = EOS_CONNECT_LOGIN_API_LATEST;
    LoginOpts.Credentials   = &Creds;
    LoginOpts.UserLoginInfo = &Info;

    SDK.fp_EOS_Connect_Login(H, &LoginOpts, this, &UEOSConnectSubsystem::OnConnectLoginCallback);
}

// ─── PUID enumeration ────────────────────────────────────────────────────────
TArray<FString> UEOSConnectSubsystem::GetLoggedInPUIDs() const
{
    TArray<FString> Out;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetLoggedInUsersCount || !SDK.fp_EOS_Connect_GetLoggedInUserByIndex) return Out;
    const int32 Count = SDK.fp_EOS_Connect_GetLoggedInUsersCount(H);
    for (int32 i = 0; i < Count; ++i)
    {
        FString S = PUIDToStr(SDK.fp_EOS_Connect_GetLoggedInUserByIndex(H, i));
        if (!S.IsEmpty()) Out.Add(S);
    }
    return Out;
}

// ─── PUID lookup by external account ────────────────────────────────────────
void UEOSConnectSubsystem::LookupPUIDByExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType)
{
    InternalLookupBatch({ExternalAccountId}, static_cast<EOS_EExternalAccountType>(ExternalAccountType));
}

void UEOSConnectSubsystem::LookupPUIDByExternalAccountTyped(const FString& ExternalAccountId, EEOSExternalAccountType AccountType)
{
    InternalLookupBatch({ExternalAccountId}, static_cast<EOS_EExternalAccountType>(AccountType));
}

void UEOSConnectSubsystem::LookupPUIDBySteam64(const FString& Steam64Id)
{
    InternalLookupBatch({Steam64Id}, EOS_EAT_STEAM);
}

void UEOSConnectSubsystem::LookupPUIDBatch(const TArray<FString>& ExternalAccountIds, EEOSExternalAccountType AccountType)
{
    if (ExternalAccountIds.IsEmpty()) return;
    InternalLookupBatch(ExternalAccountIds, static_cast<EOS_EExternalAccountType>(AccountType));
}

void UEOSConnectSubsystem::InternalLookupBatch(const TArray<FString>& ExternalAccountIds, EOS_EExternalAccountType AccountIdType)
{
    if (ExternalAccountIds.IsEmpty()) return;
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_QueryExternalAccountMappings)
    {
        UE_LOG(LogEOSConnect, Warning, TEXT("LookupPUIDBatch: Connect not ready"));
        return;
    }
    EOS_ProductUserId LocalId = PUIDFromStr(LocalServerPUID);
    if (!LocalId) { UE_LOG(LogEOSConnect, Warning, TEXT("LookupPUIDBatch: no local server PUID yet")); return; }

    // Build a char* array; pointers into the FString data are valid for the scope of this call.
    TArray<const char*> IdPtrs;
    TArray<std::string> IdStorage;
    for (const FString& Id : ExternalAccountIds)
    {
        IdStorage.Add(TCHAR_TO_UTF8(*Id));
        IdPtrs.Add(IdStorage.Last().c_str());
    }

    EOS_Connect_QueryExternalAccountMappingsOptions O = {};
    O.ApiVersion            = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
    O.LocalUserId           = LocalId;
    O.AccountIdType         = AccountIdType;
    O.ExternalAccountIds    = IdPtrs.GetData();
    O.ExternalAccountIdCount= (uint32_t)IdPtrs.Num();
    SDK.fp_EOS_Connect_QueryExternalAccountMappings(H, &O, this, &UEOSConnectSubsystem::OnQueryExternalMappingsCallback);
    UE_LOG(LogEOSConnect, Log, TEXT("LookupPUIDBatch: querying %d account(s) of type %d"), IdPtrs.Num(), (int32)AccountIdType);
}

FString UEOSConnectSubsystem::GetCachedPUIDForExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType) const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetExternalAccountMapping) return FString();

    EOS_ProductUserId LocalId = PUIDFromStr(LocalServerPUID);
    if (!LocalId) return FString();

    EOS_Connect_GetExternalAccountMappingsOptions O = {};
    O.ApiVersion         = EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST;
    O.LocalUserId        = LocalId;
    O.AccountIdType      = static_cast<EOS_EExternalAccountType>(ExternalAccountType);
    O.TargetExternalUserId = TCHAR_TO_UTF8(*ExternalAccountId);

    EOS_ProductUserId Mapped = SDK.fp_EOS_Connect_GetExternalAccountMapping(H, &O);
    return PUIDToStr(Mapped);
}

FString UEOSConnectSubsystem::GetCachedPUIDForExternalAccountTyped(const FString& ExternalAccountId, EEOSExternalAccountType AccountType) const
{
    return GetCachedPUIDForExternalAccount(ExternalAccountId, static_cast<int32>(AccountType));
}

// ─── Reverse lookup: PUID → external accounts ────────────────────────────────
void UEOSConnectSubsystem::QueryExternalAccountsForPUID(const FString& PUID)
{
    InternalReverseLookupBatch({PUID});
}

void UEOSConnectSubsystem::QueryExternalAccountsForPUIDBatch(const TArray<FString>& PUIDs)
{
    if (PUIDs.IsEmpty()) return;
    InternalReverseLookupBatch(PUIDs);
}

void UEOSConnectSubsystem::InternalReverseLookupBatch(const TArray<FString>& PUIDs)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_QueryProductUserIdMappings)
    {
        UE_LOG(LogEOSConnect, Warning, TEXT("QueryExternalAccountsForPUID: Connect not ready"));
        return;
    }
    EOS_ProductUserId LocalId = PUIDFromStr(LocalServerPUID);
    if (!LocalId) { UE_LOG(LogEOSConnect, Warning, TEXT("QueryExternalAccountsForPUID: no local server PUID")); return; }

    TArray<EOS_ProductUserId> IdArr;
    for (const FString& S : PUIDs)
    {
        EOS_ProductUserId Id = PUIDFromStr(S);
        if (Id) IdArr.Add(Id);
    }
    if (IdArr.IsEmpty()) return;

    EOS_Connect_QueryProductUserIdMappingsOptions O = {};
    O.ApiVersion          = EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST;
    O.LocalUserId         = LocalId;
    O.AccountIdType_DEPRECATED = EOS_EAT_EPIC;
    O.ProductUserIds      = IdArr.GetData();
    O.ProductUserIdCount  = (uint32_t)IdArr.Num();
    SDK.fp_EOS_Connect_QueryProductUserIdMappings(H, &O, this, &UEOSConnectSubsystem::OnQueryProductUserIdMappingsCallback);
    UE_LOG(LogEOSConnect, Log, TEXT("QueryExternalAccountsForPUID: querying %d PUID(s)"), IdArr.Num());
}

TArray<FEOSExternalAccountInfo> UEOSConnectSubsystem::GetCachedExternalAccountsForPUID(const FString& PUID) const
{
    if (const auto* Arr = ReverseLookupCache.Find(PUID)) return *Arr;
    return {};
}

FString UEOSConnectSubsystem::GetCachedSteam64ForPUID(const FString& PUID) const
{
    if (const auto* Arr = ReverseLookupCache.Find(PUID))
        for (const FEOSExternalAccountInfo& Info : *Arr)
            if (Info.AccountType == EEOSExternalAccountType::Steam)
                return Info.AccountId;
    return FString();
}

// ─── tracked PUIDs ───────────────────────────────────────────────────────────
void UEOSConnectSubsystem::RegisterPlayerPUID(const FString& PUID)
{
    if (!TrackedPUIDs.Contains(PUID))
    {
        TrackedPUIDs.Add(PUID);
        OnPlayerPUIDRegistered.Broadcast(PUID);
        UE_LOG(LogEOSConnect, Log, TEXT("Registered PUID: %s"), *PUID);
    }
}

void UEOSConnectSubsystem::UnregisterPlayerPUID(const FString& PUID)
{
    if (TrackedPUIDs.Remove(PUID) > 0)
    {
        OnPlayerPUIDUnregistered.Broadcast(PUID);
        UE_LOG(LogEOSConnect, Log, TEXT("Unregistered PUID: %s"), *PUID);
    }
}

void UEOSConnectSubsystem::HandlePostLogin(AGameModeBase* GM, APlayerController* PC)
{
    if (!PC) return;
    if (ULocalPlayer* LP = PC->GetLocalPlayer())
    {
        const FString Id = LP->GetPreferredUniqueNetId().ToString();
        if (!Id.IsEmpty()) RegisterPlayerPUID(Id);
    }
}

void UEOSConnectSubsystem::HandleLogout(AGameModeBase* GM, AController* C)
{
    if (APlayerController* PC = Cast<APlayerController>(C))
        if (ULocalPlayer* LP = PC->GetLocalPlayer())
        {
            const FString Id = LP->GetPreferredUniqueNetId().ToString();
            if (!Id.IsEmpty()) UnregisterPlayerPUID(Id);
        }
}

// ─── callbacks ───────────────────────────────────────────────────────────────
void EOS_CALL UEOSConnectSubsystem::OnCreateDeviceIdCallback(const EOS_Connect_CreateDeviceIdCallbackInfo* Data)
{
    if (!Data) return;
    UEOSConnectSubsystem* Self = static_cast<UEOSConnectSubsystem*>(Data->ClientData);
    if (!Self) return;
    if (Data->ResultCode == EOS_Success || Data->ResultCode == EOS_Duplicate)
        Self->PerformConnectLogin();
    else
        UE_LOG(LogEOSConnect, Error, TEXT("CreateDeviceId failed: %s"), *FEOSSDKLoader::ResultToString(Data->ResultCode));
}

void EOS_CALL UEOSConnectSubsystem::OnConnectLoginCallback(const EOS_Connect_LoginCallbackInfo* Data)
{
    if (!Data) return;
    UEOSConnectSubsystem* Self = static_cast<UEOSConnectSubsystem*>(Data->ClientData);
    if (!Self) return;

    if (Data->ResultCode == EOS_Success)
    {
        Self->LocalServerPUID = PUIDToStr(Data->LocalUserId);
        UE_LOG(LogEOSConnect, Log, TEXT("Connect login OK. Server PUID: %s"), *Self->LocalServerPUID);

        FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
        EOS_HConnect H = Self->GetConnectHandle();
        if (H && SDK.fp_EOS_Connect_AddNotifyAuthExpiration)
        {
            EOS_Connect_AddNotifyAuthExpirationOptions EO = {};
            EO.ApiVersion = EOS_CONNECT_ADDNOTIFYAUTHEXPIRATION_API_LATEST;
            Self->AuthExpiryNotif = SDK.fp_EOS_Connect_AddNotifyAuthExpiration(H, &EO, Self, &UEOSConnectSubsystem::OnAuthExpirationCallback);
        }
    }
    else
        UE_LOG(LogEOSConnect, Error, TEXT("Connect login failed: %s"), *FEOSSDKLoader::ResultToString(Data->ResultCode));
}

void EOS_CALL UEOSConnectSubsystem::OnAuthExpirationCallback(const EOS_Connect_AuthExpirationCallbackInfo* Data)
{
    if (!Data) return;
    UEOSConnectSubsystem* Self = static_cast<UEOSConnectSubsystem*>(Data->ClientData);
    if (Self) { UE_LOG(LogEOSConnect, Warning, TEXT("EOS auth expiring — re-logging in")); Self->PerformConnectLogin(); }
}

void EOS_CALL UEOSConnectSubsystem::OnQueryExternalMappingsCallback(const EOS_Connect_QueryExternalAccountMappingsCallbackInfo* Data)
{
    if (!Data) return;
    UEOSConnectSubsystem* Self = static_cast<UEOSConnectSubsystem*>(Data->ClientData);
    if (!Self) return;

    if (Data->ResultCode != EOS_Success)
    {
        UE_LOG(LogEOSConnect, Warning, TEXT("QueryExternalAccountMappings failed: %s"), *FEOSSDKLoader::ResultToString(Data->ResultCode));
        return;
    }

    // Fetch cached mappings and broadcast for each
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = Self->GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetExternalAccountMapping) return;

    for (uint32_t i = 0; i < Data->ExternalAccountIdCount; ++i)
    {
        const char* ExtId = Data->ExternalAccountIds[i];
        if (!ExtId) continue;

        EOS_Connect_GetExternalAccountMappingsOptions GO = {};
        GO.ApiVersion            = EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST;
        GO.LocalUserId           = Data->LocalUserId;
        GO.AccountIdType         = Data->AccountIdType;
        GO.TargetExternalUserId  = ExtId;

        EOS_ProductUserId Mapped = SDK.fp_EOS_Connect_GetExternalAccountMapping(H, &GO);
        FString PUID             = PUIDToStr(Mapped);
        FString ExtIdStr         = UTF8_TO_TCHAR(ExtId);

        UE_LOG(LogEOSConnect, Log, TEXT("PUID lookup: external='%s' -> PUID='%s'"), *ExtIdStr, *PUID);
        Self->OnPUIDLookupComplete.Broadcast(ExtIdStr, PUID);

        // Auto-register into tracked list if valid
        if (!PUID.IsEmpty()) Self->RegisterPlayerPUID(PUID);
    }
}

// ─── Reverse-lookup callback: PUID → external accounts ──────────────────────
void EOS_CALL UEOSConnectSubsystem::OnQueryProductUserIdMappingsCallback(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data)
{
    if (!Data) return;
    UEOSConnectSubsystem* Self = static_cast<UEOSConnectSubsystem*>(Data->ClientData);
    if (!Self) return;

    if (Data->ResultCode != EOS_Success)
    {
        UE_LOG(LogEOSConnect, Warning, TEXT("QueryProductUserIdMappings failed: %s"), *FEOSSDKLoader::ResultToString(Data->ResultCode));
        return;
    }

    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = Self->GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetProductUserExternalAccountCount || !SDK.fp_EOS_Connect_CopyProductUserExternalAccountByIndex) return;

    // Enumerate every PUID that was just queried — the EOS SDK caches results per-PUID.
    // We query by iterating logged-in users and any PUID we have tracked.
    TArray<FString> AllPUIDs = Self->GetLoggedInPUIDs();
    for (const FString& TP : Self->TrackedPUIDs)
        AllPUIDs.AddUnique(TP);

    for (const FString& PUIDStr : AllPUIDs)
    {
        EOS_ProductUserId PUIDHandle = PUIDFromStr(PUIDStr);
        if (!PUIDHandle) continue;

        EOS_Connect_GetProductUserExternalAccountCountOptions CountOpts = {};
        CountOpts.ApiVersion  = EOS_CONNECT_GETPRODUCTUSEREXTERNALACCOUNTCOUNT_API_LATEST;
        CountOpts.TargetUserId = PUIDHandle;
        const uint32_t Count = SDK.fp_EOS_Connect_GetProductUserExternalAccountCount(H, &CountOpts);

        TArray<FEOSExternalAccountInfo> Infos;
        for (uint32_t Idx = 0; Idx < Count; ++Idx)
        {
            EOS_Connect_CopyProductUserExternalAccountByIndexOptions CopyOpts = {};
            CopyOpts.ApiVersion               = EOS_CONNECT_COPYPRODUCTUSEREXTERNALACCOUNTBYINDEX_API_LATEST;
            CopyOpts.TargetUserId             = PUIDHandle;
            CopyOpts.ExternalAccountInfoIndex = Idx;

            EOS_Connect_ExternalAccountInfo* Info = nullptr;
            if (SDK.fp_EOS_Connect_CopyProductUserExternalAccountByIndex(H, &CopyOpts, &Info) == EOS_Success && Info)
            {
                FEOSExternalAccountInfo UInfo;
                UInfo.AccountId      = Info->AccountId      ? UTF8_TO_TCHAR(Info->AccountId)    : TEXT("");
                UInfo.DisplayName    = Info->DisplayName    ? UTF8_TO_TCHAR(Info->DisplayName)   : TEXT("");
                UInfo.AccountType    = static_cast<EEOSExternalAccountType>(Info->AccountIdType);
                UInfo.LastLoginTime  = (int64)Info->LastLoginTime;

                UE_LOG(LogEOSConnect, Log, TEXT("Reverse lookup: PUID='%s' -> external='%s' (type %d)"),
                    *PUIDStr, *UInfo.AccountId, (int32)UInfo.AccountType);

                Self->OnReverseLookupComplete.Broadcast(PUIDStr, UInfo.AccountId, UInfo.AccountType);
                Infos.Add(UInfo);

                if (SDK.fp_EOS_Connect_ExternalAccountInfo_Release)
                    SDK.fp_EOS_Connect_ExternalAccountInfo_Release(Info);
            }
        }
        if (Infos.Num() > 0)
            Self->ReverseLookupCache.Add(PUIDStr, MoveTemp(Infos));
    }
}
