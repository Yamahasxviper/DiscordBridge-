// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
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
     * Add a new warning with an optional expiry time.
     * ExpiryMinutes == 0 means no expiry (the warning never expires).
     * Saves to disk immediately. Thread-safe.
     */
    void AddWarning(const FString& Uid, const FString& PlayerName,
                    const FString& Reason, const FString& WarnedBy,
                    int32 ExpiryMinutes);

    /**
     * Removes the single warning with the given integer ID.
     * Saves immediately. Returns true if a warning was found and removed.
     * Thread-safe.
     */
    bool DeleteWarningById(int64 Id);

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FWarningEntry> Warnings;
    mutable FCriticalSection Mutex;
    FString FilePath;
};
