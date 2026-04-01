// Copyright Yamahasxviper. All Rights Reserved.

#include "BanIdResolver.h"

// ── EOSSystem headers first ────────────────────────────────────────────────
// Including EOSConnectSubsystem.h (which pulls EOSSDK/eos_sdk.h → eos_platform.h)
// BEFORE EOSDirectSDK.h (which pulls EOSDirectSDK's eos_platform.h) ensures that
// EOSSystem's eos_platform.h wins the EOS_Platform_H include-guard race.
// EOSSystem's version is preferred because it also does #include <eos_types.h>,
// making EOS_Platform_Options and other platform-creation types available.
// EOSSystem — provides UEOSConnectSubsystem for cross-platform cache lookups.
#include "EOSConnectSubsystem.h"
#include "EOSTypes.h"

// ── Player identity extraction ─────────────────────────────────────────────
// FGOnlineHelpers — the standard CSS Satisfactory mod helper module used by
// every Alpakit C++ mod in this project.  Provides:
//   • EOSId::GetProductUserId  — extract EOS PUID (FGONLINEHELPERS_API, non-inline)
//   • EOSId::GetSteam64Id      — extract Steam64 ID (FGONLINEHELPERS_API, non-inline)
//   • EOSId::IsValidSteam64Id  — validate Steam64 format string (inline)
//   • EOSId::IsValidEOSProductUserId — validate EOS PUID format string (inline)
// Non-inline exports ensure EOS SDK DLL-import symbols are resolved inside
// FGOnlineHelpers.dll rather than in this module's .obj file (avoids LNK2019).
#include "Online/FGOnlineHelpers.h"

// EOSDirectSDK — direct EOS C SDK access: PUIDFromString, PUIDToString,
// IsValidHandle and the registered EOS_HPlatform handle.
#include "EOSDirectSDK.h"

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

    // Delegate to the centralised FGOnlineHelpers helper which checks the
    // "Steam" type name and validates the 17-digit Steam64 format.
    // The type check is performed before format validation inside GetSteam64Id;
    // any non-Steam identity type returns false silently.
    if (!EOSId::GetSteam64Id(UniqueId, OutSteam64Id))
    {
        // Log a specific warning only when the type IS "Steam" but the string
        // failed format validation — that is an unexpected malformed identity.
        static const FName SteamTypeName(TEXT("Steam"));
        if (UniqueId.GetType() == SteamTypeName)
        {
            UE_LOG(LogBanIdResolver, Warning,
                TEXT("TryGetSteam64Id: UniqueNetId type is 'Steam' but ToString() '%s' "
                     "failed Steam64 format validation."),
                *UniqueId.ToString());
        }
        return false;
    }

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
    if (!EOSId::IsValidEOSProductUserId(Candidate))
    {
        UE_LOG(LogBanIdResolver, Warning,
            TEXT("TryGetEOSProductUserId: EOSId::GetProductUserId returned '%s' "
                 "but it failed PUID format validation."),
            *Candidate);
        return false;
    }

#if WITH_EOS_SDK
    // Secondary validation via EOSDirectSDK:
    // EOSDirectSDK::PUIDFromString() calls EOS_ProductUserId_FromString() to parse
    // the string into an EOS_ProductUserId handle, then IsValidHandle() calls
    // EOS_ProductUserId_IsValid() to confirm the EOS SDK considers it valid.
    // This cross-checks that the PUID produced by the UE OnlineServices layer
    // is accepted by the underlying EOS C SDK.
    {
        const EOS_ProductUserId PUIDHandle = EOSDirectSDK::PUIDFromString(Candidate);
        if (!EOSDirectSDK::IsValidHandle(PUIDHandle))
        {
            UE_LOG(LogBanIdResolver, Warning,
                TEXT("TryGetEOSProductUserId: PUID '%s' passed format validation but "
                     "EOS_ProductUserId_IsValid() returned false — discarding."),
                *Candidate);
            return false;
        }
    }
#endif // WITH_EOS_SDK

    // Normalise to lowercase before returning.  All stored EOS PUIDs are
    // lowercased by UEOSBanSubsystem::BanPlayer(), so callers that compare
    // the returned value against stored records (e.g. OnEOSPlayerBanned)
    // must have a matching case to avoid false negatives.
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
