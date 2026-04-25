// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "PlayerWarningRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogPlayerWarningRegistry, Log, All);

/**
 * UPlayerWarningRegistry
 *
 * Subsystem that tracks per-player warnings and persists them to
 * warnings.json (same directory as bans.json).
 *
 * Thread-safe: all public methods acquire the internal Mutex before
 * accessing the Warnings array or performing file I/O.
 */
UCLASS()
class BANSYSTEM_API UPlayerWarningRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Add a new warning for the given player UID.
     * Saves the updated list to disk immediately.
     * Thread-safe.
     */
    void AddWarning(const FString& Uid, const FString& PlayerName,
                    const FString& Reason, const FString& WarnedBy);

    /**
     * Returns all warnings recorded for the given UID (case-insensitive).
     * Thread-safe.
     */
    TArray<FWarningEntry> GetWarningsForUid(const FString& Uid) const;

    /**
     * Removes all warnings for the given UID (case-insensitive).
     * Saves the updated list to disk immediately.
     * Returns the number of warnings that were removed.
     * Thread-safe.
     */
    int32 ClearWarningsForUid(const FString& Uid);

    /**
     * Returns every warning entry in the registry.
     * Thread-safe.
     */
    TArray<FWarningEntry> GetAllWarnings() const;

    /**
     * Returns the number of warnings for the given UID (case-insensitive).
     * Thread-safe.
     */
    int32 GetWarningCount(const FString& Uid) const;

    /**
     * Returns the total accumulated warning points for the given UID.
     * Points are the sum of FWarningEntry::Points for all non-expired warnings
     * within the decay window (respects WarnDecayDays like GetWarningCount).
     * Thread-safe.
     */
    int32 GetWarningPoints(const FString& Uid) const;

    /**
     * Add a new warning from a pre-populated FWarningEntry struct.
     * The entry's Id and WarnDate are overwritten with auto-generated values.
     * Saves to disk immediately. Thread-safe.
     */
    void AddWarning(const FWarningEntry& Entry);

    /**
     * Add a new warning with an optional expiry time (Points defaults to 1).
     * ExpiryMinutes == 0 means no expiry (the warning never expires).
     * Saves to disk immediately. Thread-safe.
     */
    void AddWarning(const FString& Uid, const FString& PlayerName,
                    const FString& Reason, const FString& WarnedBy,
                    int32 ExpiryMinutes);

    /**
     * Add a new warning with an optional expiry time and point value.
     * ExpiryMinutes == 0 means no expiry (the warning never expires).
     * Points defaults to 1 (minor warning).
     * Saves to disk immediately. Thread-safe.
     */
    void AddWarning(const FString& Uid, const FString& PlayerName,
                    const FString& Reason, const FString& WarnedBy,
                    int32 ExpiryMinutes, int32 Points);

    /**
     * Removes the single warning with the given integer ID.
     * Saves immediately. Returns true if a warning was found and removed.
     * Thread-safe.
     */
    bool DeleteWarningById(int64 Id);

    /**
     * Removes all warnings whose expiry time has passed.
     * Returns the number of warnings that were removed.
     * Thread-safe.
     */
    int32 PruneExpiredWarnings();

    // ── Delegates ─────────────────────────────────────────────────────────────

    /**
     * Fired on the game thread after every successful AddWarning() call.
     * Allows external systems (e.g. BanDiscordSubsystem) to route warnings
     * issued from in-game commands or the chat filter into per-player threads
     * without polling or direct coupling.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnWarningAdded, const FWarningEntry& /*Entry*/);
    static FOnWarningAdded OnWarningAdded;

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FWarningEntry> Warnings;
    int64                 NextId = 1;
    mutable FCriticalSection Mutex;
    FString FilePath;
};
