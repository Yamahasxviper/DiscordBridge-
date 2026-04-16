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
	 * Discord role ID whose members are authorised to run full admin slash commands
	 * in Discord (/ban, /warn, /player, /appeal, /admin groups).
	 *
	 * How to get the role ID: enable Developer Mode in Discord
	 * (User Settings → Advanced → Developer Mode), open Server Settings → Roles,
	 * right-click the desired role, and choose "Copy Role ID".
	 *
	 * Leave empty to disable all Discord ban commands.
	 */
	FString AdminRoleId;

	/**
	 * Discord role ID for moderators who may run a limited subset of commands:
	 * /mod kick, /mod modban, /mod mute, /mod unmute, /mod tempmute,
	 * /mod mutecheck, /mod mutelist, /mod staffchat, /mod stafflist, /mod announce.
	 *
	 * Admins (AdminRoleId) automatically pass the moderator check.
	 * Leave empty to disable the moderator role (only admins can use those commands).
	 *
	 * How to get the role ID: enable Developer Mode, open Server Settings → Roles,
	 * right-click the moderator role, and choose "Copy Role ID".
	 */
	FString ModeratorRoleId;

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

	/**
	 * Optional: Discord channel ID where /admin panel posts the interactive
	 * admin panel embed.  When empty the panel is sent to the channel in which
	 * the /admin panel command was issued.  Set this to restrict the panel to a
	 * dedicated mod-tools channel.
	 *
	 * How to get the channel ID: enable Developer Mode, right-click the channel,
	 * and choose "Copy Channel ID".
	 */
	FString AdminPanelChannelId;

	// ── Authorisation helpers ─────────────────────────────────────────────────

	/** Returns true when Roles contains AdminRoleId (full admin access). */
	bool IsAdminRole(const TArray<FString>& Roles) const;

	/** Returns true when Roles contains AdminRoleId OR ModeratorRoleId. */
	bool IsModeratorRole(const TArray<FString>& Roles) const;

	// ── Static helpers ────────────────────────────────────────────────────────

	/** Load settings from DefaultBanBridge.ini (+ backup) and return a populated config. */
	static FBanBridgeConfig Load();

	/**
	 * Creates DefaultBanBridge.ini with an annotated template if the file is absent
	 * or comment-free (Alpakit-stripped).  Call once at subsystem start before Load().
	 */
	static void RestoreDefaultConfigIfNeeded();

	/** Returns the absolute path to the primary config file (DefaultBanBridge.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/DiscordBridge/BanBridge.ini). */
	static FString GetBackupFilePath();
};
