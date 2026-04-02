// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * FBanDiscordConfig
 *
 * Configuration for BanSystem's Discord command integration.
 *
 * PRIMARY config (edit this one):
 *   <ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
 * This file is NOT shipped in the mod package so mod updates will never
 * overwrite it.  The mod creates this file automatically on the first server
 * start if it does not exist.
 *
 * This config is only relevant when DiscordBridge (or another mod that
 * implements IBanDiscordCommandProvider) is also installed.  When no provider
 * is registered the settings have no effect.
 *
 * To enable Discord ban commands:
 *   1. Open  <ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
 *   2. Set   BotToken        – Discord bot token (Bot → Token in the Developer Portal).
 *            Treat this value as a password; do not share it.
 *            Required for standalone operation.  Leave empty when using DiscordBridge
 *            (or another IBanDiscordCommandProvider mod) to share its connection instead.
 *   3. Set   DiscordChannelId  – snowflake ID of the channel where ban commands
 *            are issued (same channel used in DiscordBridge, or a dedicated one).
 *   4. Set   DiscordCommandRoleId  – snowflake ID of the Discord role whose
 *            members are allowed to run !steamban, !eosban, etc.
 *   5. Restart the server.
 */
struct BANSYSTEM_API FBanDiscordConfig
{
	// ── Standalone Discord connection ─────────────────────────────────────────

	/**
	 * Discord bot token for BanSystem's own standalone Discord connection.
	 *
	 * When non-empty, BanSystem manages its own Discord Gateway connection and
	 * REST API calls independently of DiscordBridge or any other mod.
	 * Set this to the token from the Discord Developer Portal (Bot → Token).
	 * Treat this value like a password — never share or commit it.
	 *
	 * If left empty, BanSystem waits for an external mod (such as DiscordBridge)
	 * to inject a shared connection via SetCommandProvider().
	 *
	 * How to get it: Discord Developer Portal → your application → Bot → Reset Token.
	 */
	FString BotToken;

	// ── Channel & authorisation ───────────────────────────────────────────────

	/**
	 * Snowflake ID of the Discord channel where BanSystem commands are accepted.
	 * When empty, BanSystem Discord commands are disabled entirely.
	 *
	 * This can be the same channel as DiscordBridge's main ChannelId or a
	 * dedicated admin-only channel.  Command responses are always sent back to
	 * the same channel where the command was received.
	 *
	 * How to get it: enable Developer Mode in Discord (User Settings → Advanced),
	 * right-click the target channel, "Copy Channel ID".
	 */
	FString DiscordChannelId;

	/**
	 * Snowflake ID of the Discord role whose members are allowed to run BanSystem
	 * Discord commands (!steamban, !eosban, !steamunban, !eosunban, etc.).
	 * The guild owner can always run these commands regardless of this setting.
	 * When empty, nobody except the guild owner can use the commands.
	 *
	 * How to get it: Discord Settings → Advanced → Developer Mode, then
	 * right-click the role in Server Settings → Roles and choose Copy Role ID.
	 */
	FString DiscordCommandRoleId;

	// ── Command prefixes ──────────────────────────────────────────────────────
	// Set any prefix to an empty string to disable that specific command.
	// The default prefixes mirror the in-game chat command names for consistency.

	/** Prefix for the Steam ban command.   Syntax: !steamban <ID|Name> [minutes] [reason] */
	FString SteamBanCommandPrefix{ TEXT("!steamban") };

	/** Prefix for the Steam unban command.  Syntax: !steamunban <Steam64Id> */
	FString SteamUnbanCommandPrefix{ TEXT("!steamunban") };

	/** Prefix for the Steam ban list command.  Syntax: !steambanlist */
	FString SteamBanListCommandPrefix{ TEXT("!steambanlist") };

	/** Prefix for the EOS ban command.   Syntax: !eosban <ID|Name> [minutes] [reason] */
	FString EOSBanCommandPrefix{ TEXT("!eosban") };

	/** Prefix for the EOS unban command.  Syntax: !eosunban <EOSProductUserId> */
	FString EOSUnbanCommandPrefix{ TEXT("!eosunban") };

	/** Prefix for the EOS ban list command.  Syntax: !eosbanlist */
	FString EOSBanListCommandPrefix{ TEXT("!eosbanlist") };

	/**
	 * Prefix for the cross-platform ban-by-name command.
	 * Syntax: !banbyname <PlayerName> [minutes] [reason]
	 * Bans the named connected player on all available platforms simultaneously.
	 */
	FString BanByNameCommandPrefix{ TEXT("!banbyname") };

	/**
	 * Prefix for the player ID listing command.
	 * Syntax: !playerids [PlayerName]
	 * Lists Steam64 and EOS IDs for all connected players, or a specific player.
	 */
	FString PlayerIdsCommandPrefix{ TEXT("!playerids") };

	/**
	 * Prefix for the ban status check command.
	 * Syntax: !checkban <Steam64Id|EOSProductUserId|PlayerName>
	 * Checks whether a player is currently on the Steam or EOS ban list.
	 * Does NOT require the player to be online — raw IDs can be checked offline.
	 */
	FString CheckBanCommandPrefix{ TEXT("!checkban") };

	// ── Response message formats ──────────────────────────────────────────────

	/**
	 * Posted to Discord when a Steam ban is issued via !steamban.
	 * Available placeholders:  %PlayerId%  %Reason%  %BannedBy%  %Duration%
	 */
	FString SteamBanResponseMessage{
		TEXT(":hammer: **BanSystem** - Steam ID `%PlayerId%` banned by **%BannedBy%** "
		     "(%Duration%) - Reason: %Reason%")
	};

	/**
	 * Posted to Discord when a Steam ban is removed via !steamunban.
	 * Available placeholder:  %PlayerId%
	 */
	FString SteamUnbanResponseMessage{
		TEXT(":white_check_mark: **BanSystem** - Steam ID `%PlayerId%` has been unbanned.")
	};

	/**
	 * Posted to Discord when an EOS ban is issued via !eosban.
	 * Available placeholders:  %PlayerId%  %Reason%  %BannedBy%  %Duration%
	 */
	FString EOSBanResponseMessage{
		TEXT(":hammer: **BanSystem** - EOS ID `%PlayerId%` banned by **%BannedBy%** "
		     "(%Duration%) - Reason: %Reason%")
	};

	/**
	 * Posted to Discord when an EOS ban is removed via !eosunban.
	 * Available placeholder:  %PlayerId%
	 */
	FString EOSUnbanResponseMessage{
		TEXT(":white_check_mark: **BanSystem** - EOS ID `%PlayerId%` has been unbanned.")
	};

	// ── Join-kick enforcement messages ───────────────────────────────────────

	/**
	 * Text shown in-game to the player in the disconnected screen when they are
	 * kicked because they are on the Steam ban list.
	 * Leave empty to use the default "[Steam Ban] <reason>" format.
	 */
	FString SteamBanKickReason;

	/**
	 * Text shown in-game to the player in the disconnected screen when they are
	 * kicked because they are on the EOS ban list.
	 * Leave empty to use the default "[EOS Ban] <reason>" format.
	 */
	FString EOSBanKickReason;

	/**
	 * Message posted to Discord when a banned player is kicked on join.
	 * Leave empty to disable this notification.
	 *
	 * Available placeholders:
	 *   %PlayerId%  – the Steam64 ID or EOS PUID of the kicked player
	 *   %Reason%    – the ban reason string
	 */
	FString BanKickDiscordMessage{
		TEXT(":hammer: **BanSystem** - Player `%PlayerId%` tried to join but is banned. Reason: %Reason%")
	};

	// ── Lifecycle ─────────────────────────────────────────────────────────────

	/**
	 * Loads configuration from the mod-folder INI.  Creates the file with
	 * commented defaults if it does not exist yet.
	 */
	static FBanDiscordConfig LoadOrCreate();

	/** Returns the absolute path to the mod-folder INI config file. */
	static FString GetConfigFilePath();
};
