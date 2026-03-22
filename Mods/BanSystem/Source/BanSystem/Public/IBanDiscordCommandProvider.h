// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

/**
 * IBanDiscordCommandProvider
 *
 * Abstract interface that BanSystem uses to receive Discord messages and send
 * Discord responses.  Implement this interface in a mod that provides Discord
 * connectivity (e.g. DiscordBridge) and register it with UBanDiscordSubsystem
 * via SetCommandProvider().
 *
 * BanSystem has zero compile-time knowledge of any provider — the coupling is
 * one-directional: the implementing mod depends on BanSystem; BanSystem never
 * depends on the implementing mod.
 *
 * Usage (implementing mod side):
 *   1. Inherit IBanDiscordCommandProvider in your subsystem or module class.
 *   2. Implement all pure-virtual methods below.
 *   3. In your Initialize(), obtain UBanDiscordSubsystem and call:
 *        BanDiscord->SetCommandProvider(this);
 *   4. In your Deinitialize(), call:
 *        BanDiscord->SetCommandProvider(nullptr);
 */
class IBanDiscordCommandProvider
{
public:
	virtual ~IBanDiscordCommandProvider() = default;

	/**
	 * Subscribe to Discord MESSAGE_CREATE events.
	 * The callback is invoked on the game thread with the full message JSON object.
	 *
	 * @param Callback  Lambda that receives the MESSAGE_CREATE data object.
	 * @return A handle that must be passed to UnsubscribeDiscordMessages() to cancel.
	 */
	virtual FDelegateHandle SubscribeDiscordMessages(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) = 0;

	/**
	 * Cancel a previously registered message subscription.
	 *
	 * @param Handle  The handle returned by SubscribeDiscordMessages().
	 */
	virtual void UnsubscribeDiscordMessages(FDelegateHandle Handle) = 0;

	/**
	 * Post a plain-text message to a Discord channel via the REST API.
	 *
	 * @param ChannelId  Snowflake ID of the target channel.
	 * @param Message    Plain-text content (Discord markdown supported).
	 */
	virtual void SendDiscordChannelMessage(const FString& ChannelId,
	                                       const FString& Message) = 0;

	/**
	 * Returns the Discord user ID of the guild owner.
	 * The guild owner is allowed to run ban commands even without holding the
	 * configured CommandRoleId.  Empty until the Gateway handshake is complete.
	 */
	virtual const FString& GetGuildOwnerId() const = 0;
};
