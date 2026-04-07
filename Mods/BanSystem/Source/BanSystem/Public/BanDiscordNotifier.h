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

    /** Called when a player is kicked (without a ban). */
    static void NotifyPlayerKicked(const FString& PlayerName, const FString& Reason,
                                   const FString& KickedBy);

private:
    /**
     * Posts JsonPayload to the configured DiscordWebhookUrl via HTTP POST.
     * Does nothing if DiscordWebhookUrl is empty or IHttpModule is unavailable.
     */
    static void PostWebhook(const FString& JsonPayload);
};
