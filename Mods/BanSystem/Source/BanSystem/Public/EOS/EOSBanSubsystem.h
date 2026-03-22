// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "EOSBanSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogEOSBanSystem, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Delegates — broadcast when bans change so other mods can react
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerBanned,   const FString&, EOSProductUserId, const FBanEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSPlayerUnbanned, const FString&, EOSProductUserId);

/**
 * UEOSBanSubsystem
 *
 * Standalone subsystem that manages EOS Product User ID bans independently
 * of any Steam or other platform ban system.  Persists ban records to
 *   <ProjectSaved>/BanSystem/EOSBans.json
 *
 * Other mods can retrieve this subsystem with:
 *   GetWorld()->GetGameInstance()->GetSubsystem<UEOSBanSubsystem>()
 *
 * NOTE: This subsystem is completely separate from USteamBanSubsystem.
 *       The two systems share no state, no logic, and no storage.
 */
UCLASS()
class BANSYSTEM_API UEOSBanSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem interface ──────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Ban Management API ────────────────────────────────────────────────

    /**
     * Bans a player by EOS Product User ID.
     *
     * @param EOSProductUserId  32-character lowercase hex EOS PUID
     *                          (e.g. "00020aed06f0a6958c3c067fb4b73d51").
     * @param Reason            Human-readable ban reason shown to the player.
     * @param DurationMinutes   0 = permanent; >0 = timed ban.
     * @param BannedBy          Name of admin issuing the ban.
     * @return true if the ban was added or updated successfully.
     */
    UFUNCTION(BlueprintCallable, Category = "Ban System|EOS")
    bool BanPlayer(const FString& EOSProductUserId, const FString& Reason,
                   int32 DurationMinutes = 0, const FString& BannedBy = TEXT("Server"));

    /**
     * Removes an EOS Product User ID ban.
     * @return true if an existing ban was removed.
     */
    UFUNCTION(BlueprintCallable, Category = "Ban System|EOS")
    bool UnbanPlayer(const FString& EOSProductUserId);

    /**
     * Full ban check; also removes expired bans in-place.
     * @param OutEntry  Populated with the ban record when Banned is returned.
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|EOS")
    EBanCheckResult CheckPlayerBan(const FString& EOSProductUserId, FBanEntry& OutEntry);

    /**
     * Convenience wrapper — returns true and fills OutReason if the player
     * is currently banned.
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|EOS")
    bool IsPlayerBanned(const FString& EOSProductUserId, FString& OutReason);

    /** Returns a snapshot of all current (non-expired) ban records. */
    UFUNCTION(BlueprintPure, Category = "Ban System|EOS")
    TArray<FBanEntry> GetAllBans() const;

    /** Total number of stored ban records (including timed, before expiry check). */
    UFUNCTION(BlueprintPure, Category = "Ban System|EOS")
    int32 GetBanCount() const;

    /** Removes all expired timed bans and saves. */
    UFUNCTION(BlueprintCallable, Category = "Ban System|EOS")
    void PruneExpiredBans();

    /** Force a reload of the ban list from disk. */
    UFUNCTION(BlueprintCallable, Category = "Ban System|EOS")
    void ReloadBans();

    // ── Validation Helpers (callable from other mods) ─────────────────────

    /**
     * Returns true if Id looks like a valid EOS Product User ID
     * (32 lowercase hex characters, no hyphens).
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|EOS")
    static bool IsValidEOSProductUserId(const FString& Id);

    // ── Delegates ─────────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category = "Ban System|EOS")
    FOnEOSPlayerBanned OnPlayerBanned;

    UPROPERTY(BlueprintAssignable, Category = "Ban System|EOS")
    FOnEOSPlayerUnbanned OnPlayerUnbanned;

private:
    void LoadBans();
    void SaveBans() const;
    FString GetBanFilePath() const;

    /** In-memory ban map: EOSProductUserId → FBanEntry */
    TMap<FString, FBanEntry> BanMap;
};
