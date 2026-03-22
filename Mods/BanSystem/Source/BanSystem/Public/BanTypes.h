// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BanTypes.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
// Result of a ban-check call
// ─────────────────────────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EBanCheckResult : uint8
{
    NotBanned  UMETA(DisplayName = "Not Banned"),
    Banned     UMETA(DisplayName = "Banned"),
    BanExpired UMETA(DisplayName = "Ban Expired (auto-removed)"),
};

// ─────────────────────────────────────────────────────────────────────────────
// A single ban record — used by BOTH the Steam and EOS subsystems.
// Neither subsystem shares instances with the other.
// ─────────────────────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct BANSYSTEM_API FBanEntry
{
    GENERATED_BODY()

    /** Steam64 ID (e.g. "76561198000000000") or EOS Product User ID (32-char hex). */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString PlayerId;

    /** Human-readable reason shown to the player on kick. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString Reason;

    /** UTC timestamp when the ban was created. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime BannedAt;

    /**
     * UTC timestamp when the ban expires.
     * Set to FDateTime(0) (epoch) for permanent bans.
     */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FDateTime ExpiresAt;

    /** Name or identifier of the admin who issued the ban. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    FString BannedBy;

    /** True when the ban never expires. ExpiresAt is ignored when this is true. */
    UPROPERTY(BlueprintReadWrite, Category = "Ban System")
    bool bIsPermanent = true;

    FBanEntry()
        : BannedAt(FDateTime::UtcNow())
        , ExpiresAt(FDateTime(0))
        , BannedBy(TEXT("Server"))
        , bIsPermanent(true)
    {}

    /** Returns true if the ban has a finite duration AND that duration has passed. */
    bool IsExpired() const
    {
        if (bIsPermanent) return false;
        return FDateTime::UtcNow() > ExpiresAt;
    }

    /** Formats expiry for display. Returns "Permanent" or a UTC date-time string. */
    FString GetExpiryString() const
    {
        if (bIsPermanent) return TEXT("Permanent");
        return ExpiresAt.ToString(TEXT("%Y-%m-%d %H:%M:%S UTC"));
    }
};
