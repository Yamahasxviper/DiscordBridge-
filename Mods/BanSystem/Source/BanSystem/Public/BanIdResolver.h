// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/OnlineReplStructs.h"  // FUniqueNetIdRepl
#include "BanIdResolver.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Platform identity result
// ─────────────────────────────────────────────────────────────────────────────

/**
 * The platform back-end that authenticated this player's connection.
 *
 * Satisfactory uses the CSS custom UE5 OnlineServices layer.  A single player
 * may have *both* a Steam identity AND an EOS Product User ID simultaneously
 * (Steam + linked Epic account).  The two identities are tracked independently.
 */
UENUM(BlueprintType)
enum class EBanIdPlatform : uint8
{
    /** No recognised platform identity could be determined. */
    Unknown UMETA(DisplayName = "Unknown"),

    /** Player authenticated through Steam; Steam64 ID is valid. */
    Steam   UMETA(DisplayName = "Steam"),

    /**
     * Player authenticated through Epic Online Services.
     * EOS Product User ID (PUID) is valid.
     * Note: a Steam player whose account is linked to Epic will also have
     * a valid PUID, so a player can be BOTH Steam and EOS simultaneously.
     */
    EOS     UMETA(DisplayName = "EOS (Epic Online Services)"),
};

/**
 * FResolvedBanId
 *
 * The complete ban-system identity for one connecting player.
 * Both fields may be populated at once for a Steam player who has linked
 * their account to Epic Games.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FResolvedBanId
{
    GENERATED_BODY()

    /**
     * Steam 64-bit ID in decimal form (e.g. "76561198000000000").
     * Non-empty only when the player connected through Steam.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    FString Steam64Id;

    /**
     * EOS Product User ID — 32 lowercase hex characters
     * (e.g. "00020aed06f0a6958c3c067fb4b73d51").
     * Non-empty for any player with an active EOS session, including
     * Steam players whose account is linked to Epic.
     */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    FString EOSProductUserId;

    /** True when a Steam64 ID was successfully resolved. */
    bool HasSteamId()  const { return !Steam64Id.IsEmpty(); }

    /** True when an EOS PUID was successfully resolved. */
    bool HasEOSPuid()  const { return !EOSProductUserId.IsEmpty(); }

    /** True when at least one platform identity was resolved. */
    bool IsValid()     const { return HasSteamId() || HasEOSPuid(); }
};

// ─────────────────────────────────────────────────────────────────────────────
//  FBanIdResolver
// ─────────────────────────────────────────────────────────────────────────────

/**
 * FBanIdResolver
 *
 * Stateless utility that resolves a player's ban-relevant identities from
 * their network UniqueNetId.
 *
 * How Satisfactory handles player IDs:
 *
 *   STEAM
 *   ─────
 *   When a player connects via Steam the CSS online layer creates a
 *   FUniqueNetIdRepl whose GetType() FName equals "Steam".
 *   Calling ToString() on such an ID returns the raw 17-digit Steam 64-bit
 *   decimal string (e.g. "76561198000000000").
 *
 *   EOS / Epic Online Services
 *   ──────────────────────────
 *   CSS uses Epic's OnlineServices v2 layer.  Each player with an active EOS
 *   session has an "EOS Product User ID" (PUID) — a 32-char lowercase hex
 *   string (e.g. "00020aed06f0a6958c3c067fb4b73d51").
 *   The PUID is extracted via the FactoryGame EOSId::GetProductUserId()
 *   helper rather than from ToString(), because the raw UniqueNetId string
 *   representation for EOS IDs is not the PUID.
 *
 *   A Steam player who has linked their account to Epic will have BOTH a
 *   Steam64 ID AND a valid EOS PUID.  This resolver returns both.
 *
 * Usage:
 *   FResolvedBanId Ids = FBanIdResolver::Resolve(PlayerState->GetUniqueNetId());
 *   if (Ids.HasSteamId())  CheckSteamBan(Ids.Steam64Id);
 *   if (Ids.HasEOSPuid())  CheckEOSBan(Ids.EOSProductUserId);
 */
class BANSYSTEM_API FBanIdResolver
{
public:
    /**
     * Resolve all available platform identities for the given UniqueNetId.
     * Both Steam64Id and EOSProductUserId may be populated simultaneously.
     */
    static FResolvedBanId Resolve(const FUniqueNetIdRepl& UniqueId);

    /**
     * Attempt to extract a Steam 64-bit ID.
     *
     * Returns true and populates OutSteam64Id when the UniqueNetId belongs
     * to a Steam player (GetType() == "Steam") and its ToString() value
     * passes the 17-digit Steam64 format check.
     */
    static bool TryGetSteam64Id(const FUniqueNetIdRepl& UniqueId,
                                FString&                OutSteam64Id);

    /**
     * Attempt to extract an EOS Product User ID (PUID).
     *
     * Uses the CSS FactoryGame EOSId::GetProductUserId() helper, which
     * understands the internal UE5 OnlineServices ID representation.
     * Returns true and populates OutPUID when the player has an active EOS
     * session (including Steam players with a linked Epic account).
     */
    static bool TryGetEOSProductUserId(const FUniqueNetIdRepl& UniqueId,
                                       FString&                OutPUID);

    /**
     * Returns the type-name string from a UniqueNetId (e.g. "Steam", "EOS").
     * Returns empty string when the ID is invalid.
     */
    static FString GetIdTypeName(const FUniqueNetIdRepl& UniqueId);
};
