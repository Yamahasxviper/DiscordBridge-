// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "GameFramework/OnlineReplStructs.h"
#include "EOSTypes.h"
#include "BanTypes.h"
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

    /**
     * After unbanning by Steam64 ID, asynchronously look up the linked EOS PUID
     * and also remove any EOS ban for that player.
     * Checks the EOSSystem sync cache first; falls back to an async EOS query.
     * No-op when EOSSystem is unavailable or no PUID can be resolved.
     */
    void PropagateUnbanToEOSAsync(const FString& Steam64Id);

    /**
     * After unbanning by EOS PUID, asynchronously look up the linked Steam64 ID
     * and also remove any Steam ban for that player.
     * Checks the EOSSystem sync cache first; falls back to an async reverse lookup.
     * No-op when EOSSystem is unavailable or no Steam64 ID can be resolved.
     */
    void PropagateUnbanToSteamAsync(const FString& PUID);

private:
    // ── GameMode event handlers ───────────────────────────────────────────────

    void OnPreLogin(AGameModeBase* GameMode,
                    const FUniqueNetIdRepl& UniqueId,
                    FString& ErrorMessage);

    void OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer);

    /** Called when a player controller is destroyed (disconnect or level change). */
    UFUNCTION()
    void OnLogout(AGameModeBase* GameMode, AController* Controller);

    // ── Pre-join hook (Options string path) ──────────────────────────────────
    //
    // The CSS custom UE5.3.2 engine may not populate FUniqueNetIdRepl at
    // PreLogin time with Steam or EOS identities, because the engine's online
    // identity layer is not yet ready at that early stage.  To ensure banned
    // players are still rejected before they enter the game world, we also hook
    // AGameModeBase::PreLogin() directly — which exposes the raw connection
    // Options string — and parse platform IDs from it as a fallback.
    //
    // The AFTER hook fires after ApproveLogin and the FGameModePreLoginEvent,
    // so it sees the final ErrorMessage state and adds the Options-based check
    // on top of the standard UniqueId-based check as a complementary layer.

    /**
     * Hook handler for AGameModeBase::PreLogin() (AFTER hook).
     *
     * Runs AFTER the original PreLogin body has completed (i.e., after
     * ApproveLogin and the FGameModePreLoginEvent broadcast).
     *
     * Provides a complementary ban check for the CSS custom engine:
     *  1. Returns early when ErrorMessage is already set (already rejected).
     *  2. Attempts FBanIdResolver::Resolve(UniqueId) — standard UE5 path.
     *  3. Falls back to FBanIdResolver::ParseIdsFromOptions(Options) when
     *     FUniqueNetIdRepl is not populated by the CSS custom engine.
     *  4. Sets ErrorMessage to reject the connection when the player is banned.
     */
    void OnPreLoginHook(AGameModeBase*         GameMode,
                        const FString&         Options,
                        const FString&         Address,
                        const FUniqueNetIdRepl& UniqueId,
                        FString&               ErrorMessage);

    // ── EOSSystem dynamic delegate callbacks ──────────────────────────────────
    // Marked UFUNCTION so AddDynamic/RemoveDynamic work correctly.

    /** Called by EOSSystem when a new PUID is tracked (on or just after PostLogin).
     *  Controller is the PlayerController associated with this login, or nullptr
     *  when triggered by a manual RegisterPlayerPUID() call. */
    UFUNCTION()
    void OnPUIDRegistered(const FString& PUID, APlayerController* Controller);

    // ── Ban-issued callbacks — kick currently-connected banned players ─────────

    /** Called when a Steam ban is issued; kicks the player if they are online. */
    UFUNCTION()
    void OnSteamPlayerBanned(const FString& Steam64Id, const FBanEntry& Entry);

    /** Called when an EOS ban is issued; kicks the player if they are online. */
    UFUNCTION()
    void OnEOSPlayerBanned(const FString& EOSProductUserId, const FBanEntry& Entry);

    /**
     * Called when an async Steam64→PUID lookup completes.
     * Handles both join-time enforcement and cross-platform ban propagation.
     */
    UFUNCTION()
    void OnPUIDLookupDone(const FString& ExternalAccountId, const FString& PUID);

    /**
     * Called when an async PUID→external account reverse lookup completes.
     * Handles cross-platform Steam ban propagation (PropagateToSteamAsync path).
     */
    UFUNCTION()
    void OnReverseLookupDone(const FString& PUID,
                             const FString& ExternalAccountId,
                             EEOSExternalAccountType AccountType);

    /**
     * Called after ALL external accounts for a queried PUID have been enumerated
     * (including when the PUID has no linked external accounts at all).
     * Cleans up any pending Steam ban/unban propagation entries for this PUID
     * that were not already resolved by OnReverseLookupDone (i.e. the PUID has
     * no Steam account linked and no Steam ban/unban can be applied).
     */
    UFUNCTION()
    void OnReverseLookupAllDone(const FString& PUID);

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

    /** Forward unban propagation: Steam64Id set — unban EOS player when PUID lookup completes. */
    TSet<FString> PendingEOSUnban;

    /** Reverse unban propagation: PUID set — unban Steam player when reverse lookup completes. */
    TSet<FString> PendingSteamUnban;

    // ── Delegate handles ──────────────────────────────────────────────────────

    FDelegateHandle PreLoginHandle;
    FDelegateHandle PostLoginHandle;
    FDelegateHandle LogoutHandle;

    /**
     * Handle returned by SUBSCRIBE_METHOD_VIRTUAL on AGameModeBase::PreLogin.
     * Installed in Initialize(); removed in Deinitialize() via UNSUBSCRIBE_METHOD.
     * Provides access to the raw connection Options string so that player IDs
     * can be parsed as a fallback when FUniqueNetIdRepl is not populated by the
     * CSS custom engine at early PreLogin time.
     */
    FDelegateHandle PreLoginHookHandle;

    /** Periodic ticker that prunes expired bans from both subsystems. */
    FTSTicker::FDelegateHandle ExpiryPruneTickerHandle;
};
