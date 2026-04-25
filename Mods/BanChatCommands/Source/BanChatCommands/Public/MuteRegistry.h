// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "MuteRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogMuteRegistry, Log, All);

/**
 * A single mute record for a player.
 */
USTRUCT(BlueprintType)
struct BANCHATCOMMANDS_API FMuteEntry
{
    GENERATED_BODY()

    /** Compound UID of the muted player: "EOS:xxx". */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString Uid;

    /** Display name at time of mute. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString PlayerName;

    /** Reason for the mute. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString Reason;

    /** Admin who issued the mute. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString MutedBy;

    /** UTC timestamp when the mute was issued. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FDateTime MuteDate;

    /**
     * UTC timestamp when the mute expires.
     * FDateTime(0) (epoch) means the mute is indefinite.
     */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FDateTime ExpireDate;

    /** true when the mute has no expiry time. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    bool bIsIndefinite = true;

    FMuteEntry()
        : MuteDate(FDateTime::UtcNow())
        , ExpireDate(FDateTime(0))
        , bIsIndefinite(true)
    {}

    /** Returns true if this is a timed mute that has already expired. */
    bool IsExpired() const
    {
        if (bIsIndefinite) return false;
        return FDateTime::UtcNow() > ExpireDate;
    }
};

// ── Delegates ────────────────────────────────────────────────────────────────

/** Fired (on the game thread) when a player is muted. */
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnPlayerMutedDelegate,
                                     const FMuteEntry& /*Entry*/,
                                     bool              /*bIsTimed*/);

/** Fired (on the game thread) when a player is unmuted (by admin or expiry). */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerUnmutedDelegate,
                                    const FString& /*Uid*/);

/**
 * UMuteRegistry
 *
 * GameInstance subsystem that tracks in-game chat mutes.
 * Persists state to Saved/BanChatCommands/mutes.json so mutes
 * survive server restarts.  Timed mutes auto-expire via TickExpiry().
 *
 * Thread-safe: all public methods acquire the internal Mutex.
 */
UCLASS()
class BANCHATCOMMANDS_API UMuteRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ───────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Mute a player.
     * ExpiryMinutes == 0 creates an indefinite mute.
     * Saves to disk immediately.
     * Thread-safe.
     */
    void MutePlayer(const FString& Uid, const FString& PlayerName,
                    const FString& Reason, const FString& MutedBy,
                    int32 ExpiryMinutes = 0);

    /**
     * Lift an active mute by compound UID (case-insensitive).
     * Returns true if a mute was found and removed.
     * Thread-safe.
     */
    bool UnmutePlayer(const FString& Uid);

    /**
     * Returns true when the player is currently muted (and the mute has
     * not expired).
     * Thread-safe.
     */
    bool IsMuted(const FString& Uid) const;

    /**
     * Populate OutEntry with the active mute record for the given UID.
     * Returns false when the player is not muted (or the mute has expired).
     * Thread-safe.
     */
    bool GetMuteEntry(const FString& Uid, FMuteEntry& OutEntry) const;

    /**
     * Returns every active (non-expired) mute entry.
     * Thread-safe.
     */
    TArray<FMuteEntry> GetAllMutes() const;

    /**
     * Updates the reason on an existing (non-expired) mute entry in-place.
     * Finds the entry by compound UID (case-insensitive), updates the Reason
     * field, and saves to disk.
     * Returns true when a matching active mute was found and updated.
     * Thread-safe.
     */
    bool UpdateMuteReason(const FString& Uid, const FString& NewReason);

    /**
     * Expire and remove any timed mutes whose ExpireDate has passed.
     * Called periodically by BanChatCommandsModule (every ~30 s).
     * Thread-safe.
     */
    TArray<FString> TickExpiry();

    // ── Delegates ────────────────────────────────────────────────────────────

    /** Fired on the game thread whenever a player is muted. */
    FOnPlayerMutedDelegate   OnPlayerMuted;

    /** Fired on the game thread whenever a player is unmuted. */
    FOnPlayerUnmutedDelegate OnPlayerUnmuted;

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FMuteEntry> Mutes;
    mutable FCriticalSection Mutex;
    FString FilePath;
};
