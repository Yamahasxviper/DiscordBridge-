// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "IBanNotificationProvider.h"
#include "SteamBanSubsystem.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogSteamBanSystem, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Delegates — broadcast when bans change so other mods can react
// ─────────────────────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnSteamPlayerBanned,   const FString&, Steam64Id, const FBanEntry&, Entry);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnSteamPlayerUnbanned, const FString&, Steam64Id);

/**
 * USteamBanSubsystem
 *
 * Standalone subsystem that manages Steam64 ID bans independently of any
 * EOS or other platform ban system.  Persists ban records to
 *   <ProjectSaved>/BanSystem/SteamBans.json
 *
 * Other mods can retrieve this subsystem with:
 *   GetWorld()->GetGameInstance()->GetSubsystem<USteamBanSubsystem>()
 */
UCLASS()
class BANSYSTEM_API USteamBanSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem interface ──────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Ban Management API ────────────────────────────────────────────────

    /**
     * Bans a player by Steam64 ID.
     *
     * @param Steam64Id   17-digit Steam64 ID string (e.g. "76561198000000000").
     * @param Reason      Human-readable ban reason shown to the player.
     * @param DurationMinutes  0 = permanent; >0 = timed ban.
     * @param BannedBy    Name of admin issuing the ban.
     * @return true if the ban was added or updated successfully.
     */
    UFUNCTION(BlueprintCallable, Category = "Ban System|Steam")
    bool BanPlayer(const FString& Steam64Id, const FString& Reason,
                   int32 DurationMinutes = 0, const FString& BannedBy = TEXT("Server"));

    /**
     * Removes a Steam64 ID ban.
     * @return true if an existing ban was removed.
     */
    UFUNCTION(BlueprintCallable, Category = "Ban System|Steam")
    bool UnbanPlayer(const FString& Steam64Id);

    /**
     * Full ban check; also removes expired bans in-place.
     * @param OutEntry  Populated with the ban record when Banned is returned.
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|Steam")
    EBanCheckResult CheckPlayerBan(const FString& Steam64Id, FBanEntry& OutEntry);

    /**
     * Convenience wrapper — returns true and fills OutReason if the player
     * is currently banned.
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|Steam")
    bool IsPlayerBanned(const FString& Steam64Id, FString& OutReason);

    /** Returns a snapshot of all current (non-expired) ban records. */
    UFUNCTION(BlueprintPure, Category = "Ban System|Steam")
    TArray<FBanEntry> GetAllBans() const;

    /** Total number of stored ban records (including timed, before expiry check). */
    UFUNCTION(BlueprintPure, Category = "Ban System|Steam")
    int32 GetBanCount() const;

    /** Removes all expired timed bans and saves. */
    UFUNCTION(BlueprintCallable, Category = "Ban System|Steam")
    void PruneExpiredBans();

    /** Force a reload of the ban list from disk. */
    UFUNCTION(BlueprintCallable, Category = "Ban System|Steam")
    void ReloadBans();

    // ── Notification Provider ─────────────────────────────────────────────────

    /**
     * Register a notification provider to be called whenever a Steam ban or
     * unban is issued.  Pass nullptr to detach the current provider.
     *
     * This is the preferred integration point for external mods (e.g.
     * DiscordBridge): the provider implements IBanNotificationProvider and
     * registers itself here.  BanSystem has no compile-time knowledge of the
     * provider; the coupling is entirely one-directional.
     *
     * Only one provider can be active at a time.  Registering a new provider
     * replaces the previous one without notification.
     */
    void SetNotificationProvider(IBanNotificationProvider* Provider);

    // ── Validation Helpers (callable from other mods) ─────────────────────

    /**
     * Returns true if Id looks like a valid Steam64 ID
     * (17 digits, starts with "7656119").
     */
    UFUNCTION(BlueprintPure, Category = "Ban System|Steam")
    static bool IsValidSteam64Id(const FString& Id);

    // ── Delegates ─────────────────────────────────────────────────────────
    UPROPERTY(BlueprintAssignable, Category = "Ban System|Steam")
    FOnSteamPlayerBanned OnPlayerBanned;

    UPROPERTY(BlueprintAssignable, Category = "Ban System|Steam")
    FOnSteamPlayerUnbanned OnPlayerUnbanned;

private:
    void LoadBans();
    void SaveBans() const;
    /** Creates a timestamped backup of the ban file in the Backups/ sub-directory
     *  and prunes all but the most recent MaxBackups files. */
    void BackupBans() const;
    FString GetBanFilePath() const;

    /** In-memory ban map: Steam64Id → FBanEntry */
    TMap<FString, FBanEntry> BanMap;

    /** Optional notification provider — called after every ban/unban.
     *  Raw pointer; the owner is responsible for calling
     *  SetNotificationProvider(nullptr) before destroying the provider. */
    IBanNotificationProvider* NotificationProvider = nullptr;
};
