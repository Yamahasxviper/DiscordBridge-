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
	 * Discord role ID whose members are authorised to run full admin ban commands
	 * in Discord (!ban, !tempban, !unban, !unbanname, !banname, !bancheck, !banlist,
	 * !playerhistory, !linkbans, !unlinkbans, !warnings, !clearwarns, !clearwarn,
	 * !note, !notes, !duration, !banreason, !extend, !reloadconfig, !kick,
	 * !mute, !unmute, !tempmute, !mutecheck, !mutelist, !warn, !announce,
	 * !stafflist, !reason, !staffchat, !modban).
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
	 * !kick, !modban, !mute, !unmute, !tempmute, !mutecheck, !mutelist,
	 * !staffchat, !stafflist, !announce.
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

	// ── Authorisation helpers ─────────────────────────────────────────────────

	/** Returns true when Roles contains AdminRoleId (full admin access). */
	bool IsAdminRole(const TArray<FString>& Roles) const;

	/** Returns true when Roles contains AdminRoleId OR ModeratorRoleId. */
	bool IsModeratorRole(const TArray<FString>& Roles) const;

	// ── Static helpers ────────────────────────────────────────────────────────

	/** Load settings from DefaultBanBridge.ini (+ backup) and return a populated config. */
	static FBanBridgeConfig Load();

	/** Returns the absolute path to the primary config file (DefaultBanBridge.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/DiscordBridge/BanBridge.ini). */
	static FString GetBackupFilePath();
};
