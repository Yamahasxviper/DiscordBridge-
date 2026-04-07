// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "BanAppealRegistry.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanAppealRegistry, Log, All);

/**
 * UBanAppealRegistry
 *
 * Subsystem that stores and persists ban appeals submitted via the REST API.
 * Appeals are stored in appeals.json (same directory as bans.json).
 *
 * Thread-safe: all public methods acquire the internal Mutex before
 * accessing the Appeals array or performing file I/O.
 */
UCLASS()
class BANSYSTEM_API UBanAppealRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Add a new ban appeal.
     * Returns the newly created entry (with auto-assigned Id).
     * Saves to disk immediately.
     * Thread-safe.
     */
    FBanAppealEntry AddAppeal(const FString& Uid, const FString& Reason,
                              const FString& ContactInfo);

    /**
     * Returns all stored appeals.
     * Thread-safe.
     */
    TArray<FBanAppealEntry> GetAllAppeals() const;

    /**
     * Delete the appeal with the given Id.
     * Returns true if an appeal was found and removed.
     * Thread-safe.
     */
    bool DeleteAppeal(int64 Id);

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FBanAppealEntry> Appeals;
    mutable FCriticalSection Mutex;
    FString FilePath;
};
