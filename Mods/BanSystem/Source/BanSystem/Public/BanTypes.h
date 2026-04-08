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

    /** Compound key: "EOS:00020aed...". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Uid;

    /** Raw EOS Product User ID. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString PlayerUID;

    /** "EOS" | "UNKNOWN" | "IP" */
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
     * (cross-platform identity, e.g. two EOS UIDs for the same person across accounts).
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

/**
 * A single warning record issued to a player by an admin.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FWarningEntry
{
    GENERATED_BODY()

    /** Auto-incremented integer primary key (0 when not yet persisted). */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    int64 Id = 0;

    /** Compound UID of the warned player: "EOS:xxx". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Uid;

    /** Display name at time of warning (informational, may be stale). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString PlayerName;

    /** Reason for the warning. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Reason;

    /** Admin username or "console" who issued the warning. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString WarnedBy;

    /** UTC timestamp when the warning was issued. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime WarnDate;

    /** UTC timestamp when the warning expires. Only used when bHasExpiry is true. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime ExpireDate;

    /** When true, this warning has an expiry time and will be ignored after ExpireDate. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    bool bHasExpiry = false;

    FWarningEntry()
        : Id(0)
        , WarnDate(FDateTime::UtcNow())
        , ExpireDate(FDateTime(0))
        , bHasExpiry(false)
    {}

    /** Returns true when this is a timed warning that has passed its expiry date. */
    bool IsExpired() const
    {
        if (!bHasExpiry) return false;
        return FDateTime::UtcNow() > ExpireDate;
    }
};

/**
 * A ban appeal submitted by a player via the REST API.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FBanAppealEntry
{
    GENERATED_BODY()

    /** Auto-incremented integer primary key (0 when not yet persisted). */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    int64 Id = 0;

    /** Compound UID of the appealing player: "EOS:xxx". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Uid;

    /** Player-supplied reason for the appeal. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Reason;

    /** Optional contact information (Discord handle, email, etc.). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString ContactInfo;

    /** UTC timestamp when the appeal was submitted. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime SubmittedAt;

    FBanAppealEntry()
        : Id(0)
        , SubmittedAt(FDateTime::UtcNow())
    {}
};

/**
 * A single audit log entry recording an admin action.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FAuditEntry
{
    GENERATED_BODY()

    /** Auto-incremented integer primary key (0 when not yet persisted). */
    UPROPERTY(BlueprintReadOnly, Category = "Ban System")
    int64 Id = 0;

    /**
     * Action type: "ban", "tempban", "unban", "kick", "warn", "clearwarns",
     * "whitelist_add", "whitelist_remove".
     */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Action;

    /** Compound UID of the affected player: "EOS:xxx". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString TargetUid;

    /** Display name of the affected player at time of action (may be empty). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString TargetName;

    /** Compound UID of the admin who performed the action, or "console". */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString AdminUid;

    /** Display name of the admin (may be empty for console actions). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString AdminName;

    /** Optional extra details, e.g. reason or duration string. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Details;

    /** UTC timestamp when the action was performed. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime Timestamp;

    FAuditEntry()
        : Id(0)
        , Timestamp(FDateTime::UtcNow())
    {}
};
