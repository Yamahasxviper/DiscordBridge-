// Copyright Yamahasxviper. All Rights Reserved.

#include "BanIdResolver.h"

// ── EOSSystem cross-platform cache ────────────────────────────────────────────
// EOSConnectSubsystem.h is included first so EOSSystem's eos_platform.h wins
// the EOS_Platform_H include-guard race before any other EOS SDK headers.
// All TryGetCached* calls guard against nullptr so this compiles and runs
// even when the EOSSystem mod is not installed.
#include "EOSConnectSubsystem.h"
#include "EOSTypes.h"

// ── EOS Product User ID extraction ───────────────────────────────────────────
// EOSId::GetProductUserId is provided by the CSS FactoryGame module (FACTORYGAME_API).
// It wraps all OnlineServicesEOSGS / EOSShared platform guards internally, so
// BanSystem never needs to include OnlineIdEOSGS.h, EOSShared.h, or eos_common.h
// directly.  This avoids the LNK2019 errors that would occur if those symbols
// were referenced in BanSystem's .obj when building for non-server targets where
// OnlineServicesEOSGS is not linked by BanSystem.
//
// On the CSS dedicated server, EOSId::GetProductUserId returns false (no V2
// FAccountId embedded in the remote player's FUniqueNetIdRepl).  EOS PUIDs are
// obtained server-side exclusively via the async
// EOSConnectSubsystem::LookupPUIDBySteam64() path.
#include "Online/FGOnlineHelpers.h"

#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanIdResolver, Log, All);

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

    // The UE5 Steam online services plugin stamps every Steam identity with the
    // type name "Steam".  Any other type is not a Steam64 ID — return false
    // silently so callers can try the next platform identity.
    static const FName SteamTypeName(TEXT("Steam"));
    if (UniqueId.GetType() != SteamTypeName)
        return false;

    // For Steam identities, FUniqueNetIdRepl::ToString() returns the raw 17-digit
    // Steam64 decimal string (e.g. "76561198000000000").  This is standard UE5
    // behaviour — no CSS mod helper is required.
    const FString Candidate = UniqueId.ToString();

    // Validate: Steam64 IDs are exactly 17 decimal digits starting with "7656119".
    // This matches the inline IsValidSteam64Id logic from FGOnlineHelpers.h but is
    // reproduced here so BanSystem has no compile-time dependency on that mod.
    if (Candidate.Len() != 17)
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetSteam64Id: UniqueNetId type is 'Steam' but ToString() '%s' "
                 "failed Steam64 format validation (expected 17 digits)."),
            *Candidate);
        return false;
    }
    for (TCHAR C : Candidate)
    {
        if (C < TEXT('0') || C > TEXT('9'))
        {
            UE_LOG(LogBanIdResolver, Warning,
                TEXT("TryGetSteam64Id: UniqueNetId type is 'Steam' but ToString() '%s' "
                     "failed Steam64 format validation (non-digit character)."),
                *Candidate);
            return false;
        }
    }
    if (!Candidate.StartsWith(TEXT("7656119")))
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetSteam64Id: UniqueNetId type is 'Steam' but ToString() '%s' "
                 "failed Steam64 format validation (unexpected prefix)."),
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

    // Delegate to the CSS FactoryGame helper (FACTORYGAME_API).
    // EOSId::GetProductUserId handles all platform guards internally (V2 FAccountId
    // via OnlineServicesEOSGS on non-server, or V1 EOS on legacy path).  Calling
    // through this exported function means BanSystem never references EOSGSS symbols
    // directly, avoiding LNK2019 errors when those libraries are absent.
    //
    // Returns false on the CSS dedicated server (no V2 FAccountId in remote
    // FUniqueNetIdRepl).  EOS PUIDs are obtained server-side via the async
    // EOSConnectSubsystem::LookupPUIDBySteam64() path instead.
    FString Candidate;
    if (!EOSId::GetProductUserId(UniqueId, Candidate)) return false;

    if (!EOSId::IsValidEOSProductUserId(Candidate))
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetEOSProductUserId: EOS returned PUID '%s' but it failed "
                 "hex format validation — discarding."),
            *Candidate);
        return false;
    }

    // Normalise to lowercase.  All stored EOS PUIDs are lowercased by
    // UEOSBanSubsystem::BanPlayer(), so callers that compare the returned
    // value against stored records must have a matching case.
    OutPUID = Candidate.ToLower();
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
