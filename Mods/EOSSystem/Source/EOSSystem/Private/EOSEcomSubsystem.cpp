// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSEcomSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSEcom, Log, All);

struct FEcomOwnerCbData { TWeakObjectPtr<UEOSEcomSubsystem> Sub; FString EpicAccountId; EOS_EpicAccountId AccountId; TArray<std::string> ItemStorage; };
struct FEcomEntCbData   { TWeakObjectPtr<UEOSEcomSubsystem> Sub; FString EpicAccountId; EOS_EpicAccountId AccountId; };

void UEOSEcomSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Collection.InitializeDependency<UEOSSystemSubsystem>(); Super::Initialize(Collection); }
void UEOSEcomSubsystem::Deinitialize() { EntitlementsCache.Empty(); OwnershipCache.Empty(); Super::Deinitialize(); }

EOS_HEcom UEOSEcomSubsystem::GetEcomHandle() const
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    auto* Sys = GetGameInstance()->GetSubsystem<UEOSSystemSubsystem>();
    if (!Sys || !Sys->GetPlatformHandle() || !SDK.fp_EOS_Platform_GetEcomInterface) return nullptr;
    return SDK.fp_EOS_Platform_GetEcomInterface(Sys->GetPlatformHandle());
}

void UEOSEcomSubsystem::QueryOwnership(const FString& EpicAccountId, const TArray<FString>& CatalogItemIds)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HEcom H = GetEcomHandle();
    if (!H || !SDK.fp_EOS_Ecom_QueryOwnership || !SDK.fp_EOS_EpicAccountId_FromString) return;
    EOS_EpicAccountId AccountId = SDK.fp_EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*EpicAccountId));
    if (!AccountId) return;
    auto* Cb = new FEcomOwnerCbData{ this, EpicAccountId, AccountId, {} };
    for (const FString& Id : CatalogItemIds) Cb->ItemStorage.Add(TCHAR_TO_UTF8(*Id));
    TArray<const char*> ItemPtrs;
    for (const std::string& S : Cb->ItemStorage) ItemPtrs.Add(S.c_str());
    EOS_Ecom_QueryOwnershipOptions O = {}; O.ApiVersion = EOS_ECOM_QUERYOWNERSHIP_API_LATEST;
    O.LocalUserId = AccountId; O.CatalogItemIds = ItemPtrs.GetData(); O.CatalogItemIdCount = (uint32_t)ItemPtrs.Num();
    SDK.fp_EOS_Ecom_QueryOwnership(H, &O, Cb, &UEOSEcomSubsystem::OnOwnershipCb);
}

void UEOSEcomSubsystem::QueryEntitlements(const FString& EpicAccountId)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HEcom H = GetEcomHandle();
    if (!H || !SDK.fp_EOS_Ecom_QueryEntitlements || !SDK.fp_EOS_EpicAccountId_FromString) return;
    EOS_EpicAccountId AccountId = SDK.fp_EOS_EpicAccountId_FromString(TCHAR_TO_UTF8(*EpicAccountId));
    if (!AccountId) return;
    EOS_Ecom_QueryEntitlementsOptions O = {}; O.ApiVersion = EOS_ECOM_QUERYENTITLEMENTS_API_LATEST;
    O.LocalUserId = AccountId; O.EntitlementNames = nullptr; O.EntitlementNameCount = 0; O.bIncludeRedeemed = EOS_TRUE;
    auto* Cb = new FEcomEntCbData{ this, EpicAccountId, AccountId };
    SDK.fp_EOS_Ecom_QueryEntitlements(H, &O, Cb, &UEOSEcomSubsystem::OnEntitlementsCb);
}

TArray<FEOSEntitlement> UEOSEcomSubsystem::GetCachedEntitlements(const FString& EpicAccountId) const
{ if (const auto* P = EntitlementsCache.Find(EpicAccountId)) return *P; return {}; }
int32 UEOSEcomSubsystem::GetEntitlementCount(const FString& EpicAccountId) const
{ if (const auto* P = EntitlementsCache.Find(EpicAccountId)) return P->Num(); return 0; }
bool UEOSEcomSubsystem::OwnsItem(const FString& EpicAccountId, const FString& CatalogItemId) const
{ if (const auto* P = OwnershipCache.Find(EpicAccountId)) return P->Contains(CatalogItemId); return false; }

void EOS_CALL UEOSEcomSubsystem::OnOwnershipCb(const EOS_Ecom_QueryOwnershipCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FEcomOwnerCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSEcomSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString AccountId = Cb->EpicAccountId; EOS_EpicAccountId EpicId = Cb->AccountId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSEcom, Warning, TEXT("QueryOwnership failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    TSet<FString>& Owned = Self->OwnershipCache.FindOrAdd(AccountId);
    for (uint32_t i = 0; i < D->ItemOwnershipCount; ++i)
    {
        const EOS_Ecom_ItemOwnership& Item = D->ItemOwnership[i];
        if (Item.OwnershipStatus == EOS_OS_Owned && Item.Id)
            Owned.Add(UTF8_TO_TCHAR(Item.Id));
    }
    UE_LOG(LogEOSEcom, Log, TEXT("Ownership for %s: %d items owned"), *AccountId, Owned.Num());
    Self->OnOwnershipQueried.Broadcast(AccountId, true);
}

void EOS_CALL UEOSEcomSubsystem::OnEntitlementsCb(const EOS_Ecom_QueryEntitlementsCallbackInfo* D)
{
    if (!D) return;
    auto* Cb = static_cast<FEcomEntCbData*>(D->ClientData);
    if (!Cb) return;
    UEOSEcomSubsystem* Self = Cb->Sub.IsValid() ? Cb->Sub.Get() : nullptr;
    FString AccountId = Cb->EpicAccountId; EOS_EpicAccountId EpicId = Cb->AccountId;
    delete Cb;
    if (D->ResultCode != EOS_Success || !Self) { UE_LOG(LogEOSEcom, Warning, TEXT("QueryEntitlements failed: %s"), *FEOSSDKLoader::ResultToString(D->ResultCode)); return; }
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    EOS_HEcom H = Self->GetEcomHandle();
    if (!H || !SDK.fp_EOS_Ecom_GetEntitlementsCount || !SDK.fp_EOS_Ecom_CopyEntitlementByIndex) return;
    EOS_Ecom_GetEntitlementsCountOptions CO = {}; CO.ApiVersion = EOS_ECOM_GETENTITLEMENTSCOUNT_API_LATEST; CO.LocalUserId = EpicId;
    uint32_t Count = SDK.fp_EOS_Ecom_GetEntitlementsCount(H, &CO);
    TArray<FEOSEntitlement> List;
    for (uint32_t i = 0; i < Count; ++i)
    {
        EOS_Ecom_CopyEntitlementByIndexOptions CopyO = {}; CopyO.ApiVersion = EOS_ECOM_COPYENTITLEMENTBYINDEX_API_LATEST; CopyO.LocalUserId = EpicId; CopyO.EntitlementIndex = i;
        EOS_Ecom_Entitlement* E = nullptr;
        if (SDK.fp_EOS_Ecom_CopyEntitlementByIndex(H, &CopyO, &E) == EOS_Success && E)
        {
            FEOSEntitlement Ent;
            Ent.EntitlementName = E->EntitlementName ? UTF8_TO_TCHAR(E->EntitlementName) : TEXT("");
            Ent.EntitlementId   = E->EntitlementId   ? UTF8_TO_TCHAR(E->EntitlementId)   : TEXT("");
            Ent.ItemId          = E->CatalogItemId   ? UTF8_TO_TCHAR(E->CatalogItemId)   : TEXT("");
            Ent.bRedeemed       = (E->bRedeemed == EOS_TRUE);
            Ent.EndTimestamp    = (int32)E->EndTimestamp;
            List.Add(Ent);
            if (SDK.fp_EOS_Ecom_Entitlement_Release) SDK.fp_EOS_Ecom_Entitlement_Release(E);
        }
    }
    Self->EntitlementsCache.Add(AccountId, List);
    UE_LOG(LogEOSEcom, Log, TEXT("Entitlements for %s: %d"), *AccountId, List.Num());
}
