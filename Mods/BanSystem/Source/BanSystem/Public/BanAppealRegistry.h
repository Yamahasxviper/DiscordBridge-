// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
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
    // ── USubsystem ───────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Add a new ban appeal.
     * Returns the newly created entry (with auto-assigned Id).
     * Saves to disk immediately.
     * Thread-safe.
     */
    /**
     * @param BanUid  Optional — the actual ban UID (e.g. "EOS:<puid>") to use
     *                when auto-unbanning on approval.  Should be set when the
     *                submission Uid differs from the ban-database key (e.g.
     *                Discord-originated appeals where Uid = "Discord:<id>").
     */
    FBanAppealEntry AddAppeal(const FString& Uid, const FString& Reason,
                              const FString& ContactInfo,
                              const FString& BanUid = FString());

    /**
     * Returns all stored appeals.
     * Thread-safe.
     */
    TArray<FBanAppealEntry> GetAllAppeals() const;

    /**
     * Returns the single appeal with the given Id, or an empty entry (Id==0) if not found.
     * Thread-safe.
     */
    FBanAppealEntry GetAppealById(int64 Id) const;

    /**
     * Delete the appeal with the given Id.
     * Returns true if an appeal was found and removed.
     * Thread-safe.
     */
    bool DeleteAppeal(int64 Id);

    /**
     * Mark an appeal as Approved or Denied.
     * Updates Status, ReviewedBy, ReviewNote, and ReviewedAt then saves to disk.
     * Returns true when the appeal was found and updated; false when not found.
     * Thread-safe.
     */
    bool ReviewAppeal(int64 Id, EAppealStatus NewStatus,
                      const FString& ReviewedByName,
                      const FString& ReviewNote);

    // ── Delegates ────────────────────────────────────────────────────────────

    /**
     * Fired after every AddAppeal() call.
     * External systems (e.g. DiscordBridge) bind here to post notifications.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBanAppealSubmitted, const FBanAppealEntry&);
    static FOnBanAppealSubmitted OnBanAppealSubmitted;

    /**
     * Fired after a successful ReviewAppeal() call.
     * External systems (e.g. DiscordBridge) bind here to post review notifications.
     */
    DECLARE_MULTICAST_DELEGATE_OneParam(FOnBanAppealReviewed, const FBanAppealEntry&);
    static FOnBanAppealReviewed OnBanAppealReviewed;

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FBanAppealEntry> Appeals;
    int64                   NextId = 1;
    mutable FCriticalSection Mutex;
    FString FilePath;
};
