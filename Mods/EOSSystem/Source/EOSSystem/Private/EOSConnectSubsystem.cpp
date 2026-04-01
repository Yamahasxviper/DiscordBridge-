// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSConnectSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSSystemConfig.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
// FGOnlineHelpers — EOSId::GetProductUserId(FUniqueNetIdRepl) handles both the
// V2 (FAccountId/EOSGSS) and V1 (OnlineSubsystemEOS) identity paths internally,
// with all OnlineServicesEOSGS guards encapsulated there.
#include "Online/FGOnlineHelpers.h"
// JSON serialisation — Dom, Serialization (Json module) + FJsonObjectConverter (JsonUtilities)
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "JsonObjectConverter.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include <string>

DEFINE_LOG_CATEGORY_STATIC(LogEOSConnect, Log, All);

// ─── QueryExternalAccountMappings context ────────────────────────────────────
// EOS_Connect_QueryExternalAccountMappingsCallbackInfo does not carry the
// queried IDs or account type back in the callback.  We store them here and
// pass the struct as ClientData so the callback can call
// EOS_Connect_GetExternalAccountMapping for each ID.
struct FQueryExternalMappingsContext
{
    TWeakObjectPtr<UEOSConnectSubsystem> Subsystem;
    TArray<std::string>                 ExternalAccountIds;   // UTF-8 copies kept alive
    EOS_EExternalAccountType            AccountIdType;
};

// ─── QueryProductUserIdMappings context ──────────────────────────────────────
// Carries the set of PUIDs that were the SUBJECT of a specific reverse-lookup
// query so the callback can fire OnReverseLookupAllComplete only for those
// PUIDs rather than for every tracked PUID processed by the callback.
// This prevents false "no Steam account" notifications for PUIDs whose OWN
// query has not yet completed (avoids a race condition).
struct FReverseLookupContext
{
    TWeakObjectPtr<UEOSConnectSubsystem> Subsystem;
    TArray<FString>                      QueriedPUIDs;
};

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

static EOS_ProductUserId Connect_PUIDFromStr(const FString& S)
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

    // Restore any PUID↔external-account mappings persisted from a previous session.
    LoadCacheFromDisk();

    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (Cfg && Cfg->bEnableConnect)
        LoginAsServer();
}

void UEOSConnectSubsystem::Deinitialize()
{
    // Persist the reverse-lookup cache so Steam64↔PUID mappings survive restarts.
    SaveCacheToDisk();

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
    EOS_ProductUserId LocalId = Connect_PUIDFromStr(LocalServerPUID);
    if (!LocalId) { UE_LOG(LogEOSConnect, Warning, TEXT("LookupPUIDBatch: no local server PUID yet")); return; }

    // Build a char* array whose backing storage lives in the context struct so
    // it remains valid until the async callback fires and deletes the context.
    auto* Ctx = new FQueryExternalMappingsContext();
    Ctx->Subsystem     = this;
    Ctx->AccountIdType = AccountIdType;

    TArray<const char*> IdPtrs;
    for (const FString& Id : ExternalAccountIds)
    {
        Ctx->ExternalAccountIds.Add(TCHAR_TO_UTF8(*Id));
        IdPtrs.Add(Ctx->ExternalAccountIds.Last().c_str());
    }

    EOS_Connect_QueryExternalAccountMappingsOptions O = {};
    O.ApiVersion            = EOS_CONNECT_QUERYEXTERNALACCOUNTMAPPINGS_API_LATEST;
    O.LocalUserId           = LocalId;
    O.AccountIdType         = AccountIdType;
    O.ExternalAccountIds    = IdPtrs.GetData();
    O.ExternalAccountIdCount= (uint32_t)IdPtrs.Num();
    SDK.fp_EOS_Connect_QueryExternalAccountMappings(H, &O, Ctx, &UEOSConnectSubsystem::OnQueryExternalMappingsCallback);
    UE_LOG(LogEOSConnect, Log, TEXT("LookupPUIDBatch: querying %d account(s) of type %d"), IdPtrs.Num(), (int32)AccountIdType);
}

FString UEOSConnectSubsystem::GetCachedPUIDForExternalAccount(const FString& ExternalAccountId, int32 ExternalAccountType) const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetExternalAccountMapping) return FString();

    EOS_ProductUserId LocalId = Connect_PUIDFromStr(LocalServerPUID);
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
    EOS_ProductUserId LocalId = Connect_PUIDFromStr(LocalServerPUID);
    if (!LocalId) { UE_LOG(LogEOSConnect, Warning, TEXT("QueryExternalAccountsForPUID: no local server PUID")); return; }

    TArray<EOS_ProductUserId> IdArr;
    for (const FString& S : PUIDs)
    {
        EOS_ProductUserId Id = Connect_PUIDFromStr(S);
        if (Id) IdArr.Add(Id);
    }
    if (IdArr.IsEmpty()) return;

    EOS_Connect_QueryProductUserIdMappingsOptions O = {};
    O.ApiVersion          = EOS_CONNECT_QUERYPRODUCTUSERIDMAPPINGS_API_LATEST;
    O.LocalUserId         = LocalId;
    O.AccountIdType_DEPRECATED = EOS_EAT_EPIC;
    O.ProductUserIds      = IdArr.GetData();
    O.ProductUserIdCount  = (uint32_t)IdArr.Num();

    // Pass the queried PUIDs in a heap-allocated context so the callback can
    // broadcast OnReverseLookupAllComplete ONLY for the PUIDs that were the
    // subject of THIS specific query.  Ownership is transferred to the callback.
    FReverseLookupContext* Ctx = new FReverseLookupContext();
    Ctx->Subsystem    = this;
    Ctx->QueriedPUIDs = PUIDs;

    SDK.fp_EOS_Connect_QueryProductUserIdMappings(H, &O, Ctx, &UEOSConnectSubsystem::OnQueryProductUserIdMappingsCallback);
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
    // Public UFUNCTION path: no associated PlayerController available.
    RegisterPlayerPUIDInternal(PUID, nullptr);
}

void UEOSConnectSubsystem::RegisterPlayerPUIDInternal(const FString& PUID, APlayerController* PC)
{
    if (!TrackedPUIDs.Contains(PUID))
    {
        TrackedPUIDs.Add(PUID);
        OnPlayerPUIDRegistered.Broadcast(PUID, PC);
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
    if (!PC || !PC->PlayerState) return;

    // Delegate PUID extraction to EOSId::GetProductUserId (FGOnlineHelpers module).
    // This handles both V2 (FAccountId/EOSGSS, guarded inside FGOnlineHelpers) and
    // V1 (OnlineSubsystemEOS) paths internally.
    //
    // We do NOT use GetLocalPlayer() — on a dedicated server every remote player has
    // GetLocalPlayer() == nullptr.  We also do NOT call UniqueId.ToString() — for a
    // V2 FAccountId that returns an opaque handle string, not the 32-char hex PUID.
    //
    // For Steam players without a linked Epic account (V1 "Steam" type),
    // GetProductUserId returns false; those players are handled by
    // BanEnforcementSubsystem's async Steam64→PUID lookup path.

    const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
    if (!UniqueId.IsValid()) return;

    FString PUID;
    if (EOSId::GetProductUserId(UniqueId, PUID))
    {
        // Pass the PlayerController so BanEnforcementSubsystem::OnPUIDRegistered
        // can act directly without a world scan.
        RegisterPlayerPUIDInternal(PUID, PC);
    }
}

void UEOSConnectSubsystem::HandleLogout(AGameModeBase* GM, AController* C)
{
    APlayerController* PC = Cast<APlayerController>(C);
    if (!PC || !PC->PlayerState) return;

    // Mirror the logic from HandlePostLogin: extract the PUID the same way
    // we registered it so that TrackedPUIDs stays consistent.
    const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
    if (!UniqueId.IsValid()) return;

    FString PUID;
    if (EOSId::GetProductUserId(UniqueId, PUID))
        UnregisterPlayerPUID(PUID);
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
    auto* Ctx = static_cast<FQueryExternalMappingsContext*>(Data->ClientData);
    UEOSConnectSubsystem* Self = Ctx ? Ctx->Subsystem.Get() : nullptr;

    if (Data->ResultCode != EOS_Success)
    {
        UE_LOG(LogEOSConnect, Warning, TEXT("QueryExternalAccountMappings failed: %s"), *FEOSSDKLoader::ResultToString(Data->ResultCode));
        delete Ctx;
        return;
    }

    if (!Self)
    {
        delete Ctx;
        return;
    }

    // Fetch cached mappings and broadcast for each
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HConnect H = Self->GetConnectHandle();
    if (!H || !SDK.fp_EOS_Connect_GetExternalAccountMapping)
    {
        delete Ctx;
        return;
    }

    for (const std::string& ExtIdUtf8 : Ctx->ExternalAccountIds)
    {
        const char* ExtId = ExtIdUtf8.c_str();
        if (!ExtId || ExtIdUtf8.empty()) continue;

        EOS_Connect_GetExternalAccountMappingsOptions GO = {};
        GO.ApiVersion            = EOS_CONNECT_GETEXTERNALACCOUNTMAPPINGS_API_LATEST;
        GO.LocalUserId           = Data->LocalUserId;
        GO.AccountIdType         = Ctx->AccountIdType;
        GO.TargetExternalUserId  = ExtId;

        EOS_ProductUserId Mapped = SDK.fp_EOS_Connect_GetExternalAccountMapping(H, &GO);
        FString PUID             = PUIDToStr(Mapped);
        FString ExtIdStr         = UTF8_TO_TCHAR(ExtId);

        UE_LOG(LogEOSConnect, Log, TEXT("PUID lookup: external='%s' -> PUID='%s'"), *ExtIdStr, *PUID);
        Self->OnPUIDLookupComplete.Broadcast(ExtIdStr, PUID);

        // Auto-register into tracked list if valid
        if (!PUID.IsEmpty()) Self->RegisterPlayerPUID(PUID);
    }

    delete Ctx;
}

// ─── Reverse-lookup callback: PUID → external accounts ──────────────────────
void EOS_CALL UEOSConnectSubsystem::OnQueryProductUserIdMappingsCallback(const EOS_Connect_QueryProductUserIdMappingsCallbackInfo* Data)
{
    if (!Data) return;

    // Retrieve and take ownership of the context (always delete, even on failure).
    FReverseLookupContext* Ctx = static_cast<FReverseLookupContext*>(Data->ClientData);
    UEOSConnectSubsystem* Self = Ctx ? Ctx->Subsystem.Get() : nullptr;
    TArray<FString> QueriedPUIDs = Ctx ? MoveTemp(Ctx->QueriedPUIDs) : TArray<FString>();
    delete Ctx;

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
        EOS_ProductUserId PUIDHandle = Connect_PUIDFromStr(PUIDStr);
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

    // Persist the updated cache immediately so mappings survive a crash or fast restart.
    Self->SaveCacheToDisk();

    // Fire the "all complete" notification for each PUID that was the SUBJECT of
    // this specific query.  This allows subscribers (e.g. BanEnforcementSubsystem)
    // to clean up pending work for PUIDs that turned out to have no Steam account,
    // WITHOUT prematurely cancelling pending work for OTHER PUIDs whose own query
    // has not yet completed (avoids race conditions with concurrent queries).
    for (const FString& PUID : QueriedPUIDs)
    {
        Self->OnReverseLookupAllComplete.Broadcast(PUID);
    }
}

// ─── JSON cache persistence ──────────────────────────────────────────────────
void UEOSConnectSubsystem::SaveIdCache()
{
    SaveCacheToDisk();
}

void UEOSConnectSubsystem::SaveCacheToDisk() const
{
    if (ReverseLookupCache.IsEmpty()) return;

    TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> CacheArray;

    for (const auto& Pair : ReverseLookupCache)
    {
        TSharedRef<FJsonObject> Entry = MakeShared<FJsonObject>();
        Entry->SetStringField(TEXT("PUID"), Pair.Key);

        TArray<TSharedPtr<FJsonValue>> AccountsArr;
        for (const FEOSExternalAccountInfo& Info : Pair.Value)
        {
            TSharedPtr<FJsonObject> AccObj = MakeShared<FJsonObject>();
            if (FJsonObjectConverter::UStructToJsonObject(FEOSExternalAccountInfo::StaticStruct(), &Info, AccObj.ToSharedRef(), 0, 0))
                AccountsArr.Add(MakeShared<FJsonValueObject>(AccObj));
        }
        Entry->SetArrayField(TEXT("accounts"), AccountsArr);
        CacheArray.Add(MakeShared<FJsonValueObject>(Entry));
    }
    Root->SetArrayField(TEXT("cache"), CacheArray);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (FJsonSerializer::Serialize(Root, Writer))
    {
        const FString Path = FPaths::ProjectSavedDir() / TEXT("EOSSystem/IdCache.json");
        IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true);
        FFileHelper::SaveStringToFile(JsonStr, *Path);
        UE_LOG(LogEOSConnect, Verbose, TEXT("Saved %d PUID cache entries to %s"), ReverseLookupCache.Num(), *Path);
    }
}

void UEOSConnectSubsystem::LoadCacheFromDisk()
{
    const FString Path = FPaths::ProjectSavedDir() / TEXT("EOSSystem/IdCache.json");
    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *Path)) return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    const TArray<TSharedPtr<FJsonValue>>* CacheArray;
    if (!Root->TryGetArrayField(TEXT("cache"), CacheArray)) return;

    int32 Loaded = 0;
    for (const TSharedPtr<FJsonValue>& EntryVal : *CacheArray)
    {
        const TSharedPtr<FJsonObject>* EntryObj;
        if (!EntryVal->TryGetObject(EntryObj)) continue;

        FString PUID;
        if (!(*EntryObj)->TryGetStringField(TEXT("PUID"), PUID) || PUID.IsEmpty()) continue;

        const TArray<TSharedPtr<FJsonValue>>* AccountsArr;
        if (!(*EntryObj)->TryGetArrayField(TEXT("accounts"), AccountsArr)) continue;

        TArray<FEOSExternalAccountInfo> Infos;
        for (const TSharedPtr<FJsonValue>& AccVal : *AccountsArr)
        {
            const TSharedPtr<FJsonObject>* AccObj;
            if (!AccVal->TryGetObject(AccObj)) continue;
            FEOSExternalAccountInfo Info;
            if (FJsonObjectConverter::JsonObjectToUStruct(AccObj->ToSharedRef(), &Info, 0, 0))
                Infos.Add(Info);
        }
        if (!Infos.IsEmpty())
        {
            ReverseLookupCache.Add(PUID, MoveTemp(Infos));
            ++Loaded;
        }
    }
    UE_LOG(LogEOSConnect, Log, TEXT("Loaded %d PUID→ExternalAccount cache entries from %s"), Loaded, *Path);
}
