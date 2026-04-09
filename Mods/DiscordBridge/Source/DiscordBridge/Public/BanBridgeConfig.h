// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanDiscord, Log, All);

/**
 * FBanBridgeConfig
 *
 * Plain-old-data struct that holds settings for the BanSystem ↔ Discord
 * integration.  Loaded once per server start from DefaultBanBridge.ini
 * (with a backup in Saved/DiscordBridge/BanBridge.ini).
 */
struct DISCORDBRIDGE_API FBanBridgeConfig
{
	/**
	 * Discord role ID whose members are authorised to run ban commands in Discord
	 * (!ban, !tempban, !unban, !bancheck, !banlist, !playerhistory).
	 *
	 * How to get the role ID: enable Developer Mode in Discord
	 * (User Settings → Advanced → Developer Mode), open Server Settings → Roles,
	 * right-click the desired role, and choose "Copy Role ID".
	 *
	 * Leave empty to disable all Discord ban commands.
	 */
	FString AdminRoleId;

	/**
	 * Optional: when non-empty, ban commands are only accepted from this Discord
	 * channel.  Leave empty to allow commands from any channel where the bot
	 * can read messages.
	 *
	 * How to get the channel ID: enable Developer Mode, right-click the channel,
	 * and choose "Copy Channel ID".
	 */
	FString BanCommandChannelId;

	/**
	 * Optional: Discord channel ID to post a moderation log message to after
	 * every ban/unban/kick/mute/warn action performed through Discord commands.
	 * Leave empty to disable moderation logging (default).
	 *
	 * How to get the channel ID: enable Developer Mode, right-click the channel,
	 * and choose "Copy Channel ID".
	 */
	FString ModerationLogChannelId;

	// ── Static helpers ────────────────────────────────────────────────────────

	/** Load settings from DefaultBanBridge.ini (+ backup) and return a populated config. */
	static FBanBridgeConfig Load();

	/** Returns the absolute path to the primary config file (DefaultBanBridge.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/DiscordBridge/BanBridge.ini). */
	static FString GetBackupFilePath();
};
