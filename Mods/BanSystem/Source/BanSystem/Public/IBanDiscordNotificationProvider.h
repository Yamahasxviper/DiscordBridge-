// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IBanDiscordNotificationProvider
 *
 * Lightweight interface for sending ban/unban notification messages to a
 * Discord channel.  Implemented by DiscordBridge (or any other notification
 * backend you want to use in a different project).
 *
 * BanSystem is fully standalone and never links against DiscordBridge.
 * The integration works as follows:
 *
 *   1. DiscordBridge implements this interface (one method).
 *   2. DiscordBridge calls:
 *        USteamBanSubsystem::SetNotificationProvider(this);
 *        UEOSBanSubsystem::SetNotificationProvider(this);
 *      in its own Initialize(), and passes nullptr in Deinitialize().
 *   3. BanSystem calls SendBanDiscordMessage() when a ban/unban occurs,
 *      using the channel ID and message templates from DefaultBanSystem.ini.
 *
 * If no provider is registered the ban system operates identically —
 * Discord notifications are simply skipped.
 */
class BANSYSTEM_API IBanDiscordNotificationProvider
{
public:
    virtual ~IBanDiscordNotificationProvider() = default;

    /**
     * Send a plain-text message to a Discord channel.
     *
     * @param ChannelId  Discord channel snowflake ID string.
     * @param Message    Formatted notification body.
     */
    virtual void SendBanDiscordMessage(const FString& ChannelId, const FString& Message) = 0;
};
