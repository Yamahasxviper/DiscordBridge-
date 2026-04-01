// Copyright Yamahasxviper. All Rights Reserved.

#include "BanIdResolver.h"

// ── EOSSystem cross-platform cache ────────────────────────────────────────────
// EOSConnectSubsystem.h is included first so EOSSystem's eos_platform.h wins
// the EOS_Platform_H include-guard race before our own EOS SDK headers below.
// All TryGetCached* calls are guarded with nullptr checks so this code
// compiles and runs even when the EOSSystem mod is not installed.
#include "EOSConnectSubsystem.h"
#include "EOSTypes.h"

// ── EOS Product User ID extraction ────────────────────────────────────────────
// All EOS SDK calls in this file are confined to this single translation unit
// (BanIdResolver.cpp).  Keeping them here means the DLL-import stubs for
// EOS_ProductUserId_IsValid and LexToString(EOS_ProductUserId) are emitted
// in exactly one .obj file, preventing LNK2019 unresolved-external errors in
// other BanSystem translation units.
//
// WITH_ONLINE_SERVICES_EOSGSS is set by BanSystem.Build.cs:
//   1 on non-server targets  (OnlineServicesEOSGS is available)
//   0 on server targets      (OnlineServicesEOSGS is absent on the server)
#if WITH_ONLINE_SERVICES_EOSGSS

// EOS C SDK types: EOS_ProductUserId, EOS_ProductUserId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#ifndef EOS_CALL
#  if defined(_WIN32)
#    define EOS_CALL __cdecl
#  else
#    define EOS_CALL
#  endif
#endif
#ifndef EOS_MEMORY_CALL
#  define EOS_MEMORY_CALL EOS_CALL
#endif
#ifndef EOS_USE_DLLEXPORT
#  define EOS_USE_DLLEXPORT 0
#endif
#include "eos_common.h"

// UE5 V2 — UE::Online::GetProductUserId(FAccountId) -> EOS_ProductUserId
#include "Online/OnlineIdEOSGS.h"

// EOSShared — LexToString(EOS_ProductUserId) -> FString
#include "EOSShared.h"

#endif // WITH_ONLINE_SERVICES_EOSGSS

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

    // ── V2 path: FAccountId (OnlineServicesEOSGS) ─────────────────────────────
    // Every Satisfactory player with an active EOS session holds a V2 FAccountId.
    // OnlineServicesEOSGS is absent on the Linux dedicated server (CSS
    // TargetDenyList=["Server"]), so this path is compiled only for non-server
    // targets (WITH_ONLINE_SERVICES_EOSGSS=1, set by BanSystem.Build.cs).
    //
    // UE::Online::GetProductUserId(FAccountId) maps the opaque FAccountId to the
    // EOS_ProductUserId that the EOS SDK recognises for ban-list lookups.
    //
    // This path succeeds for:
    //   • Players who connected directly via EOS / Epic Games Launcher
    //   • Steam players whose account is linked to an Epic account
    //       (Satisfactory links Steam + Epic for crossplay)
    //
    // It returns false for:
    //   • Offline / LAN players (Null online service)
    //   • Steam-only players with no linked Epic account
#if WITH_ONLINE_SERVICES_EOSGSS
    if (UniqueId.IsV2())
    {
        const UE::Online::FAccountId& AccountId = UniqueId.GetV2Unsafe();
        if (!AccountId.IsValid()) return false;

        const EOS_ProductUserId ProductUserId = UE::Online::GetProductUserId(AccountId);
        if (!EOS_ProductUserId_IsValid(ProductUserId)) return false;

        // LexToString converts EOS_ProductUserId to the 32-char lowercase hex
        // string used throughout the ban system (e.g. "00020aed06f0a6958c3c067fb4b73d51").
        FString Candidate = LexToString(ProductUserId);
        if (Candidate.IsEmpty()) return false;

        // Validate 32-char lowercase hex format.
        if (Candidate.Len() != 32)
        {
            UE_LOG(LogBanIdResolver, Warning,
                TEXT("TryGetEOSProductUserId: EOS returned PUID '%s' but it is not "
                     "32 characters — discarding."),
                *Candidate);
            return false;
        }
        const FString Lower = Candidate.ToLower();
        for (TCHAR C : Lower)
        {
            const bool bDigit = (C >= TEXT('0') && C <= TEXT('9'));
            const bool bHex   = (C >= TEXT('a') && C <= TEXT('f'));
            if (!bDigit && !bHex)
            {
                UE_LOG(LogBanIdResolver, Warning,
                    TEXT("TryGetEOSProductUserId: EOS returned PUID '%s' "
                         "but it failed hex format validation — discarding."),
                    *Candidate);
                return false;
            }
        }

        // Normalise to lowercase before returning.  All stored EOS PUIDs are
        // lowercased by UEOSBanSubsystem::BanPlayer(), so callers that compare
        // the returned value against stored records must have a matching case.
        OutPUID = Lower;
        UE_LOG(LogBanIdResolver, Verbose,
            TEXT("TryGetEOSProductUserId: resolved EOS PUID '%s'"), *OutPUID);
        return true;
    }
#endif // WITH_ONLINE_SERVICES_EOSGSS

    return false;
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
