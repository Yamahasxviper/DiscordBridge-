// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Configuration for the Discord Bridge mod.
 *
 * PRIMARY config (edit this one):
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
 * This file is NOT shipped in the mod package, so mod updates will never
 * overwrite it.  The mod creates this file automatically on the first server
 * start if it does not exist.  All settings – connection, chat, whitelist,
 * and ban system – are in this single file.
 *
 * BACKUP config (auto-managed, extra safety net):
 *   <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini
 * Written automatically on every server start.  If the primary file is ever
 * missing or has empty credentials (e.g. after a manual deletion or a direct
 * Alpakit dev-mode deploy that deletes and recreates the mod directory), the
 * mod falls back to this backup so the bridge keeps working.
 *
 * To enable the bot:
 *   1. Open  <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
 *   2. Set   BotToken  – the token from the Discord Developer Portal (Bot → Token).
 *            Treat this value as a password; do not share it.
 *   3. Set   ChannelId – the snowflake ID of the target text channel.
 *            Enable Developer Mode in Discord, right-click the channel, "Copy Channel ID".
 *   4. Restart the server.
 *
 * Discord bot requirements
 * ────────────────────────
 *  • Privileged Gateway Intents that must be enabled in the Discord Developer
 *    Portal (Bot → Privileged Gateway Intents):
 *      – Server Members Intent  (GUILD_MEMBERS)
 *      – Message Content Intent (MESSAGE_CONTENT)
 *  • The bot must be invited to your server with at minimum the
 *    "Send Messages" and "Read Message History" permissions in the target channel.
 */
struct DISCORDBRIDGE_API FDiscordBridgeConfig
{
	// ── Connection ────────────────────────────────────────────────────────────

	/** Discord bot token (Bot → Token in the Developer Portal). Treat as a password. */
	FString BotToken;

	/** Snowflake ID of the Discord text channel to bridge with in-game chat. */
	FString ChannelId;

	// ── Message formats ───────────────────────────────────────────────────────

	/** Display name for this server used as the %ServerName% placeholder in
	 *  GameToDiscordFormat.  Leave empty to omit the server label. */
	FString ServerName;

	/** Format for game → Discord messages. Placeholders: %ServerName%, %PlayerName%, %Message%. */
	FString GameToDiscordFormat{ TEXT("**%PlayerName%**: %Message%") };

	/** Format for Discord → game messages.  This single string controls the complete
	 *  line of text shown in the Satisfactory in-game chat whenever a Discord message
	 *  is relayed.  Available placeholders:
	 *    %Username%   – the Discord display name of the sender
	 *    %PlayerName% – alias for %Username%
	 *    %Message%    – the Discord message text
	 *  Default: "[Discord] %Username%: %Message%" */
	FString DiscordToGameFormat{ TEXT("[Discord] %Username%: %Message%") };

	// ── Behaviour ─────────────────────────────────────────────────────────────

	/** When true, messages from bot accounts are ignored (prevents echo loops). */
	bool bIgnoreBotMessages{ true };

	// ── Server status messages ────────────────────────────────────────────────

	/**
	 * Master on/off switch for server status messages (online/offline
	 * notifications).  When false, neither ServerOnlineMessage nor
	 * ServerOfflineMessage is ever posted to Discord regardless of their values.
	 * Default: true.
	 */
	bool bServerStatusMessagesEnabled{ true };

	/**
	 * Snowflake ID of a dedicated Discord channel for server status messages
	 * (online/offline notifications).  When set, status messages are posted to
	 * this channel instead of the main ChannelId so that they appear in their
	 * own channel and do not mix with regular chat.  Leave empty to post status
	 * messages to the main bridged channel (ChannelId).
	 *
	 * Get the channel ID the same way as ChannelId (right-click the channel in
	 * Discord with Developer Mode enabled → Copy Channel ID).
	 *
	 * Example: StatusChannelId=111222333444555666777
	 */
	FString StatusChannelId;

	/** Posted to Discord when the server comes online. Leave empty to disable. */
	FString ServerOnlineMessage{ TEXT(":green_circle: Server is now **online**!") };

	/** Posted to Discord when the server shuts down. Leave empty to disable. */
	FString ServerOfflineMessage{ TEXT(":red_circle: Server is now **offline**.") };

	// ── Whitelist ─────────────────────────────────────────────────────────────

	/**
	 * When true, the whitelist is enabled on every server start, overriding any
	 * runtime change made via !whitelist on / !whitelist off Discord commands.
	 * Default: false (all players can join).
	 */
	bool bWhitelistEnabled{ false };

	/**
	 * Snowflake ID of the Discord role whose members are allowed to run
	 * !whitelist management commands.
	 * Leave empty (or unset) to disable !whitelist commands entirely – no
	 * Discord user will be able to run them until a role ID is provided.
	 *
	 * IMPORTANT: holding this role does NOT grant automatic access to the game
	 * server.  Discord members with this role are still subject to the whitelist
	 * and ban checks when they join; they must be added to the whitelist separately.
	 *
	 * To get the role ID: Discord Settings → Advanced → Developer Mode, then
	 * right-click the role in Server Settings → Roles and choose Copy Role ID.
	 */
	FString WhitelistCommandRoleId;

	/**
	 * Snowflake ID of the Discord role whose members are allowed to run
	 * !ban management commands.
	 * Leave empty (or unset) to disable !ban commands entirely – no Discord
	 * user will be able to run them until a role ID is provided.
	 *
	 * This role is also the one granted or revoked by the
	 * `!ban role add <user_id>` and `!ban role remove <user_id>` commands,
	 * so holders can promote or demote other Discord members from within Discord
	 * without needing server-level role management access.
	 * The bot must have the **Manage Roles** permission for those commands to work.
	 *
	 * IMPORTANT: holding this role does NOT grant automatic access to the game
	 * server.  Discord members with this role are still subject to the whitelist
	 * and ban checks when they join; they must be added to the whitelist separately.
	 *
	 * To get the role ID: Discord Settings → Advanced → Developer Mode, then
	 * right-click the role in Server Settings → Roles and choose Copy Role ID.
	 */
	FString BanCommandRoleId;

	/**
	 * Prefix that triggers whitelist management commands from Discord.
	 * Set to an empty string to disable Discord-based whitelist management.
	 * Default: "!whitelist"
	 *
	 * Supported commands (type in the bridged Discord channel):
	 *   !whitelist on                       – enable the whitelist
	 *   !whitelist off                      – disable the whitelist
	 *   !whitelist add <name>               – add a player
	 *   !whitelist remove <name>            – remove a player
	 *   !whitelist list                     – list all whitelisted players
	 *   !whitelist status                   – show current enabled/disabled state
	 *   !whitelist role add <discord_id>    – grant WhitelistRoleId to a Discord user
	 *   !whitelist role remove <discord_id> – revoke WhitelistRoleId from a Discord user
	 */
	FString WhitelistCommandPrefix{ TEXT("!whitelist") };

	/**
	 * Snowflake ID of the Discord role used to identify whitelisted members.
	 * Leave empty to disable Discord role integration.
	 *
	 * When set:
	 *  • Messages received on WhitelistChannelId are relayed to the game only
	 *    when the sender holds this role.
	 *  • The `!whitelist role add/remove <user_id>` commands assign or revoke
	 *    this role via the Discord REST API (bot must have Manage Roles permission).
	 *  • At bot startup the member list is fetched from Discord and cached.
	 *    Any player whose in-game name matches a cached Discord display name
	 *    (server nickname, global name, or username) is allowed through the
	 *    whitelist check without needing an explicit entry in ServerWhitelist.json.
	 *    The cache is kept up to date by GUILD_MEMBER_ADD/UPDATE/REMOVE events.
	 */
	FString WhitelistRoleId;

	/**
	 * Snowflake ID of a dedicated Discord channel for whitelisted members.
	 * Leave empty to disable the whitelist-only channel.
	 *
	 * When set:
	 *  • In-game messages from players on the server whitelist are posted to
	 *    this channel in addition to the main ChannelId.
	 *  • Discord messages sent to this channel are relayed to the game only
	 *    when the sender holds WhitelistRoleId (if that field is non-empty).
	 */
	FString WhitelistChannelId;

	/**
	 * Snowflake ID of a dedicated Discord channel for ban management.
	 * Leave empty to disable the ban-only channel.
	 *
	 * When set:
	 *  • !ban commands issued from this channel are accepted (sender must still
	 *    hold BanCommandRoleId).  Command responses are sent back to this channel.
	 *  • Ban-kick notifications are also posted here (in addition to the main
	 *    ChannelId), giving admins a focused audit log of bans.
	 *
	 * Get the channel ID the same way as ChannelId (right-click the channel in
	 * Discord with Developer Mode enabled → Copy Channel ID).
	 *
	 * Example: BanChannelId=567890123456789012
	 */
	FString BanChannelId;

	/**
	 * Message posted to the main Discord channel whenever the whitelist kicks
	 * a player who tried to join.  Leave empty to disable the notification.
	 *
	 * Available placeholder:
	 *   %PlayerName%  – in-game name of the player who was kicked.
	 *
	 * Example:
	 *   WhitelistKickDiscordMessage=:boot: **%PlayerName%** is not whitelisted and was kicked.
	 */
	FString WhitelistKickDiscordMessage{
		TEXT(":boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.")
	};

	/**
	 * Reason shown in-game to the player when they are kicked because they are
	 * not on the whitelist.  This is the text the player sees in the "Disconnected"
	 * screen.
	 * Default: "You are not on this server's whitelist. Contact the server admin to be added."
	 */
	FString WhitelistKickReason{
		TEXT("You are not on this server's whitelist. Contact the server admin to be added.")
	};

	// ── Ban system ────────────────────────────────────────────────────────────

	/**
	 * Prefix that triggers ban management commands from Discord.
	 * Set to an empty string to disable Discord-based ban management.
	 * Default: "!ban"
	 *
	 * All ban commands are handled by the BanSystem mod (UBanDiscordSubsystem).
	 * Supported commands (type in the bridged Discord channel):
	 *   !ban add <name> [duration_minutes] [reason] – ban a connected player by name
	 *   !ban remove <name>                          – unban an online player by name
	 *   !ban list                                   – show all active Steam + EOS bans
	 *   !ban status                                 – show ban count summary
	 *   !ban role add <discord_id>                  – grant ban-admin Discord role
	 *   !ban role remove <discord_id>               – revoke ban-admin Discord role
	 */
	FString BanCommandPrefix{ TEXT("!ban") };

	// ── BanSystem Mod Integration ─────────────────────────────────────────────

	/**
	 * Message posted to Discord when the BanSystem mod bans a player by Steam64 ID
	 * (via /steamban or /banbyname in-game chat commands).
	 * Leave empty to disable the notification.
	 *
	 * Available placeholders:
	 *   %PlayerId%  – the Steam64 ID of the banned player
	 *   %Reason%    – the ban reason string
	 *   %BannedBy%  – the name of the admin who issued the ban
	 */
	FString BanSystemSteamBanDiscordMessage{
		TEXT(":hammer: **BanSystem** - Steam ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%")
	};

	/**
	 * Message posted to Discord when the BanSystem mod unbans a player by Steam64 ID
	 * (via /steamunban in-game chat command).
	 * Leave empty to disable the notification.
	 *
	 * Available placeholder:
	 *   %PlayerId%  – the Steam64 ID of the unbanned player
	 */
	FString BanSystemSteamUnbanDiscordMessage{
		TEXT(":white_check_mark: **BanSystem** - Steam ID `%PlayerId%` has been unbanned.")
	};

	/**
	 * Message posted to Discord when the BanSystem mod bans a player by EOS Product User ID
	 * (via /eosban or /banbyname in-game chat commands).
	 * Leave empty to disable the notification.
	 *
	 * Available placeholders:
	 *   %PlayerId%  – the EOS Product User ID of the banned player
	 *   %Reason%    – the ban reason string
	 *   %BannedBy%  – the name of the admin who issued the ban
	 */
	FString BanSystemEOSBanDiscordMessage{
		TEXT(":hammer: **BanSystem** - EOS ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%")
	};

	/**
	 * Message posted to Discord when the BanSystem mod unbans a player by EOS Product User ID
	 * (via /eosunban in-game chat command).
	 * Leave empty to disable the notification.
	 *
	 * Available placeholder:
	 *   %PlayerId%  – the EOS Product User ID of the unbanned player
	 */
	FString BanSystemEOSUnbanDiscordMessage{
		TEXT(":white_check_mark: **BanSystem** - EOS ID `%PlayerId%` has been unbanned.")
	};

	// ── In-game commands ──────────────────────────────────────────────────────

	/**
	 * Prefix that triggers whitelist management commands when typed in the
	 * Satisfactory in-game chat.  Set to an empty string to disable in-game
	 * whitelist commands.
	 * Default: "!whitelist"
	 *
	 * Supported commands (type in the Satisfactory in-game chat):
	 *   !whitelist on            – enable the whitelist
	 *   !whitelist off           – disable the whitelist
	 *   !whitelist add <name>    – add a player by in-game name
	 *   !whitelist remove <name> – remove a player by in-game name
	 *   !whitelist list          – list all whitelisted players
	 *   !whitelist status        – show current enabled/disabled state
	 */
	FString InGameWhitelistCommandPrefix{ TEXT("!whitelist") };

	// ── Player count presence ─────────────────────────────────────────────────

	/** When true, the bot's Discord presence activity shows the current player count. */
	bool bShowPlayerCountInPresence{ true };

	/** The custom text shown in the bot's Discord presence.
	 *  Type whatever you like.  Use %PlayerCount% to insert the live player count
	 *  and %ServerName% to insert the configured server name.
	 *  Example: "My Server with %PlayerCount% players" */
	FString PlayerCountPresenceFormat{ TEXT("Satisfactory with %PlayerCount% players") };

	/** How often (in seconds) to refresh the player count shown in the bot's presence.
	 *  Must be at least 15 seconds; the default is 60 seconds. */
	float PlayerCountUpdateIntervalSeconds{ 60.0f };

	/** Discord activity type used when displaying the player count.
	 *  Controls the verb shown before the activity text in Discord:
	 *   0 = Playing         → "Playing <your text>"
	 *   2 = Listening to    → "Listening to <your text>"
	 *   3 = Watching        → "Watching <your text>"
	 *   5 = Competing in    → "Competing in <your text>"
	 *  Default: 0 (Playing). */
	int32 PlayerCountActivityType{ 0 };

	/**
	 * Loads configuration from the primary mod-folder INI, falling back to the
	 * Saved/Config backup when credentials are missing.  If the primary file does
	 * not exist it is created with default values.  On every server start, an
	 * up-to-date backup of ALL settings is written to the Saved/Config location so
	 * they survive the next mod update (even when BotToken is still empty).
	 *
	 * When credentials are absent from the primary (e.g. after a mod update
	 * resets DefaultDiscordBridge.ini) but present in the backup, ALL settings are
	 * restored from the backup and also written back into the primary config file
	 * so operators can always see and manage their settings there.
	 *
	 * A single backup file is written on every server start (located in
	 * Saved/Config/, which Alpakit/SMM mod updates never touch):
	 *   DiscordBridge.ini  – all settings (connection, chat, presence, whitelist, ban)
	 *
	 * IMPORTANT FOR FUTURE MAINTAINERS: when adding a new field to
	 * FDiscordBridgeConfig, you must update DiscordBridgeConfig.cpp in ALL
	 * four places marked with the tag [BACKUP-SYNC]:
	 *   1. Primary config load   (Step 1 – read from DefaultDiscordBridge.ini)
	 *   2. Backup restore        (Step 2 – read from Saved/Config/DiscordBridge.ini)
	 *   3. Primary patch-back    (Step 2 PatchLine calls – write back to primary)
	 *   4. Backup write          (Step 3 – write to Saved/Config/DiscordBridge.ini)
	 * Also add the key to the DefaultContent template and the second-pass upgrade
	 * check so existing server configs pick up the new setting automatically.
	 */
	static FDiscordBridgeConfig LoadOrCreate();

	/** Returns the absolute path to the primary INI config file (mod folder). */
	static FString GetModConfigFilePath();

	/** Returns the absolute path to the backup INI config file (Saved/Config/). */
	static FString GetBackupConfigFilePath();
};
