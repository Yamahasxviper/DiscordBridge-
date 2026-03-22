// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * Configuration for the Discord Bridge mod.
 *
 * PRIMARY config files (edit these):
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
 *     Connection, chat, server status, presence, and server control settings.
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultWhitelist.ini
 *     Whitelist settings (WhitelistEnabled, command roles and prefixes, etc.)
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultBan.ini
 *     Ban system settings (BanSystemEnabled, command roles and prefixes, etc.)
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultTickets.ini
 *     Ticket system settings (channels, role mentions, panel, etc.)
 * These files ship with the mod. They are overwritten by Alpakit/SMM mod
 * updates, but the mod automatically saves a backup so settings can be restored.
 *
 * BACKUP config files (auto-managed, survive mod updates):
 *   <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini
 *   <ServerRoot>/FactoryGame/Saved/Config/Whitelist.ini
 *   <ServerRoot>/FactoryGame/Saved/Config/Ban.ini
 *   <ServerRoot>/FactoryGame/Saved/Config/Tickets.ini
 * Each backup mirrors its matching primary file and is written automatically on
 * every server start.  If the primary file is reset by a mod update, the mod
 * falls back to the backup so the bridge keeps working until settings are restored.
 *
 * To enable the bot:
 *   1. Open  <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
 *   2. Set   BotToken  – the token from the Discord Developer Portal (Bot → Token).
 *            Treat this value as a password; do not share it.
 *   3. Set   ChannelId – the Discord ID of the target text channel.
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

	/** Discord ID(s) of the Discord text channel(s) to bridge with in-game chat.
	 *  To bridge multiple channels at once, provide a comma-separated list of
	 *  Discord IDs, e.g. "123456789012345678,987654321098765432".
	 *  All listed channels receive game→Discord messages and are listened to for
	 *  Discord→game relaying and commands. */
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
	 * Discord ID(s) of a dedicated Discord channel(s) for server status messages
	 * (online/offline notifications).  When set, status messages are posted to
	 * these channels instead of the main ChannelId so that they appear in their
	 * own channel and do not mix with regular chat.  Leave empty to post status
	 * messages to the main bridged channel(s) (ChannelId).
	 *
	 * To use multiple status channels, provide a comma-separated list of IDs,
	 * e.g. "111222333444555666777,888999000111222333444".
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

	/**
	 * Posted to Discord when the server crashes unexpectedly (i.e. exits without
	 * a graceful shutdown via !admin stop / !admin restart).  Leave empty to
	 * disable the crash notification.
	 *
	 * A crash is detected when Unreal Engine's OnHandleSystemError delegate fires,
	 * which covers assertion failures, fatal errors, and most other UE-level
	 * crashes.  Low-level hardware faults (e.g. OOM kill) cannot always be caught.
	 *
	 * Default: :warning: **Server crashed!** The server encountered an unexpected error.
	 */
	FString ServerCrashMessage{
		TEXT(":warning: **Server crashed!** The server encountered an unexpected error.")
	};

	// ── Player join / leave notifications ─────────────────────────────────────

	/**
	 * Message posted to the bridged Discord channel when a player successfully
	 * joins the server (after passing ban and whitelist checks).
	 *
	 * Available placeholders:
	 *   %PlayerName% – the in-game name of the joining player.
	 *   %Platform%   – the platform display name (e.g. "Steam", "Epic Games Store").
	 *
	 * Leave empty to disable player join notifications.
	 */
	FString PlayerJoinMessage{ TEXT(":arrow_right: **%PlayerName%** joined the server.") };

	/**
	 * Message posted to the bridged Discord channel when a player leaves the
	 * server, whether by a clean disconnect or a connection timeout.
	 *
	 * Available placeholders:
	 *   %PlayerName% – the in-game name of the leaving player.
	 *   %Platform%   – the platform display name (e.g. "Steam", "Epic Games Store").
	 *
	 * Leave empty to disable player leave notifications.
	 */
	FString PlayerLeaveMessage{ TEXT(":arrow_left: **%PlayerName%** left the server.") };

	/**
	 * Message posted to the bridged Discord channel when a new network
	 * connection is accepted by the server before login completes.  This fires
	 * at the earliest detectable stage of the connection handshake, before the
	 * player's name or identity is known.
	 *
	 * Available placeholder:
	 *   %RemoteAddr%  – the remote IP address and port of the connecting client
	 *                   (e.g. "203.0.113.42:54321").
	 *
	 * Useful for monitoring who is attempting to connect and for correlating
	 * pre-login connection events with server crashes.
	 * Leave empty (default) to disable this notification.
	 *
	 * Example:
	 *   PlayerConnectingMessage=:satellite: New connection attempt from %RemoteAddr%
	 */
	FString PlayerConnectingMessage;

	/**
	 * Message posted to the bridged Discord channel when a network connection
	 * is dropped before login completes (i.e. the connection was never promoted
	 * to a fully-logged-in player).  This covers failed handshakes, rejected
	 * login requests, client-side cancellations, and any crash that terminates
	 * the join flow before PostLogin fires.
	 *
	 * Available placeholder:
	 *   %RemoteAddr%  – the remote IP address and port that was dropped.
	 *
	 * Leave empty (default) to disable this notification.
	 *
	 * Example:
	 *   PlayerConnectionDroppedMessage=:warning: Pre-login connection from %RemoteAddr% was dropped.
	 */
	FString PlayerConnectionDroppedMessage;

	// ── Whitelist ─────────────────────────────────────────────────────────────

	/**
	 * When true, the whitelist is enabled on every server start, overriding any
	 * runtime change made via !whitelist on / !whitelist off Discord commands.
	 * Default: false (all players can join).
	 */
	bool bWhitelistEnabled{ false };

	/**
	 * Discord role ID whose members are allowed to run
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
	 * Discord role ID whose members are allowed to run
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
	 * Discord role ID used to identify whitelisted members.
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
	 * Discord ID(s) of a dedicated Discord channel(s) for whitelisted members.
	 * Leave empty to disable the whitelist-only channel.
	 *
	 * To use multiple whitelist channels, provide a comma-separated list of IDs,
	 * e.g. "222333444555666777,444555666777888999".
	 *
	 * When set:
	 *  • In-game messages from players on the server whitelist are posted to
	 *    these channels in addition to the main ChannelId(s).
	 *  • Discord messages sent to these channels are relayed to the game only
	 *    when the sender holds WhitelistRoleId (if that field is non-empty).
	 */
	FString WhitelistChannelId;

	/**
	 * Discord ID(s) of a dedicated Discord channel(s) for ban management.
	 * Leave empty to disable the ban-only channel.
	 *
	 * To use multiple ban channels, provide a comma-separated list of IDs,
	 * e.g. "567890123456789012,112233445566778899".
	 *
	 * When set:
	 *  • !ban commands issued from any of these channels are accepted (sender
	 *    must still hold BanCommandRoleId).  Command responses are sent back to
	 *    the channel where the command was issued.
	 *  • Ban-kick notifications are also posted to all of these channels (in
	 *    addition to the main ChannelId(s)), giving admins a focused audit log
	 *    of bans.
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
	 * When true, the ban system is active on every server start, overriding any
	 * runtime change made via !ban on / !ban off Discord commands.
	 * Default: true (ban list is enforced; set to false to disable enforcement).
	 */
	bool bBanSystemEnabled{ true };

	/**
	 * When true (default), Discord `!ban` commands and in-game `!ban` chat commands
	 * are enabled.  Set to false to disable the entire ban command interface while
	 * still enforcing bans on join (bBanSystemEnabled is unaffected).
	 *
	 * This is the "on/off from config" toggle for the command interface:
	 *   bBanCommandsEnabled=True   → admins can run !ban commands (subject to BanCommandRoleId)
	 *   bBanCommandsEnabled=False  → !ban commands are silently ignored; bans still enforced
	 *
	 * Default: true.
	 */
	bool bBanCommandsEnabled{ true };

	/**
	 * Prefix that triggers ban management commands from Discord.
	 * Set to an empty string to disable Discord-based ban management.
	 * Default: "!ban"
	 *
	 * Supported commands (type in the bridged Discord channel):
	 *   !ban players                  – list all connected players with platform
	 *                                   (Steam/Epic) and EOS Product User ID
	 *   !ban check <name>             – check if a player is banned (name + ID)
	 *   !ban add <name>               – ban a player by in-game name
	 *   !ban remove <name>            – unban a player by in-game name
	 *   !ban list                     – list all banned player names
	 *   !ban id lookup <name>         – look up EOS PUID of a connected player
	 *   !ban id add <platform_id>     – ban by EOS Product User ID (Steam & Epic)
	 *   !ban id remove <platform_id>  – unban by EOS Product User ID
	 *   !ban id list                  – list all banned platform IDs
	 *   !ban status                   – show ban system, whitelist, and EOS state
	 *   !ban on                       – enable the ban system
	 *   !ban off                      – disable the ban system
	 *   !ban role add <discord_id>    – grant BanCommandRoleId to a Discord user
	 *   !ban role remove <discord_id> – revoke BanCommandRoleId from a Discord user
	 */
	FString BanCommandPrefix{ TEXT("!ban") };

	/**
	 * Message posted to the main Discord channel whenever a banned player tries
	 * to join.  Leave empty to disable the notification.
	 *
	 * Available placeholder:
	 *   %PlayerName%  – in-game name of the player who was kicked.
	 *
	 * Example:
	 *   BanKickDiscordMessage=:hammer: **%PlayerName%** is banned and was kicked.
	 */
	FString BanKickDiscordMessage{
		TEXT(":hammer: **%PlayerName%** is banned from this server and was kicked.")
	};

	/**
	 * Message posted to the Discord channel(s) whenever a banned player is
	 * rejected by the pre-login ban check (platform-ID ban enforcement at
	 * connection time, before the player fully joins the server).  Leave empty
	 * to disable the notification.
	 *
	 * Unlike BanKickDiscordMessage (which fires after the player joins and is
	 * then kicked), this fires at the very start of the connection handshake,
	 * so the player's in-game display name is not yet available.
	 *
	 * Available placeholders:
	 *   %PlatformId%    – the banned platform ID (Steam64 or EOS PUID).
	 *   %PlatformType%  – "Steam" or "EOS PUID", depending on the ID format.
	 *
	 * Example:
	 *   BanLoginRejectDiscordMessage=:no_entry: A banned player (%PlatformType% `%PlatformId%`) was rejected at login.
	 */
	FString BanLoginRejectDiscordMessage{
		TEXT(":no_entry: A banned player (%PlatformType% `%PlatformId%`) tried to connect and was rejected.")
	};

	/**
	 * Reason shown in-game to the player when they are kicked for being banned.
	 * This is the text the player sees in the "Disconnected" screen.
	 * Default: "You are banned from this server."
	 */
	FString BanKickReason{ TEXT("You are banned from this server.") };

	/**
	 * How often (in seconds) to scan all connected players against the ban list
	 * and kick any who are banned.  This acts as a safety net for bans that are
	 * added directly to ServerBanlist.json without going through the bot commands
	 * (bot commands already kick the player immediately when a ban is issued).
	 *
	 * Set to 0 to disable the periodic scan entirely.  Must be at least 30 seconds
	 * when enabled.  Default: 300 (5 minutes).
	 *
	 * Example: BanScanIntervalSeconds=300
	 */
	float BanScanIntervalSeconds{ 300.0f };

	// ── Kick command ──────────────────────────────────────────────────────────

	/**
	 * Discord role ID whose members are permitted to run !kick commands.
	 * When set, ONLY members of this role can use !kick commands.
	 * When left empty (the default), ALL !kick commands are disabled for every
	 * Discord user until a role ID is provided here (deny-by-default).
	 *
	 * Sharing this role with BanCommandRoleId is fine: the same Discord admins
	 * can then use both !kick (temporary, no permanent record) and !ban commands.
	 */
	FString KickCommandRoleId;

	/**
	 * Prefix that triggers the kick command from Discord.
	 * Set to an empty string to disable Discord-based kick management.
	 * Default: "!kick"
	 *
	 * Supported commands (type in the bridged Discord channel):
	 *   !kick <PlayerName>             – kick a player (default reason)
	 *   !kick <PlayerName> <reason>    – kick a player with a custom reason
	 *
	 * Unlike !ban add, kick does NOT add the player to the ban list.
	 * The player can reconnect immediately after being kicked.
	 */
	FString KickCommandPrefix{ TEXT("!kick") };

	/**
	 * When true (default), Discord !kick commands and in-game !kick chat commands
	 * are enabled.  Set to false to disable the kick command interface.
	 *
	 * Default: true.
	 */
	bool bKickCommandsEnabled{ true };

	/**
	 * Message posted to the Discord channel(s) when a player is kicked via !kick.
	 * Leave empty to disable the notification.
	 *
	 * Available placeholders:
	 *   %PlayerName%  – in-game name of the player who was kicked.
	 *   %Reason%      – the kick reason (from the command argument or KickReason default).
	 *
	 * Example:
	 *   KickDiscordMessage=:boot: **%PlayerName%** was kicked by an admin. Reason: %Reason%
	 */
	FString KickDiscordMessage{
		TEXT(":boot: **%PlayerName%** was kicked by an admin.")
	};

	/**
	 * Default reason shown in-game to a kicked player when no reason is provided
	 * in the !kick command.  This is the text the player sees in the
	 * "Disconnected" screen.
	 * Default: "You have been kicked from the server by an admin."
	 */
	FString KickReason{ TEXT("You have been kicked from the server by an admin.") };

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

	/**
	 * Prefix that triggers ban management commands when typed in the
	 * Satisfactory in-game chat.  Set to an empty string to disable in-game
	 * ban commands.
	 * Default: "!ban"
	 *
	 * Supported commands (type in the Satisfactory in-game chat):
	 *   !ban players                 – list connected players with platform + PUID
	 *   !ban check <name>            – check if a player is banned
	 *   !ban add <name>              – ban a player by in-game name
	 *   !ban remove <name>           – unban a player by in-game name
	 *   !ban list                    – list all banned player names
	 *   !ban id lookup <name>        – look up EOS PUID of a connected player
	 *   !ban id add <platform_id>    – ban by EOS Product User ID (Steam & Epic)
	 *   !ban id remove <platform_id> – unban by EOS Product User ID
	 *   !ban id list                 – list all banned platform IDs
	 *   !ban status                  – show ban system, whitelist, and EOS state
	 *   !ban on                      – enable the ban system
	 *   !ban off                     – disable the ban system
	 */
	FString InGameBanCommandPrefix{ TEXT("!ban") };

	/**
	 * Prefix that triggers the kick command when typed in the Satisfactory
	 * in-game chat.  Set to an empty string to disable in-game kick commands.
	 * Default: "!kick"
	 *
	 * Supported commands (type in the Satisfactory in-game chat):
	 *   !kick <PlayerName>           – kick a player (default KickReason)
	 *   !kick <PlayerName> <reason>  – kick a player with a custom reason
	 */
	FString InGameKickCommandPrefix{ TEXT("!kick") };

	// ── Server info commands ──────────────────────────────────────────────────

	/**
	 * Prefix that triggers server-information commands from Discord.
	 * Set to an empty string to disable all server-info commands.
	 * Unlike ban/whitelist commands, server-info commands require no role – any
	 * member of the bridged channel can run them.
	 * Default: "!server"
	 *
	 * Supported commands (type in the bridged Discord channel):
	 *   !server players  – list the names of all currently online players
	 *   !server online   – alias for !server players
	 *   !server status   – show server name, online/offline state, and player count
	 *   !server eos      – EOS / platform diagnostics (credentials, runtime state)
	 *   !server help     – list all available bot commands
	 */
	FString ServerInfoCommandPrefix{ TEXT("!server") };

	/**
	 * Prefix that triggers server-information commands when typed in the
	 * Satisfactory in-game chat.  Set to an empty string to disable in-game
	 * server-info commands.
	 * Default: "!server"
	 *
	 * Supported commands (type in the Satisfactory in-game chat):
	 *   !server players  – list the names of all currently online players
	 *   !server status   – show server name and player count
	 *   !server eos      – EOS / platform diagnostics in plain text
	 *   !server help     – list available in-game server-info commands
	 */
	FString InGameServerInfoCommandPrefix{ TEXT("!server") };

	// ── Ticket / support system ───────────────────────────────────────────────

	/**
	 * Discord ID(s) of a dedicated Discord channel(s) where submitted tickets
	 * are posted for admin review.  When empty, tickets are posted to the main
	 * bridged channel(s) (ChannelId) instead.
	 *
	 * To post tickets to multiple channels, provide a comma-separated list of
	 * IDs, e.g. "345678901234567890,678901234567890123".
	 *
	 * Get the channel ID the same way as ChannelId (right-click the channel in
	 * Discord with Developer Mode enabled → Copy Channel ID).
	 *
	 * Example: TicketChannelId=345678901234567890
	 */
	FString TicketChannelId;

	/**
	 * When true (default), the Whitelist Request button is shown on the ticket
	 * panel and members can open whitelist tickets.  Set to false to hide the
	 * button and disable this ticket type.
	 */
	bool bTicketWhitelistEnabled{ true };

	/**
	 * When true (default), the Help / Support button is shown on the ticket
	 * panel and members can open help tickets.  Set to false to hide the button
	 * and disable this ticket type.
	 */
	bool bTicketHelpEnabled{ true };

	/**
	 * When true (default), the Report a Player button is shown on the ticket
	 * panel and members can open report tickets.  Set to false to hide the
	 * button and disable this ticket type.
	 */
	bool bTicketReportEnabled{ true };

	/**
	 * Discord role ID to @mention in every ticket notification
	 * posted to TicketChannelId (or the main channel).  When set, the bot
	 * prepends "<@&ROLE_ID>" to each ticket message so all members holding
	 * that role receive a Discord ping.  Leave empty to post tickets without
	 * any mention.
	 *
	 * To get the role ID: Discord Settings -> Advanced -> Developer Mode, then
	 * right-click the role in Server Settings -> Roles -> Copy Role ID.
	 * Example: TicketNotifyRoleId=123456789012345678
	 */
	FString TicketNotifyRoleId;

	/**
	 * Discord channel ID of the channel where the ticket panel (the message
	 * with clickable buttons) is posted by the `!admin ticket-panel` command.
	 * Members click one of the buttons instead of typing `!ticket` commands.
	 * Leave empty to use the main bridged channel (ChannelId) as the panel channel.
	 *
	 * RECOMMENDED: create a read-only channel (members can read but not post)
	 * and set its ID here so the panel stays visible at the top of the channel.
	 *
	 * How to get the channel ID: enable Developer Mode in Discord
	 * (User Settings -> Advanced -> Developer Mode), right-click the channel,
	 * and choose "Copy Channel ID".
	 * Example: TicketPanelChannelId=123456789012345678
	 */
	FString TicketPanelChannelId;

	/**
	 * Discord category ID under which newly created ticket channels are placed.
	 * When set, each ticket channel is created inside this category so all
	 * tickets are grouped in one place in the Discord sidebar.
	 * Leave empty to create ticket channels without a category.
	 *
	 * How to get the category ID: enable Developer Mode in Discord, right-click
	 * the category name, and choose "Copy Channel ID".
	 * Example: TicketCategoryId=123456789012345678
	 */
	FString TicketCategoryId;

	/**
	 * Custom ticket reasons shown as additional buttons on the ticket panel.
	 * Each entry must be formatted as "Label|Description" where:
	 *   Label       - The text shown on the Discord button (keep short, ~40 chars max).
	 *   Description - A brief summary shown in the panel message body.
	 *
	 * You can add as many entries as you need.  Discord allows at most 25 buttons
	 * per message (5 action rows x 5 buttons), so the combined count of enabled
	 * built-in types plus custom reasons must not exceed 25.
	 *
	 * Format in DefaultTickets.ini (one entry per line):
	 *   TicketReason=Bug Report|Report a bug or technical issue with the server
	 *   TicketReason=Suggestion|Submit a suggestion or feature request
	 *
	 * Leave empty to show only the built-in ticket types.
	 */
	TArray<FString> CustomTicketReasons;

	/**
	 * Discord role ID to @mention in every server status
	 * notification (ServerOnlineMessage / ServerOfflineMessage).  When set,
	 * the bot prepends "<@&ROLE_ID>" to each status message so all members
	 * holding that role receive a Discord ping when the server starts or stops.
	 * Leave empty to post status messages without any mention.
	 *
	 * Use @everyone or @here by setting this to "everyone" or "here" instead
	 * of a Discord role ID to mention all channel members.
	 *
	 * To get the role ID: Discord Settings -> Advanced -> Developer Mode, then
	 * right-click the role in Server Settings -> Roles -> Copy Role ID.
	 * Example: ServerStatusNotifyRoleId=123456789012345678
	 */
	FString ServerStatusNotifyRoleId;

	// ── Server control commands (admin-only) ──────────────────────────────────

	/**
	 * Prefix that triggers server control commands from Discord.
	 * These commands are ADMIN-ONLY: only Discord members who hold
	 * ServerControlCommandRoleId may use them.  When ServerControlCommandRoleId
	 * is empty the commands are disabled for everyone (deny-by-default).
	 * Set this prefix to an empty string to disable the command group entirely.
	 * Default: "!admin"
	 *
	 * Supported commands (type in any bridged Discord channel):
	 *   !admin start    – replies that the server is already online
	 *   !admin stop     – gracefully shuts down the server process (exit code 0).
	 *                     A supervisor using Restart=on-failure will NOT restart.
	 *   !admin restart  – gracefully shuts down the server process (exit code 1)
	 *                     so that a supervisor using Restart=on-failure will
	 *                     restart it automatically.
	 *
	 * IMPORTANT: To prevent the server from restarting after !admin stop, your
	 * process supervisor must use Restart=on-failure (systemd) or
	 * --restart on-failure (Docker), NOT Restart=always / --restart always.
	 */
	FString ServerControlCommandPrefix{ TEXT("!admin") };

	/**
	 * Discord role ID whose members are allowed to run
	 * !admin server control commands.  When empty (the default), ALL server
	 * control commands are disabled until a role ID is set here.
	 *
	 * How to get the role ID: Discord Settings -> Advanced -> Developer Mode,
	 * then right-click the role in Server Settings -> Roles -> Copy Role ID.
	 * Example: ServerControlCommandRoleId=123456789012345678
	 */
	FString ServerControlCommandRoleId;

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
	 * Parses a comma-separated channel ID string (as stored in a config field)
	 * into an array of trimmed, non-empty Discord ID strings.
	 *
	 * Example:
	 *   "123456789012345678,  987654321098765432  " →
	 *   { "123456789012345678", "987654321098765432" }
	 *
	 * A single ID (no comma) is returned as a one-element array.
	 * An empty input string returns an empty array.
	 */
	static TArray<FString> ParseChannelIds(const FString& Raw);

	/**
	 * Loads configuration from the primary mod-folder INI files, falling back to
	 * the Saved/Config backup files when credentials or feature files are missing.
	 * If any primary file does not exist it is created with default values.  On
	 * every server start, up-to-date backups of ALL settings are written to the
	 * Saved/Config location so they survive the next mod update.
	 *
	 * Primary config files (in the mod's Config folder):
	 *   DefaultDiscordBridge.ini  – connection, chat, server status, presence, server control
	 *   DefaultWhitelist.ini      – whitelist settings
	 *   DefaultBan.ini            – ban system settings
	 *   DefaultTickets.ini        – ticket system settings
	 *
	 * Backup files (in Saved/Config/, never touched by Alpakit/SMM):
	 *   DiscordBridge.ini  – backup of DefaultDiscordBridge.ini settings
	 *   Whitelist.ini      – backup of DefaultWhitelist.ini settings
	 *   Ban.ini            – backup of DefaultBan.ini settings
	 *   Tickets.ini        – backup of DefaultTickets.ini settings
	 *
	 * When credentials are absent from the primary (e.g. after a mod update
	 * resets DefaultDiscordBridge.ini) but present in the main backup, ALL main
	 * settings are restored from the backup and written back into the primary
	 * config file.
	 *
	 * When a secondary config file (whitelist/ban/ticket) is missing (e.g. after
	 * a mod update), it is recreated from its own dedicated backup file.  As an
	 * upgrade-migration fallback, the old combined DiscordBridge.ini backup is
	 * also checked so settings from pre-split installations are preserved.
	 *
	 * IMPORTANT FOR FUTURE MAINTAINERS: when adding a new field to
	 * FDiscordBridgeConfig, you must update DiscordBridgeConfig.cpp in ALL
	 * four places marked with the tag [BACKUP-SYNC]:
	 *   1. Primary config load   (Step 1 – read from the appropriate primary file)
	 *   2. Backup restore        (Step 2 – read from Saved/Config/DiscordBridge.ini)
	 *   3. Primary patch-back    (Step 2 PatchLine calls – write back to primary)
	 *   4. Backup write          (Step 3 – write to the appropriate Saved/Config/ file)
	 * Also add the key to the appropriate DefaultContent template and the
	 * secondary-file creation block so new servers get the setting automatically.
	 */
	static FDiscordBridgeConfig LoadOrCreate();

	/**
	 * Performs a lightweight check of the primary config file to determine
	 * whether BotToken and ChannelId have both been set.
	 * Does not write any backup files or perform a full config load.
	 * Used by the config polling ticker in DiscordBridgeSubsystem to detect
	 * when credentials are configured without requiring a server restart.
	 */
	static bool HasCredentials();

	/** Returns the absolute path to the primary INI config file (mod folder). */
	static FString GetModConfigFilePath();

	/** Returns the absolute path to the whitelist INI config file (mod folder). */
	static FString GetWhitelistConfigFilePath();

	/** Returns the absolute path to the ban system INI config file (mod folder). */
	static FString GetBanConfigFilePath();

	/** Returns the absolute path to the ticket system INI config file (mod folder). */
	static FString GetTicketConfigFilePath();

	/** Returns the absolute path to the main backup INI config file (Saved/Config/). */
	static FString GetBackupConfigFilePath();

	/** Returns the absolute path to the whitelist backup INI config file (Saved/Config/). */
	static FString GetWhitelistBackupFilePath();

	/** Returns the absolute path to the ban system backup INI config file (Saved/Config/). */
	static FString GetBanBackupFilePath();

	/** Returns the absolute path to the ticket system backup INI config file (Saved/Config/). */
	static FString GetTicketBackupFilePath();
};
