// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "PlayerSessionRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerSessionRegistry, Log, All);

/**
 * A single recorded player session — the compound UID seen at join time
 * together with the player's display name and a timestamp.
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FPlayerSessionRecord
{
    GENERATED_BODY()

    /** Compound UID: "EOS:xxx" */
    UPROPERTY(BlueprintReadOnly, Category = "BanSystem")
    FString Uid;

    /** Display name at time of join (informational; may be stale or duplicated). */
    UPROPERTY(BlueprintReadOnly, Category = "BanSystem")
    FString DisplayName;

    /** ISO-8601 UTC timestamp of the most recent join with this UID. */
    UPROPERTY(BlueprintReadOnly, Category = "BanSystem")
    FString LastSeen;

    /** Remote IP address recorded at the most recent join (may be empty for legacy records). */
    UPROPERTY(BlueprintReadOnly, Category = "BanSystem")
    FString IpAddress;
};

/**
 * UPlayerSessionRegistry
 *
 * Lightweight session-tracking subsystem that persists a JSON file
 * (player_sessions.json, same directory as bans.json) mapping known
 * compound UIDs to the player display names seen at join time.
 *
 * Purpose (Gap 4 — identity persistence):
 *   If an EOS player reconnects under a different Product User ID (e.g. after
 *   an account migration), the registry lets admins discover the old UID via
 *   the /playerhistory command, cross-check it against the ban database, and
 *   issue a new ban or use /linkbans to associate the two UIDs.
 *
 *   The registry does NOT automatically ban players — PUID collisions from
 *   common display names would create false positives.  It is an audit tool.
 *
 * BanEnforcer calls RecordSession() each time a player successfully passes the
 * ban check, keeping the history up to date without exposing it to game logic.
 */
UCLASS()
class BANSYSTEM_API UPlayerSessionRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Record (or update) the most-recent-seen timestamp for a UID + display name.
     * Thread-safe.  Called from the game thread by BanEnforcer after a successful
     * ban check.
     */
    void RecordSession(const FString& Uid, const FString& DisplayName, const FString& IpAddress = FString());

    /**
     * Returns all session records whose display name contains the given substring
     * (case-insensitive).  Useful for /playerhistory <name>.
     */
    TArray<FPlayerSessionRecord> FindByName(const FString& NameSubstring) const;

    /**
     * Returns the session record for the given UID, or an empty record if not found.
     * Useful for /playerhistory <UID>.
     */
    bool FindByUid(const FString& Uid, FPlayerSessionRecord& OutRecord) const;

    /**
     * Returns all session records whose IP address contains the given substring.
     * Useful for !playerhistory <IP>.
     */
    TArray<FPlayerSessionRecord> FindByIp(const FString& IpSubstring) const;

    /**
     * All known session records, sorted by LastSeen descending.
     */
    TArray<FPlayerSessionRecord> GetAllRecords() const;

    /**
     * Removes all session records whose LastSeen timestamp is older than
     * DaysToKeep days.  Returns the number of records that were removed.
     * Thread-safe.
     */
    int32 PruneOldRecords(int32 DaysToKeep);

private:
    void LoadFromFile();
    bool SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FPlayerSessionRecord> Records;
    mutable FCriticalSection     Mutex;
    FString                      FilePath;
};
