// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSSanctionsSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "EOSSystemConfig.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSanctions, Log, All);

void UEOSSanctionsSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UEOSSystemSubsystem>();
    Super::Initialize(Collection);
}
void UEOSSanctionsSubsystem::Deinitialize() { SanctionCache.Empty(); Super::Deinitialize(); }

EOS_HSanctions UEOSSanctionsSubsystem::GetSanctionsHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetSanctionsInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetSanctionsInterface(Sys->GetPlatformHandle());
}

void UEOSSanctionsSubsystem::QuerySanctions(const FString& TargetPUID)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HSanctions H = GetSanctionsHandle();
    if (!H || !SDK.fp_EOS_Sanctions_QueryActivePlayerSanctions)
    { UE_LOG(LogEOSSanctions, Warning, TEXT("QuerySanctions: sanctions interface not ready")); return; }

    auto* Conn = GetGameInstance()->GetSubsystem<UEOSConnectSubsystem>();
    EOS_ProductUserId LocalId = nullptr;
    if (Conn && SDK.fp_EOS_ProductUserId_FromString && !Conn->GetLocalServerPUID().IsEmpty())
        LocalId = SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*Conn->GetLocalServerPUID()));

    EOS_ProductUserId TargetId = (SDK.fp_EOS_ProductUserId_FromString && !TargetPUID.IsEmpty())
        ? SDK.fp_EOS_ProductUserId_FromString(TCHAR_TO_UTF8(*TargetPUID)) : nullptr;
    if (!TargetId) { UE_LOG(LogEOSSanctions, Warning, TEXT("QuerySanctions: invalid TargetPUID '%s'"), *TargetPUID); return; }

    EOS_Sanctions_QueryActivePlayerSanctionsOptions O = {};
    O.ApiVersion    = EOS_SANCTIONS_QUERYACTIVEPLAYERSANCTIONS_API_LATEST;
    O.TargetUserId  = TargetId;
    O.LocalUserId   = LocalId;
    SDK.fp_EOS_Sanctions_QueryActivePlayerSanctions(H, &O, this, &UEOSSanctionsSubsystem::OnQueryCb);
    UE_LOG(LogEOSSanctions, Log, TEXT("QuerySanctions queued for PUID: %s"), *TargetPUID);
}

TArray<FEOSSanctionInfo> UEOSSanctionsSubsystem::GetCachedSanctions(const FString& PUID) const
{
    if (const auto* P = SanctionCache.Find(PUID)) return *P;
    return {};
}
bool UEOSSanctionsSubsystem::HasActiveSanction(const FString& PUID) const { return GetSanctionCount(PUID) > 0; }
int32 UEOSSanctionsSubsystem::GetSanctionCount(const FString& PUID) const
{
    if (const auto* P = SanctionCache.Find(PUID)) return P->Num();
    return 0;
}
void UEOSSanctionsSubsystem::ClearCachedSanctions(const FString& PUID) { SanctionCache.Remove(PUID); }

void EOS_CALL UEOSSanctionsSubsystem::OnQueryCb(const EOS_Sanctions_QueryActivePlayerSanctionsCallbackInfo* D)
{
    if (!D) return;
    auto* Self = static_cast<UEOSSanctionsSubsystem*>(D->ClientData);
    if (!Self) return;

    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (D->ResultCode != EOS_Success)
    {
        UE_LOG(LogEOSSanctions, Warning, TEXT("QuerySanctions failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode));
        return;
    }

    // Convert TargetUserId back to string
    FString TargetStr;
    if (D->TargetUserId && SDK.fp_EOS_ProductUserId_ToString)
    {
        char Buf[64] = {}; int32_t Len = 64;
        if (SDK.fp_EOS_ProductUserId_ToString(D->TargetUserId, Buf, &Len) == EOS_Success)
            TargetStr = UTF8_TO_TCHAR(Buf);
    }

    EOS_HSanctions H = Self->GetSanctionsHandle();
    TArray<FEOSSanctionInfo> Sanctions;

    if (H && SDK.fp_EOS_Sanctions_GetPlayerSanctionCount && SDK.fp_EOS_Sanctions_CopyPlayerSanctionByIndex)
    {
        EOS_Sanctions_GetPlayerSanctionCountOptions CO = {};
        CO.ApiVersion    = EOS_SANCTIONS_GETPLAYERSANCTIONCOUNT_API_LATEST;
        CO.TargetUserId  = D->TargetUserId;
        const uint32_t Count = SDK.fp_EOS_Sanctions_GetPlayerSanctionCount(H, &CO);

        for (uint32_t i = 0; i < Count; ++i)
        {
            EOS_Sanctions_CopyPlayerSanctionByIndexOptions CopyO = {};
            CopyO.ApiVersion    = EOS_SANCTIONS_COPYPLAYERSANCTIONBYINDEX_API_LATEST;
            CopyO.TargetUserId  = D->TargetUserId;
            CopyO.SanctionIndex = i;
            EOS_Sanctions_PlayerSanction* S = nullptr;
            if (SDK.fp_EOS_Sanctions_CopyPlayerSanctionByIndex(H, &CopyO, &S) == EOS_Success && S)
            {
                FEOSSanctionInfo Info;
                Info.Action      = S->Action      ? UTF8_TO_TCHAR(S->Action)      : TEXT("");
                Info.TimePlaced  = S->TimePlaced  ? UTF8_TO_TCHAR(S->TimePlaced)  : TEXT("");
                Info.ReferenceId = S->ReferenceId ? UTF8_TO_TCHAR(S->ReferenceId) : TEXT("");
                Sanctions.Add(Info);
                if (SDK.fp_EOS_Sanctions_PlayerSanction_Release) SDK.fp_EOS_Sanctions_PlayerSanction_Release(S);
            }
        }
    }

    Self->SanctionCache.Add(TargetStr, Sanctions);
    UE_LOG(LogEOSSanctions, Log, TEXT("Sanctions for %s: %d active"), *TargetStr, Sanctions.Num());
    Self->OnSanctionsQueried.Broadcast(TargetStr, Sanctions);
}
