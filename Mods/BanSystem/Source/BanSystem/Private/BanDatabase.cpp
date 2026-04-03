// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/database.ts

#include "BanDatabase.h"
#include "SQLitePreparedStatement.h"
#include "Misc/Paths.h"
#include "Misc/ConfigCacheIni.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogBanDatabase);

// ── Column indices for  SELECT * FROM bans ───────────────────────────────────
// Must match the CREATE TABLE column order exactly.
namespace BanCol
{
    static constexpr int32 Id          = 0;
    static constexpr int32 Uid         = 1;
    static constexpr int32 PlayerUID   = 2;
    static constexpr int32 Platform    = 3;
    static constexpr int32 PlayerName  = 4;
    static constexpr int32 Reason      = 5;
    static constexpr int32 BannedBy    = 6;
    static constexpr int32 BanDate     = 7;
    static constexpr int32 ExpireDate  = 8;
    static constexpr int32 IsPermanent = 9;
}

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanDatabase::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    DbPath = GetDatabasePath();

    // Ensure the directory exists.
    const FString Dir = FPaths::GetPath(DbPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    // Open or create the database.
    if (!Db.Open(*DbPath, ESQLiteDatabaseOpenMode::ReadWriteCreate))
    {
        UE_LOG(LogBanDatabase, Error,
            TEXT("BanDatabase: failed to open %s"), *DbPath);
        return;
    }

    ApplySchema();

    const int32 Pruned = PruneExpiredBans();
    UE_LOG(LogBanDatabase, Log,
        TEXT("BanDatabase: opened %s — pruned %d expired ban(s)"), *DbPath, Pruned);
}

void UBanDatabase::Deinitialize()
{
    FScopeLock Lock(&DbMutex);
    if (Db.IsValid())
        Db.Close();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Schema
// ─────────────────────────────────────────────────────────────────────────────

void UBanDatabase::ApplySchema()
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return;

    // WAL mode: allow concurrent reads while a write is in progress.
    Db.Execute(TEXT("PRAGMA journal_mode = WAL;"));
    Db.Execute(TEXT("PRAGMA foreign_keys = ON;"));

    // Main table — schema mirrors Tools/BanSystem/src/database.ts exactly.
    Db.Execute(
        TEXT("CREATE TABLE IF NOT EXISTS bans (")
        TEXT("  id          INTEGER PRIMARY KEY AUTOINCREMENT,")
        TEXT("  uid         TEXT    NOT NULL UNIQUE,")
        TEXT("  playerUID   TEXT    NOT NULL,")
        TEXT("  platform    TEXT    NOT NULL,")
        TEXT("  playerName  TEXT    NOT NULL DEFAULT '',")
        TEXT("  reason      TEXT    NOT NULL DEFAULT 'No reason given',")
        TEXT("  bannedBy    TEXT    NOT NULL DEFAULT 'system',")
        TEXT("  banDate     TEXT    NOT NULL,")
        TEXT("  expireDate  TEXT,")
        TEXT("  isPermanent INTEGER NOT NULL DEFAULT 1")
        TEXT(");")
    );

    Db.Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_bans_uid      ON bans(uid);"));
    Db.Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_bans_platform ON bans(platform);"));
    Db.Execute(TEXT("CREATE INDEX IF NOT EXISTS idx_bans_expire   ON bans(expireDate);"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Write
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDatabase::AddBan(const FBanEntry& Entry)
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return false;

    static const TCHAR* Sql =
        TEXT("INSERT INTO bans")
        TEXT("  (uid, playerUID, platform, playerName, reason, bannedBy, banDate, expireDate, isPermanent)")
        TEXT(" VALUES")
        TEXT("  (?1,  ?2,       ?3,       ?4,         ?5,     ?6,      ?7,     ?8,         ?9)")
        TEXT(" ON CONFLICT(uid) DO UPDATE SET")
        TEXT("  playerUID   = excluded.playerUID,")
        TEXT("  playerName  = excluded.playerName,")
        TEXT("  reason      = excluded.reason,")
        TEXT("  bannedBy    = excluded.bannedBy,")
        TEXT("  banDate     = excluded.banDate,")
        TEXT("  expireDate  = excluded.expireDate,")
        TEXT("  isPermanent = excluded.isPermanent;");

    FSQLitePreparedStatement Stmt;
    if (!Stmt.Create(Db, Sql, ESQLitePreparedStatementFlags::Persistent))
    {
        UE_LOG(LogBanDatabase, Error, TEXT("AddBan: failed to prepare statement"));
        return false;
    }

    Stmt.SetBindingValueByIndex(1, Entry.Uid);
    Stmt.SetBindingValueByIndex(2, Entry.PlayerUID);
    Stmt.SetBindingValueByIndex(3, Entry.Platform);
    Stmt.SetBindingValueByIndex(4, Entry.PlayerName);
    Stmt.SetBindingValueByIndex(5, Entry.Reason);
    Stmt.SetBindingValueByIndex(6, Entry.BannedBy);
    Stmt.SetBindingValueByIndex(7, Entry.BanDate.ToIso8601());

    // NULL expireDate for permanent bans — matches the Node.js schema.
    if (Entry.bIsPermanent)
        Stmt.SetBindingValueByIndex(8, nullptr);
    else
        Stmt.SetBindingValueByIndex(8, Entry.ExpireDate.ToIso8601());

    Stmt.SetBindingValueByIndex(9, Entry.bIsPermanent ? (int64)1 : (int64)0);

    const ESQLitePreparedStatementStepResult Result = Stmt.Step();
    Stmt.Destroy();

    return Result == ESQLitePreparedStatementStepResult::Done;
}

bool UBanDatabase::RemoveBanByUid(const FString& Uid)
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return false;

    // Check existence first so we can return a meaningful bool.
    FBanEntry Dummy;
    if (!GetBanByUid_Locked(Uid, Dummy))
        return false;

    FSQLitePreparedStatement Stmt;
    if (!Stmt.Create(Db, TEXT("DELETE FROM bans WHERE uid = ?1;")))
        return false;
    Stmt.SetBindingValueByIndex(1, Uid);
    const ESQLitePreparedStatementStepResult Result = Stmt.Step();
    Stmt.Destroy();
    return Result == ESQLitePreparedStatementStepResult::Done;
}

bool UBanDatabase::RemoveBanById(int64 Id)
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return false;

    // Check existence first.
    FSQLitePreparedStatement CheckStmt;
    CheckStmt.Create(Db, TEXT("SELECT COUNT(*) FROM bans WHERE id = ?1;"));
    CheckStmt.SetBindingValueByIndex(1, Id);
    int64 Count = 0;
    if (CheckStmt.Step() == ESQLitePreparedStatementStepResult::Row)
        CheckStmt.GetColumnValueByIndex(0, Count);
    CheckStmt.Destroy();

    if (Count == 0) return false;

    FSQLitePreparedStatement DeleteStmt;
    if (!DeleteStmt.Create(Db, TEXT("DELETE FROM bans WHERE id = ?1;")))
        return false;
    DeleteStmt.SetBindingValueByIndex(1, Id);
    const ESQLitePreparedStatementStepResult Result = DeleteStmt.Step();
    DeleteStmt.Destroy();
    return Result == ESQLitePreparedStatementStepResult::Done;
}

int32 UBanDatabase::PruneExpiredBans()
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return 0;

    const FString Now = FDateTime::UtcNow().ToIso8601();

    // Count how many will be removed so we can return an accurate number.
    FSQLitePreparedStatement CountStmt;
    CountStmt.Create(Db,
        TEXT("SELECT COUNT(*) FROM bans")
        TEXT(" WHERE isPermanent = 0")
        TEXT("   AND expireDate IS NOT NULL")
        TEXT("   AND expireDate <= ?1;"));
    CountStmt.SetBindingValueByIndex(1, Now);

    int64 Count = 0;
    if (CountStmt.Step() == ESQLitePreparedStatementStepResult::Row)
        CountStmt.GetColumnValueByIndex(0, Count);
    CountStmt.Destroy();

    if (Count <= 0) return 0;

    FSQLitePreparedStatement DelStmt;
    DelStmt.Create(Db,
        TEXT("DELETE FROM bans")
        TEXT(" WHERE isPermanent = 0")
        TEXT("   AND expireDate IS NOT NULL")
        TEXT("   AND expireDate <= ?1;"));
    DelStmt.SetBindingValueByIndex(1, Now);
    DelStmt.Step();
    DelStmt.Destroy();

    return static_cast<int32>(Count);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Read
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDatabase::IsCurrentlyBanned(const FString& Uid, FBanEntry& OutEntry) const
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return false;

    const FString Now = FDateTime::UtcNow().ToIso8601();

    FSQLitePreparedStatement Stmt;
    Stmt.Create(Db,
        TEXT("SELECT * FROM bans")
        TEXT(" WHERE uid = ?1")
        TEXT("   AND (isPermanent = 1 OR (expireDate IS NOT NULL AND expireDate > ?2));"));
    Stmt.SetBindingValueByIndex(1, Uid);
    Stmt.SetBindingValueByIndex(2, Now);

    bool bFound = false;
    if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        bFound = RowToEntry(Stmt, OutEntry);
    }
    Stmt.Destroy();
    return bFound;
}

bool UBanDatabase::GetBanByUid(const FString& Uid, FBanEntry& OutEntry) const
{
    FScopeLock Lock(&DbMutex);
    return GetBanByUid_Locked(Uid, OutEntry);
}

bool UBanDatabase::GetBanByUid_Locked(const FString& Uid, FBanEntry& OutEntry) const
{
    // Caller must already hold DbMutex.
    if (!Db.IsValid()) return false;

    FSQLitePreparedStatement Stmt;
    Stmt.Create(Db, TEXT("SELECT * FROM bans WHERE uid = ?1;"));
    Stmt.SetBindingValueByIndex(1, Uid);

    bool bFound = false;
    if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        bFound = RowToEntry(Stmt, OutEntry);
    }
    Stmt.Destroy();
    return bFound;
}

TArray<FBanEntry> UBanDatabase::GetActiveBans() const
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return {};

    const FString Now = FDateTime::UtcNow().ToIso8601();

    FSQLitePreparedStatement Stmt;
    Stmt.Create(Db,
        TEXT("SELECT * FROM bans")
        TEXT(" WHERE isPermanent = 1")
        TEXT("    OR (expireDate IS NOT NULL AND expireDate > ?1)")
        TEXT(" ORDER BY banDate DESC;"));
    Stmt.SetBindingValueByIndex(1, Now);

    TArray<FBanEntry> Rows;
    while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        FBanEntry E;
        if (RowToEntry(Stmt, E))
            Rows.Add(E);
    }
    Stmt.Destroy();
    return Rows;
}

TArray<FBanEntry> UBanDatabase::GetAllBans() const
{
    FScopeLock Lock(&DbMutex);
    if (!Db.IsValid()) return {};

    FSQLitePreparedStatement Stmt;
    Stmt.Create(Db, TEXT("SELECT * FROM bans ORDER BY banDate DESC;"));

    TArray<FBanEntry> Rows;
    while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        FBanEntry E;
        if (RowToEntry(Stmt, E))
            Rows.Add(E);
    }
    Stmt.Destroy();
    return Rows;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Backup  (mirrors BanDatabase.backup() in database.ts)
// ─────────────────────────────────────────────────────────────────────────────

FString UBanDatabase::Backup(const FString& BackupDir, int32 MaxKeep) const
{
    FScopeLock Lock(&DbMutex);

    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

    if (!PF.FileExists(*DbPath))
        return FString();

    if (!PF.DirectoryExists(*BackupDir))
        PF.CreateDirectoryTree(*BackupDir);

    // Flush WAL to the main database file before copying.
    if (Db.IsValid())
        Db.Execute(TEXT("PRAGMA wal_checkpoint(FULL);"));

    const FDateTime Now = FDateTime::UtcNow();
    const FString   Stamp = FString::Printf(
        TEXT("%04d-%02d-%02d_%02d-%02d-%02d"),
        Now.GetYear(), Now.GetMonth(),  Now.GetDay(),
        Now.GetHour(), Now.GetMinute(), Now.GetSecond());
    const FString Dest = BackupDir / FString::Printf(TEXT("bans_%s.db"), *Stamp);

    if (!PF.CopyFile(*Dest, *DbPath))
    {
        UE_LOG(LogBanDatabase, Warning,
            TEXT("Backup: failed to copy %s → %s"), *DbPath, *Dest);
        return FString();
    }

    UE_LOG(LogBanDatabase, Log, TEXT("Backup: %s"), *Dest);

    // Prune old backups beyond MaxKeep.
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(BackupDir / TEXT("bans_*.db")), true, false);
    Files.Sort();
    while (Files.Num() > MaxKeep)
    {
        PF.DeleteFile(*(BackupDir / Files[0]));
        Files.RemoveAt(0);
    }

    return Dest;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

FString UBanDatabase::MakeUid(const FString& Platform, const FString& PlayerUID)
{
    return Platform.ToUpper() + TEXT(":") + PlayerUID;
}

void UBanDatabase::ParseUid(const FString& Uid, FString& OutPlatform, FString& OutPlayerUID)
{
    int32 ColonIdx = INDEX_NONE;
    if (Uid.FindChar(TEXT(':'), ColonIdx) && ColonIdx > 0)
    {
        OutPlatform  = Uid.Left(ColonIdx).ToUpper();
        OutPlayerUID = Uid.Mid(ColonIdx + 1);
    }
    else
    {
        OutPlatform  = TEXT("UNKNOWN");
        OutPlayerUID = Uid;
    }
}

FString UBanDatabase::GetDatabasePath() const
{
    // Read override from DefaultBanSystem.ini, fall back to default path.
    FString Configured;
    GConfig->GetString(TEXT("BanSystem"), TEXT("DatabasePath"), Configured, GGameIni);
    if (!Configured.IsEmpty())
        return Configured;

    return FPaths::ProjectSavedDir() / TEXT("BanSystem") / TEXT("bans.db");
}

bool UBanDatabase::RowToEntry(FSQLitePreparedStatement& Stmt, FBanEntry& OutEntry) const
{
    // Caller must hold DbMutex.
    int64 Id = 0;
    Stmt.GetColumnValueByIndex(BanCol::Id, Id);
    OutEntry.Id = Id;

    Stmt.GetColumnValueByIndex(BanCol::Uid,        OutEntry.Uid);
    Stmt.GetColumnValueByIndex(BanCol::PlayerUID,   OutEntry.PlayerUID);
    Stmt.GetColumnValueByIndex(BanCol::Platform,    OutEntry.Platform);
    Stmt.GetColumnValueByIndex(BanCol::PlayerName,  OutEntry.PlayerName);
    Stmt.GetColumnValueByIndex(BanCol::Reason,      OutEntry.Reason);
    Stmt.GetColumnValueByIndex(BanCol::BannedBy,    OutEntry.BannedBy);

    FString BanDateStr;
    Stmt.GetColumnValueByIndex(BanCol::BanDate, BanDateStr);
    FDateTime::ParseIso8601(*BanDateStr, OutEntry.BanDate);

    // expireDate may be NULL for permanent bans.
    FString ExpireDateStr;
    const bool bHasExpire = Stmt.GetColumnValueByIndex(BanCol::ExpireDate, ExpireDateStr);
    if (bHasExpire && !ExpireDateStr.IsEmpty())
    {
        FDateTime::ParseIso8601(*ExpireDateStr, OutEntry.ExpireDate);
        OutEntry.bIsPermanent = false;
    }
    else
    {
        OutEntry.ExpireDate   = FDateTime(0);
        OutEntry.bIsPermanent = true;
    }

    // Also read the stored isPermanent flag as an override.
    int64 IsPerm = 1;
    Stmt.GetColumnValueByIndex(BanCol::IsPermanent, IsPerm);
    OutEntry.bIsPermanent = (IsPerm != 0);

    return true;
}

TArray<FBanEntry> UBanDatabase::QueryRows_Locked(const TCHAR* Sql,
    const FString& Param1, const FString& Param2) const
{
    TArray<FBanEntry> Rows;
    if (!Db.IsValid()) return Rows;

    FSQLitePreparedStatement Stmt;
    Stmt.Create(Db, Sql);
    if (!Param1.IsEmpty()) Stmt.SetBindingValueByIndex(1, Param1);
    if (!Param2.IsEmpty()) Stmt.SetBindingValueByIndex(2, Param2);

    while (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
    {
        FBanEntry E;
        if (RowToEntry(Stmt, E))
            Rows.Add(E);
    }
    Stmt.Destroy();
    return Rows;
}
