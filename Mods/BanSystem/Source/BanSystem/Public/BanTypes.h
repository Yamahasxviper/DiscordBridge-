// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/models.ts

#pragma once

#include "CoreMinimal.h"
#include "BanTypes.generated.h"

/**
 * A single ban record — mirrors the SQLite schema in database.ts and
 * the BanRecord interface in models.ts exactly.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FBanEntry
{
    GENERATED_BODY()

    /** Auto-incremented integer primary key (0 when not yet persisted). */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    int64 Id = 0;

    /** Compound key: "STEAM:76561198012345678" or "EOS:00020aed...". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Uid;

    /** Raw platform user ID (Steam64 or EOS Product User ID). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString PlayerUID;

    /** "STEAM" | "EOS" | "UNKNOWN" */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Platform;

    /** Human-readable display name at time of ban (informational, may be stale). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString PlayerName;

    /** Why this player was banned. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Reason;

    /** Admin username or "system". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString BannedBy;

    /** UTC timestamp when the ban was created. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime BanDate;

    /**
     * UTC timestamp when the ban expires.
     * FDateTime(0) (epoch) means the ban is permanent.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime ExpireDate;

    /** true = permanent ban; false = temporary (ExpireDate is valid). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    bool bIsPermanent = true;

    /**
     * Optional list of additional compound UIDs that are linked to this ban
     * (cross-platform identity, e.g. "STEAM:xxx" and "EOS:yyy" for the same person).
     *
     * When UBanDatabase::IsCurrentlyBannedByAnyId() is called, it searches both
     * the primary Uid and every entry in this list.  Use /linkbans to associate
     * two UIDs and /unlinkbans to remove the association.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    TArray<FString> LinkedUids;

    FBanEntry()
        : Id(0)
        , BanDate(FDateTime::UtcNow())
        , ExpireDate(FDateTime(0))
        , bIsPermanent(true)
    {}

    /** Returns true if this is a temporary ban that has already passed its expiry. */
    bool IsExpired() const
    {
        if (bIsPermanent) return false;
        return FDateTime::UtcNow() > ExpireDate;
    }

    /**
     * Returns true if the given compound UID matches this ban's primary Uid or
     * any entry in LinkedUids (case-insensitive).
     */
    bool MatchesUid(const FString& InUid) const
    {
        if (Uid.Equals(InUid, ESearchCase::IgnoreCase)) return true;
        for (const FString& L : LinkedUids)
            if (L.Equals(InUid, ESearchCase::IgnoreCase)) return true;
        return false;
    }

    /** Human-readable message shown to the player when they are kicked. */
    FString GetKickMessage() const
    {
        if (bIsPermanent)
        {
            return FString::Printf(
                TEXT("You have been permanently banned. Reason: %s"), *Reason);
        }
        return FString::Printf(
            TEXT("You are banned until %s UTC. Reason: %s"),
            *ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
            *Reason);
    }
};
