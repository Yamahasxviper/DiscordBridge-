// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/database.ts

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "BanDatabase.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanDatabase, Log, All);

/**
 * UBanDatabase
 *
 * JSON-file-backed in-memory ban storage for the dedicated server.
 * Direct port of the BanDatabase class in Tools/BanSystem/src/database.ts.
 *
 * All bans are held in a TArray<FBanEntry> and written to a JSON file on
 * every change.  The JSON file schema mirrors the Node.js version exactly.
 *
 * This subsystem is thread-safe: all reads and writes are protected by an
 * internal mutex so that the REST API thread and the game thread can both
 * access the storage safely.
 */
UCLASS()
class BANSYSTEM_API UBanDatabase : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Write ─────────────────────────────────────────────────────────────

    /**
     * Insert or replace (upsert on uid) a ban record.
     * Returns true on success.
     */
    bool AddBan(const FBanEntry& Entry);

    /**
     * Remove a ban by compound UID ("EOS:xxx").
     * Returns true if a row was found and deleted.
     */
    bool RemoveBanByUid(const FString& Uid);

    /**
     * Remove a ban by its integer row ID.
     * Returns true if a row was found and deleted.
     */
    bool RemoveBanById(int64 Id);

    /**
     * Atomically look up and remove a ban by its integer row ID.
     * If a matching record is found it is removed, OutEntry is filled with the
     * deleted data, and the method returns true.  OutEntry is untouched on
     * failure.  Use this overload when the caller needs the deleted entry's
     * details (e.g. UID, player name) for notifications — it eliminates the
     * TOCTOU window that exists when GetAllBans() + RemoveBanById() are called
     * separately.
     */
    bool RemoveBanById(int64 Id, FBanEntry& OutEntry);

    /**
     * Delete all expired temporary bans.
     * Returns the number of bans removed.
     * When bSilent is true, OnBanRemoved is NOT broadcast for each removed entry
     * (use this during Initialize() to avoid sending Discord notifications for
     * bans that expired while the server was offline).
     */
    int32 PruneExpiredBans(bool bSilent = false);

    // ── Read ──────────────────────────────────────────────────────────────

    /**
     * Returns true and fills OutEntry if the UID is currently banned
     * (permanent or not yet expired).  Thread-safe.
     * Only the primary Uid field is checked.
     */
    bool IsCurrentlyBanned(const FString& Uid, FBanEntry& OutEntry) const;

    /**
     * Like IsCurrentlyBanned(), but also scans the LinkedUids array of every
     * ban record.  Use this in enforcement paths so that a ban issued for one
     * platform also blocks the same player under a linked identity.
     * Thread-safe.
     */
    bool IsCurrentlyBannedByAnyId(const FString& Uid, FBanEntry& OutEntry) const;

    /**
     * Returns the ban record for the UID regardless of expiry.
     * Returns false if no record exists.  Thread-safe.
     */
    bool GetBanByUid(const FString& Uid, FBanEntry& OutEntry) const;

    /**
     * All currently-active bans (permanent + unexpired temporary).
     * Thread-safe.
     */
    TArray<FBanEntry> GetActiveBans() const;

    /**
     * All ban rows including expired ones.
     * Thread-safe.
     */
    TArray<FBanEntry> GetAllBans() const;

    // ── Cross-platform linking ─────────────────────────────────────────────

    /**
     * Links two UIDs together so that a ban on one also blocks the other.
     *
     * Finds the ban record for UidA (or UidB) and adds the other UID to its
     * LinkedUids list.  If both UIDs have their own ban records the link is
     * added to both.  Returns true if at least one record was updated.
     *
     * Use /linkbans <UID1> <UID2> from the server console or an admin chat
     * command to associate two EOS PUID bans that belong to the same player.
     */
    bool LinkBans(const FString& UidA, const FString& UidB);

    /**
     * Removes the directional link from PrimaryUid's LinkedUids list.
     * Returns true if the link was found and removed from at least one record.
     */
    bool UnlinkBans(const FString& UidA, const FString& UidB);

    /**
     * Copies the live JSON file to BackupDir/bans_YYYY-MM-DD_HH-MM-SS.json.
     * Prunes backups beyond MaxKeep.
     * Returns the backup path on success, or an empty string on failure.
     */
    FString Backup(const FString& BackupDir, int32 MaxKeep = 5) const;

    // ── Helpers ───────────────────────────────────────────────────────────

    /** Build compound UID: "EOS:00020aed..." */
    static FString MakeUid(const FString& Platform, const FString& PlayerUID);

    /** Split a compound UID back into platform + raw player ID. */
    static void ParseUid(const FString& Uid, FString& OutPlatform, FString& OutPlayerUID);

    /** Returns the resolved path of the JSON ban file. */
    FString GetDatabasePath() const;

    /**
     * Re-reads bans.json if it has been modified on disk since the last load
     * or save.  Call this before any enforcement check so that manual edits to
     * the file (e.g. to remove a ban) take effect immediately without a server
     * restart.
     *
     * Must be called from the game thread.  The comparison is based on the
     * file's last-write timestamp; if the timestamp has not changed this
     * method returns immediately with no I/O.
     */
    void ReloadIfChanged();

    // ── Delegates (fired after every successful write) ─────────────────────

    /** Fired after a ban is successfully added (from any code path). */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBanAdded, const FBanEntry&);
    static FOnBanAdded OnBanAdded;

    /**
     * Fired after a ban is successfully removed.
     * @param Uid        Compound UID of the removed ban.
     * @param PlayerName Display name (may be empty for IP bans).
     */
    DECLARE_MULTICAST_DELEGATE_TwoParams(FOnBanRemoved, const FString& /*Uid*/,
                                          const FString& /*PlayerName*/);
    static FOnBanRemoved OnBanRemoved;

private:
    void LoadFromFile();
    bool SaveToFile() const;

    // Caller must already hold DbMutex.
    bool GetBanByUid_Locked(const FString& Uid, FBanEntry& OutEntry) const;

    TArray<FBanEntry>    Bans;
    int64                NextId = 1;
    FString              DbPath;

    /**
     * Last-write timestamp of DbPath as of the most recent load or save.
     * Compared in ReloadIfChanged() to detect external edits.
     * Mutable so SaveToFile() (which is const) can update it after writing.
     */
    mutable FDateTime    LastKnownFileModTime;

    /** Protects all in-memory and file operations so the HTTP thread and
     *  game thread can both access the ban list safely. */
    mutable FCriticalSection DbMutex;
};
