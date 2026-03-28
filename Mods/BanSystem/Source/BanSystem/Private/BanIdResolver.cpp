// Copyright Yamahasxviper. All Rights Reserved.

#include "BanIdResolver.h"

// CSS FactoryGame helper for extracting EOS Product User IDs
#include "Online/FGOnlineHelpers.h"

// Ban subsystem validators
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"

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
            *GetIdTypeName(UniqueId),
            UniqueId.IsValid() && UniqueId.GetUniqueNetId().IsValid()
                ? *UniqueId->ToString() : TEXT("<invalid>"));
    }

    return Result;
}

FString FBanIdResolver::GetIdTypeName(const FUniqueNetIdRepl& UniqueId)
{
    if (!UniqueId.IsValid()) return FString();
    // Guard against a null underlying pointer even when IsValid() passes.
    const TSharedPtr<const FUniqueNetId> SharedId = UniqueId.GetUniqueNetId();
    if (!SharedId.IsValid()) return FString();
    return SharedId->GetType().ToString();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Steam
// ─────────────────────────────────────────────────────────────────────────────
bool FBanIdResolver::TryGetSteam64Id(const FUniqueNetIdRepl& UniqueId,
                                     FString&                OutSteam64Id)
{
    if (!UniqueId.IsValid()) return false;

    // Guard against a null underlying FUniqueNetId pointer.
    // In the CSS UE5 build, FUniqueNetIdRepl::IsValid() can return true based on a
    // cached string representation even when the inner TSharedPtr<FUniqueNetId> is
    // null (e.g. a player connecting with a partially-initialised network identity
    // where the string is replicated before the FUniqueNetId object is created).
    // Dereferencing with operator->() without this check causes a segmentation fault.
    const TSharedPtr<const FUniqueNetId> SharedId = UniqueId.GetUniqueNetId();
    if (!SharedId.IsValid()) return false;

    // The UE5 Steam online services plugin stamps each ID it creates with the
    // type name "Steam".  Any other type name is not a Steam ID.
    if (SharedId->GetType() != SteamTypeName)
        return false;

    // For Steam IDs, ToString() returns the raw 17-digit Steam64 decimal
    // string (e.g. "76561198000000000").  Validate the format before use.
    const FString Candidate = SharedId->ToString();
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

    // Same defensive check as TryGetSteam64Id – ensure the underlying pointer is
    // non-null before passing it to EOSId::GetProductUserId, which takes a
    // const FUniqueNetId& (dereferences UniqueId).
    const TSharedPtr<const FUniqueNetId> SharedId = UniqueId.GetUniqueNetId();
    if (!SharedId.IsValid()) return false;

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
    if (!EOSId::GetProductUserId(*SharedId, Candidate) || Candidate.IsEmpty())
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

    OutPUID = Candidate;
    UE_LOG(LogBanIdResolver, Verbose,
        TEXT("TryGetEOSProductUserId: resolved EOS PUID '%s'"), *OutPUID);
    return true;
}
