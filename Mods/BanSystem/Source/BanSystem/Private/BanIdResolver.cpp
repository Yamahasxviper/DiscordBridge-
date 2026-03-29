// Copyright Yamahasxviper. All Rights Reserved.

#include "BanIdResolver.h"

// EOSIdHelper — custom plugin providing EOS Product User ID extraction via
// the UE5 OnlineServicesEOSGS V2 path (OnlineSubsystemEOS is disabled in
// Satisfactory, so this plugin replaces the DummyHeaders/FGOnlineHelpers path).
#include "EOSIdHelper.h"

// EOSBanSDK — custom EOS SDK wrapper that converts stored PUID strings back to
// EOS_ProductUserId handles, validates them via the EOS C SDK, and exposes the
// EOS platform handle for direct SDK calls.
#include "EOSBanSDK.h"

// EOSSystem — provides UEOSConnectSubsystem for cross-platform cache lookups.
#include "EOSConnectSubsystem.h"
#include "EOSTypes.h"

// Ban subsystem validators
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanIdResolver, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Type name used by the UE5 Steam online services plugin.
 * When a player authenticates through Steam, FUniqueNetIdRepl::GetType()
 * returns this FName.
 *
 * Reference: UE5 Online::Steam service implementation.
 */
static const FName SteamTypeName(TEXT("Steam"));

// ─────────────────────────────────────────────────────────────────────────────
//  FBanIdResolver
// ─────────────────────────────────────────────────────────────────────────────

FResolvedBanId FBanIdResolver::Resolve(const FUniqueNetIdRepl& UniqueId)
{
    FResolvedBanId Result;

    // ── Steam ────────────────────────────────────────────────────────────
    TryGetSteam64Id(UniqueId, Result.Steam64Id);

    // ── EOS / Epic Online Services ───────────────────────────────────────
    // Run this regardless of whether Steam already succeeded — a Steam player
    // who has linked their account to Epic has BOTH identities active.
    TryGetEOSProductUserId(UniqueId, Result.EOSProductUserId);

    if (!Result.IsValid())
    {
        UE_LOG(LogBanIdResolver, Verbose,
            TEXT("FBanIdResolver::Resolve — no recognised platform ID for type '%s' / raw '%s'"),
            UniqueId.IsValid() ? *UniqueId.GetType().ToString() : TEXT("<invalid>"),
            UniqueId.IsValid() ? *UniqueId.ToString()           : TEXT("<invalid>"));
    }

    return Result;
}

FString FBanIdResolver::GetIdTypeName(const FUniqueNetIdRepl& UniqueId)
{
    if (!UniqueId.IsValid()) return FString();
    return UniqueId.GetType().ToString();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Steam
// ─────────────────────────────────────────────────────────────────────────────
bool FBanIdResolver::TryGetSteam64Id(const FUniqueNetIdRepl& UniqueId,
                                     FString&                OutSteam64Id)
{
    if (!UniqueId.IsValid()) return false;

    // The UE5 Steam online services plugin stamps each ID it creates with the
    // type name "Steam".  Any other type name is not a Steam ID.
    if (UniqueId.GetType() != SteamTypeName)
        return false;

    // For Steam IDs, ToString() returns the raw 17-digit Steam64 decimal
    // string (e.g. "76561198000000000").  Validate the format before use.
    const FString Candidate = UniqueId.ToString();
    if (!USteamBanSubsystem::IsValidSteam64Id(Candidate))
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetSteam64Id: UniqueNetId type is 'Steam' but ToString() '%s' "
                 "failed Steam64 format validation."),
            *Candidate);
        return false;
    }

    OutSteam64Id = Candidate;
    UE_LOG(LogBanIdResolver, Verbose,
        TEXT("TryGetSteam64Id: resolved Steam64 ID '%s'"), *OutSteam64Id);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  EOS / Epic Online Services
// ─────────────────────────────────────────────────────────────────────────────
bool FBanIdResolver::TryGetEOSProductUserId(const FUniqueNetIdRepl& UniqueId,
                                            FString&                OutPUID)
{
    if (!UniqueId.IsValid()) return false;

    // EOSId::GetProductUserId is a CSS FactoryGame helper that understands the
    // internal representation of EOS accounts in the UE5 OnlineServices layer.
    // It extracts the 32-char hex EOS Product User ID from the opaque FAccountId
    // that the EOS online service embeds inside the FUniqueNetId.
    //
    // This succeeds for:
    //   • Players who connected directly via EOS/Epic Games Launcher
    //   • Steam players whose account is linked to an Epic account
    //       (CSS Satisfactory links Steam + Epic for crossplay)
    //
    // It returns false / empty for:
    //   • Offline/LAN players (Null online service)
    //   • Steam-only players with no linked Epic account
    FString Candidate;
    if (!EOSId::GetProductUserId(UniqueId, Candidate) || Candidate.IsEmpty())
        return false;

    // Validate that the returned string matches the 32-char lowercase hex
    // format expected by the EOS ban system.
    if (!UEOSBanSubsystem::IsValidEOSProductUserId(Candidate))
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetEOSProductUserId: EOSId::GetProductUserId returned '%s' "
                 "but it failed PUID format validation."),
            *Candidate);
        return false;
    }

#if WITH_EOS_SDK
    // Secondary validation via the custom EOS SDK:
    // EOSBanSDK::PUIDFromString() calls EOS_ProductUserId_FromString() to parse
    // the string into an EOS_ProductUserId handle, then IsValidHandle() calls
    // EOS_ProductUserId_IsValid() to confirm the EOS SDK considers it valid.
    // This cross-checks that the PUID produced by the UE OnlineServices layer
    // is accepted by the underlying EOS C SDK.
    {
        const EOS_ProductUserId PUIDHandle = EOSBanSDK::PUIDFromString(Candidate);
        if (!EOSBanSDK::IsValidHandle(PUIDHandle))
        {
            UE_LOG(LogBanIdResolver, Warning,
                TEXT("TryGetEOSProductUserId: PUID '%s' passed format validation but "
                     "EOS_ProductUserId_IsValid() returned false — discarding."),
                *Candidate);
            return false;
        }
    }
#endif // WITH_EOS_SDK

    OutPUID = Candidate;
    UE_LOG(LogBanIdResolver, Verbose,
        TEXT("TryGetEOSProductUserId: resolved EOS PUID '%s'"), *OutPUID);
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  EOSSystem cross-platform cache helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FBanIdResolver::TryGetCachedPUIDFromSteam64(UGameInstance* GameInstance,
                                                  const FString& Steam64Id,
                                                  FString&       OutPUID)
{
    if (!GameInstance || Steam64Id.IsEmpty()) return false;

    UEOSConnectSubsystem* EOS = GameInstance->GetSubsystem<UEOSConnectSubsystem>();
    if (!EOS) return false;

    const FString Cached = EOS->GetCachedPUIDForExternalAccountTyped(
        Steam64Id, EEOSExternalAccountType::Steam);

    if (Cached.IsEmpty()) return false;

    OutPUID = Cached;
    UE_LOG(LogBanIdResolver, Verbose,
        TEXT("TryGetCachedPUIDFromSteam64: EOSSystem cache hit — Steam64 '%s' → PUID '%s'"),
        *Steam64Id, *OutPUID);
    return true;
}

bool FBanIdResolver::TryGetCachedSteam64FromPUID(UGameInstance* GameInstance,
                                                   const FString& PUID,
                                                   FString&       OutSteam64Id)
{
    if (!GameInstance || PUID.IsEmpty()) return false;

    UEOSConnectSubsystem* EOS = GameInstance->GetSubsystem<UEOSConnectSubsystem>();
    if (!EOS) return false;

    const FString Cached = EOS->GetCachedSteam64ForPUID(PUID);
    if (Cached.IsEmpty()) return false;

    OutSteam64Id = Cached;
    UE_LOG(LogBanIdResolver, Verbose,
        TEXT("TryGetCachedSteam64FromPUID: EOSSystem cache hit — PUID '%s' → Steam64 '%s'"),
        *PUID, *OutSteam64Id);
    return true;
}
