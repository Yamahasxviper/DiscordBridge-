// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BanTypes.h"

/**
 * FBanDiscordNotifier
 *
 * Stateless helper that posts Discord embed notifications to the webhook URL
 * configured in UBanSystemConfig::DiscordWebhookUrl.
 *
 * All methods are no-ops when DiscordWebhookUrl is empty.
 */
class BANSYSTEM_API FBanDiscordNotifier
{
public:
    /** Called when a ban is created (by REST API or chat command). */
    static void NotifyBanCreated(const FBanEntry& Entry);

    /** Called when a ban is removed. */
    static void NotifyBanRemoved(const FString& Uid, const FString& PlayerName,
                                 const FString& RemovedBy);

    /** Called when a player is warned. */
    static void NotifyWarningIssued(const FString& Uid, const FString& PlayerName,
                                    const FString& Reason, const FString& WarnedBy,
                                    int32 TotalWarnings);

    /**
     * Called when a player is kicked (without a ban).
     * @param Uid  Optional compound UID ("EOS:xxx").  When non-empty the
     *             OnPlayerKickedLogged delegate is broadcast so that subsystems
     *             such as BanDiscordSubsystem can route the event into a
     *             per-player moderation thread.  Existing callers that omit
     *             the Uid (e.g. Discord slash-kick handlers) are unaffected.
     */
    static void NotifyPlayerKicked(const FString& PlayerName, const FString& Reason,
                                   const FString& KickedBy,
                                   const FString& Uid = FString());

    /** Called when a temporary ban expires and the player is allowed to reconnect. */
    static void NotifyBanExpired(const FBanEntry& Entry);

    /** Called when a player submits a ban appeal via the REST API. */
    static void NotifyAppealSubmitted(const FBanAppealEntry& Appeal);

    /** Called when an appeal is approved or denied by an admin. */
    static void NotifyAppealReviewed(const FBanAppealEntry& Appeal);

    /**
     * Called when a warn-based auto-escalation triggers an automatic ban.
     * Posts an embed to Discord so staff can review and override.
     * Only has an effect when DiscordWebhookUrl is set.
     */
    static void NotifyAutoEscalationBan(const FBanEntry& Ban, int32 WarnCount);

    /** Called when a geo-IP check blocks a player from connecting. */
    static void NotifyGeoIpBlocked(const FString& PlayerName, const FString& Uid,
                                    const FString& IpAddress, const FString& CountryCode);

    /** Called when a player is muted via the in-game /mute command. */
    static void NotifyPlayerMuted(const FString& Uid, const FString& PlayerName,
                                  const FString& MutedBy, const FString& Reason,
                                  bool bIsTimed, const FDateTime& ExpireDate);

    /** Called when a player is unmuted via the in-game /unmute command. */
    static void NotifyPlayerUnmuted(const FString& Uid, const FString& PlayerName,
                                    const FString& UnmutedBy);

    // ── Delegates ─────────────────────────────────────────────────────────────

    /**
     * Fired (on the game thread) by NotifyPlayerKicked() when a non-empty Uid
     * is supplied.  Allows BanDiscordSubsystem to route in-game /kick events
     * into per-player moderation threads without direct coupling.
     * Discord slash-kick handlers intentionally omit the Uid, so this delegate
     * is only fired for in-game commands — preventing duplicate thread posts.
     */
    DECLARE_MULTICAST_DELEGATE_FourParams(FOnPlayerKickedLogged,
        const FString& /*Uid*/, const FString& /*PlayerName*/,
        const FString& /*Reason*/, const FString& /*KickedBy*/);
    static FOnPlayerKickedLogged OnPlayerKickedLogged;

private:
    /**
     * Posts JsonPayload to the configured DiscordWebhookUrl via HTTP POST.
     * Does nothing if DiscordWebhookUrl is empty or IHttpModule is unavailable.
     */
    static void PostWebhook(const FString& JsonPayload);
};
