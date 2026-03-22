// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * IBanDiscordCommandProvider
 *
 * Abstract interface through which UBanDiscordSubsystem communicates with the
 * Discord bridge.  Implemented by UDiscordBridgeSubsystem in the DiscordBridge
 * mod.  Decouples BanSystem from a hard compile-time dependency on DiscordBridge
 * so that BanSystem remains fully standalone.
 *
 * DiscordBridge calls UBanDiscordSubsystem::SetProvider(this) during its own
 * Initialize() to inject itself as the active provider.  When DiscordBridge
 * is deinitialized it calls SetProvider(nullptr) to detach.
 *
 * BanSystem has zero compile-time knowledge of any provider — the coupling is
 * one-directional: DiscordBridge depends on BanSystem; BanSystem never depends
 * on DiscordBridge.
 */
class IBanDiscordCommandProvider
{
public:
	virtual ~IBanDiscordCommandProvider() = default;

	/**
	 * Post a plain-text message to a Discord channel via the REST API.
	 *
	 * @param ChannelId  Snowflake ID of the destination Discord channel.
	 * @param Message    Plain-text content (Discord markdown supported).
	 */
	virtual void SendBanDiscordMessage(const FString& ChannelId, const FString& Message) = 0;

	/**
	 * Grant or revoke the configured ban-admin Discord role from a guild member.
	 *
	 * When BanCommandRoleId is not configured in DefaultDiscordBridge.ini the
	 * call is a no-op and the method returns false.
	 *
	 * @param TargetUserId  Discord user snowflake to modify.
	 * @param bGrant        true = grant the role, false = revoke it.
	 * @return true when the role ID is configured and the REST call was dispatched.
	 */
	virtual bool ManageBanDiscordRole(const FString& TargetUserId, bool bGrant) = 0;
};
