// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "BanAuditLog.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanAuditLog, Log, All);

/**
 * UBanAuditLog
 *
 * Subsystem that records admin actions (bans, unbans, kicks, warns, etc.)
 * and persists them to audit_log.json (same directory as bans.json).
 *
 * Entries are trimmed to a maximum of 10 000 records; oldest entries are
 * discarded when the cap is exceeded.
 *
 * Thread-safe: all public methods acquire the internal Mutex before
 * accessing the Entries array or performing file I/O.
 */
UCLASS()
class BANSYSTEM_API UBanAuditLog : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Append a new audit entry for the given action.
     * Assigns a new auto-incremented Id, records FDateTime::UtcNow() as the
     * timestamp, and saves to disk immediately.
     * Thread-safe.
     */
    void LogAction(const FString& Action,
                   const FString& TargetUid,  const FString& TargetName,
                   const FString& AdminUid,   const FString& AdminName,
                   const FString& Details = TEXT(""));

    /**
     * Returns up to Limit of the most recent entries, newest first.
     * Thread-safe.
     */
    TArray<FAuditEntry> GetRecentEntries(int32 Limit = 100) const;

    /**
     * Returns all entries where TargetUid matches the given value
     * (case-insensitive), newest first.
     * Thread-safe.
     */
    TArray<FAuditEntry> GetEntriesForTarget(const FString& TargetUid) const;

    /**
     * Returns all stored audit entries (oldest first, as stored on disk).
     * Thread-safe.
     */
    TArray<FAuditEntry> GetAllEntries() const;

private:
    static constexpr int32 MaxEntries = 10000;

    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FAuditEntry>  Entries;
    mutable FCriticalSection Mutex;
    FString FilePath;

    /** Auto-incremented ID for the next entry; initialised from the loaded
     *  entries in LoadFromFile() to avoid an O(N) scan on every LogAction(). */
    int64 NextAuditId = 1;

    /**
     * BanSystem mod version string cached at Initialize() time via SML's
     * UModLoadingLibrary.  Written into every FAuditEntry.ModVersion so that
     * audit consumers can see which build of BanSystem recorded each action.
     * Empty when the mod info is not available (e.g. standalone unit tests).
     */
    FString CachedModVersion;
};
