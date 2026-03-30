// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameFramework/OnlineReplStructs.h"
#include "EOSTypes.h"
#include "BanEnforcementSubsystem.generated.h"

class AGameModeBase;
class APlayerController;

/**
 * UBanEnforcementSubsystem
 *
 * Central hub for all ban enforcement and async cross-platform EOS PUID resolution.
 * Replaces the inline lambda enforcement that was previously embedded in FBanSystemModule.
 *
 * ENFORCEMENT FLOW
 * ────────────────
 * 1. PreLogin  (FGameModeEvents::GameModePreLoginEvent)
 *    Rejects connections from players already on the Steam or EOS ban list.
 *    The connection is refused before any PlayerController is created, so the
 *    banned player never enters the game world.
 *
 * 2. PostLogin  (FGameModeEvents::GameModePostLoginEvent)
 *    Fallback enforcement after the PlayerState is available.  Runs immediately
 *    if the player's EOS PUID was already resolved, or triggers an async
 *    Steam64→PUID lookup via UEOSConnectSubsystem when the PUID is not yet
 *    available (common on first-time Steam+EOS logins).
 *
 * 3. PUID Registered  (UEOSConnectSubsystem::OnPlayerPUIDRegistered)
 *    Fires when EOSSystem successfully tracks a new player PUID (may arrive
 *    milliseconds after PostLogin).  Immediately checks the EOS ban list and
 *    kicks any banned player.
 *
 * 4. Async PUID Lookup  (UEOSConnectSubsystem::OnPUIDLookupComplete)
 *    Triggered by step 2 when a Steam player has no immediate PUID.  When the
 *    EOS result arrives, the EOS ban list is checked and the player is kicked
 *    if banned.  Also handles cross-platform EOS ban propagation (see below).
 *
 * 5. Async Reverse Lookup  (UEOSConnectSubsystem::OnReverseLookupComplete)
 *    Triggered when PropagateToSteamAsync() issues a PUID→Steam64 query.
 *    Applies the complementary Steam ban once the Steam64 ID is resolved.
 *
 * CROSS-PLATFORM BAN PROPAGATION
 * ──────────────────────────────
 * Call PropagateToEOSAsync() after issuing a Steam ban.
 * Call PropagateToSteamAsync() after issuing an EOS ban.
 *
 * Both methods first check the EOSSystem sync cache (zero latency if cached).
 * If the ID is not cached, an async EOS query is started and the ban is applied
 * automatically when the result arrives.  Both are no-ops if EOSSystem is not
 * installed or not ready.
 */
UCLASS()
class BANSYSTEM_API UBanEnforcementSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // ── Cross-platform propagation API ────────────────────────────────────────
    //
    // Call these from BanCommands or BanDiscordSubsystem immediately after
    // issuing a platform-specific ban.  Each method applies the complementary
    // ban on the other platform: EOS for Steam bans, Steam for EOS bans.

    /**
     * After banning by Steam64 ID, asynchronously look up the linked EOS PUID
     * and also apply an EOS ban with the same reason and duration.
     * Checks the EOSSystem sync cache first; falls back to an async EOS query.
     * No-op when EOSSystem is unavailable or no PUID can be resolved.
     */
    void PropagateToEOSAsync(const FString& Steam64Id,
                             const FString& Reason,
                             int32          DurationMinutes,
                             const FString& BannedBy);

    /**
     * After banning by EOS PUID, asynchronously look up the linked Steam64 ID
     * and also apply a Steam ban with the same reason and duration.
     * Checks the EOSSystem sync cache first; falls back to an async EOS query.
     * No-op when EOSSystem is unavailable or no Steam64 ID can be resolved.
     */
    void PropagateToSteamAsync(const FString& PUID,
                               const FString& Reason,
                               int32          DurationMinutes,
                               const FString& BannedBy);

private:
    // ── GameMode event handlers ───────────────────────────────────────────────

    void OnPreLogin(AGameModeBase* GameMode,
                    const FUniqueNetIdRepl& UniqueId,
                    FString& ErrorMessage);

    void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

    /** Called when a player controller is destroyed (disconnect or level change). */
    UFUNCTION()
    void OnLogout(AGameModeBase* GameMode, AController* Controller);

    // ── EOSSystem dynamic delegate callbacks ──────────────────────────────────
    // Marked UFUNCTION so AddDynamic/RemoveDynamic work correctly.

    /** Called by EOSSystem when a new PUID is tracked (on or just after PostLogin).
     *  Controller is the PlayerController associated with this login, or nullptr
     *  when triggered by a manual RegisterPlayerPUID() call. */
    UFUNCTION()
    void OnPUIDRegistered(const FString& PUID, APlayerController* Controller);

    /**
     * Called when an async Steam64→PUID lookup completes.
     * Handles both join-time enforcement and cross-platform ban propagation.
     */
    UFUNCTION()
    void OnPUIDLookupDone(const FString& ExternalAccountId, const FString& PUID);

    // ── Helpers ───────────────────────────────────────────────────────────────

    /** Kick a player who is on the ban list after they have already joined. */
    void KickBannedPlayer(APlayerController* Controller,
                          const FString&     PlayerId,
                          const FString&     KickMessage);

    /** Build the disconnect-reason message shown to a banned player. */
    FString BuildKickMessage(bool bIsEOSBan, const FString& Reason) const;

    /** Optionally post a ban-kick notification to Discord if configured. */
    void PostBanKickDiscordNotification(const FString& PlayerId,
                                        const FString& Reason) const;

    // ── Pending join-time ban checks ──────────────────────────────────────────
    // When a Steam player joins without an immediately-resolved EOS PUID, we
    // add them here and kick them as soon as the PUID lookup result arrives.

    struct FPendingEOSCheck
    {
        TWeakObjectPtr<APlayerController> Controller;
        FString Steam64Id;
    };

    /** Keyed by Steam64Id — matched against OnPUIDLookupDone results. */
    TMap<FString, FPendingEOSCheck> PendingBySteam64;

    // ── Pending cross-platform ban propagation ────────────────────────────────

    struct FPropagationEntry
    {
        FString Reason;
        int32   DurationMinutes = 0;
        FString BannedBy;
    };

    /** Forward propagation: Steam64Id → pending EOS ban.  Set by PropagateToEOSAsync. */
    TMap<FString, FPropagationEntry> PendingEOSPropagation;

    /** Reverse propagation: PUID → pending Steam ban.  Set by PropagateToSteamAsync. */
    TMap<FString, FPropagationEntry> PendingSteamPropagation;

    // ── Delegate handles ──────────────────────────────────────────────────────

    FDelegateHandle PreLoginHandle;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;
};
