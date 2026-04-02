// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/database.ts

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "SQLiteDatabase.h"
#include "BanDatabase.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanDatabase, Log, All);

/**
 * UBanDatabase
 *
 * Unified SQLite-backed ban storage for the dedicated server.
 * Direct port of the BanDatabase class in Tools/BanSystem/src/database.ts.
 *
 * Schema (identical to the Node.js version):
 *   id          INTEGER PRIMARY KEY AUTOINCREMENT
 *   uid         TEXT UNIQUE  -- "STEAM:xxx" or "EOS:xxx"
 *   playerUID   TEXT         -- raw platform ID
 *   platform    TEXT         -- "STEAM" | "EOS" | "UNKNOWN"
 *   playerName  TEXT
 *   reason      TEXT
 *   bannedBy    TEXT
 *   banDate     TEXT         -- ISO-8601 UTC
 *   expireDate  TEXT         -- ISO-8601 UTC, NULL for permanent bans
 *   isPermanent INTEGER      -- 1 = permanent, 0 = temporary
 *
 * This subsystem is thread-safe: all reads and writes are protected by an
 * internal mutex so that the REST API thread and the game thread can both
 * access the database safely.
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
     * Remove a ban by compound UID ("STEAM:xxx" or "EOS:xxx").
     * Returns true if a row was found and deleted.
     */
    bool RemoveBanByUid(const FString& Uid);

    /**
     * Remove a ban by its integer row ID.
     * Returns true if a row was found and deleted.
     */
    bool RemoveBanById(int64 Id);

    /**
     * Delete all expired temporary bans.
     * Returns the number of bans removed.
     */
    int32 PruneExpiredBans();

    // ── Read ──────────────────────────────────────────────────────────────

    /**
     * Returns true and fills OutEntry if the UID is currently banned
     * (permanent or not yet expired).  Thread-safe.
     */
    bool IsCurrentlyBanned(const FString& Uid, FBanEntry& OutEntry) const;

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

    // ── Backup ────────────────────────────────────────────────────────────

    /**
     * Copies the live database to BackupDir/bans_YYYY-MM-DD_HH-MM-SS.db.
     * Prunes backups beyond MaxKeep.
     * Returns the backup path on success, or an empty string on failure.
     */
    FString Backup(const FString& BackupDir, int32 MaxKeep = 5) const;

    // ── Helpers ───────────────────────────────────────────────────────────

    /** Build compound UID: "STEAM:76561198..." or "EOS:00020aed..." */
    static FString MakeUid(const FString& Platform, const FString& PlayerUID);

    /** Split a compound UID back into platform + raw player ID. */
    static void ParseUid(const FString& Uid, FString& OutPlatform, FString& OutPlayerUID);

    /** Returns the resolved path of the database file. */
    FString GetDatabasePath() const;

private:
    void ApplySchema();
    bool RowToEntry(FSQLitePreparedStatement& Stmt, FBanEntry& OutEntry) const;

    // Internal read helpers that assume DbMutex is already held.
    bool GetBanByUid_Locked(const FString& Uid, FBanEntry& OutEntry) const;
    TArray<FBanEntry> QueryRows_Locked(const TCHAR* Sql, const FString& Param1 = TEXT(""), const FString& Param2 = TEXT("")) const;

    FSQLiteDatabase Db;
    FString         DbPath;

    /** Protects all SQLite operations so the HTTP thread and game thread
     *  can both access the database safely. */
    mutable FCriticalSection DbMutex;
};
