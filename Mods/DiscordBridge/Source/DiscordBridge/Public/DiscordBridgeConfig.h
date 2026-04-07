// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A single find-and-replace entry for the chat relay blocklist.
 */
struct DISCORDBRIDGE_API FChatRelayReplacement
{
	/** The pattern to search for (case-insensitive substring match). */
	FString Pattern;

	/** The replacement text (default: ***). */
	FString Replacement{ TEXT("***") };
};

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
 *   <ServerRoot>/FactoryGame/Saved/DiscordBridge/DiscordBridge.ini
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

	// ── Chat relay filter ─────────────────────────────────────────────────────

	/**
	 * List of blocked keywords/phrases (case-insensitive).  Any in-game chat message
	 * that contains one of these strings will be silently dropped and not relayed to
	 * Discord.  Useful for blocking spam, slurs, or other unwanted content.
	 *
	 * Add one entry per line using +ChatRelayBlocklist= syntax:
	 *   +ChatRelayBlocklist=spam
	 *   +ChatRelayBlocklist=badword
	 *
	 * Leave empty to relay all messages (default).
	 */
	TArray<FString> ChatRelayBlocklist;

	/**
	 * Find-and-replace replacements for the chat relay filter.
	 * Instead of dropping the whole message when a keyword is matched, the
	 * matching text is replaced with Replacement (default: ***).
	 *
	 * When a message matches both ChatRelayBlocklist (drop) and
	 * ChatRelayBlocklistReplacements (replace), the full-drop rule wins.
	 *
	 * Config syntax:
	 *   +ChatRelayBlocklistReplacements=(Pattern="badword",Replacement="***")
	 */
	TArray<FChatRelayReplacement> ChatRelayBlocklistReplacements;

	// ── Bot commands ──────────────────────────────────────────────────────────

	/**
	 * The prefix for the !players Discord command (default: !players).
	 * Discord users can type this in the bot channel to see the list of online players.
	 */
	FString PlayersCommandPrefix{ TEXT("!players") };

	/**
	 * Snowflake ID of the Discord channel where !players responses are posted.
	 * Leave empty to post to the main bridged channel (ChannelId).
	 */
	FString PlayersCommandChannelId;

	/**
	 * Discord invite link shown to players when they type /discord or !discord
	 * in the Satisfactory in-game chat.
	 * Leave empty to disable the /discord in-game command.
	 * Example: https://discord.gg/yourserver
	 */
	FString DiscordInviteUrl;

	// ── Player join / leave / timeout notifications ───────────────────────────

	/**
	 * Master on/off switch for player join/leave/timeout notifications.
	 * When true, DiscordBridge posts a message to Discord whenever a player
	 * joins, leaves, or times out from the server.
	 * Default: false (disabled).
	 */
	bool bPlayerEventsEnabled{ false };

	/**
	 * Snowflake ID of a dedicated Discord channel for player join/leave/timeout
	 * notifications.  Leave empty to post to the main bridged channel (ChannelId).
	 *
	 * Get the channel ID the same way as ChannelId (right-click the channel in
	 * Discord with Developer Mode enabled → Copy Channel ID).
	 *
	 * Example: PlayerEventsChannelId=111222333444555666777
	 */
	FString PlayerEventsChannelId;

	/**
	 * Message posted to Discord when a player joins the server.
	 * Available placeholder: %PlayerName%
	 * Leave empty to disable join notifications.
	 * Default: ":green_circle: **%PlayerName%** joined the server."
	 */
	FString PlayerJoinMessage{
		TEXT(":green_circle: **%PlayerName%** joined the server.")
	};

	/**
	 * Snowflake ID of a private Discord channel where sensitive join details
	 * (EOS Product User ID and IP address) are posted for admins only.
	 * Leave empty to disable admin-info notifications entirely.
	 *
	 * Example: PlayerJoinAdminChannelId=111222333444555666777
	 */
	FString PlayerJoinAdminChannelId;

	/**
	 * Message posted to the admin channel when a player joins.
	 * Available placeholders:
	 *   %PlayerName%       – in-game display name of the joining player.
	 *   %EOSProductUserId% – EOS Product User ID (32-char lowercase hex).
	 *                        Empty string when no EOS session is present.
	 *   %IpAddress%        – Remote IP address of the joining player.
	 *                        Empty string when the address cannot be determined.
	 *
	 * Only posted when PlayerJoinAdminChannelId is set.
	 * Leave empty to disable (PlayerJoinAdminChannelId is then ignored).
	 * Default: ":shield: **%PlayerName%** joined | EOS: `%EOSProductUserId%` | IP: `%IpAddress%`"
	 */
	FString PlayerJoinAdminMessage{
		TEXT(":shield: **%PlayerName%** joined | EOS: `%EOSProductUserId%` | IP: `%IpAddress%`")
	};

	/**
	 * Message posted to Discord when a player leaves the server cleanly.
	 * Also used as a fallback when PlayerTimeoutMessage is empty and a timeout
	 * is detected.
	 * Available placeholder: %PlayerName%
	 * Leave empty to disable leave notifications.
	 * Default: ":red_circle: **%PlayerName%** left the server."
	 */
	FString PlayerLeaveMessage{
		TEXT(":red_circle: **%PlayerName%** left the server.")
	};

	/**
	 * Message posted to Discord when a player times out (connection lost without
	 * a clean disconnect).  Leave empty to use PlayerLeaveMessage for timeouts
	 * instead (or to disable timeout-specific notifications when PlayerLeaveMessage
	 * is also empty).
	 * Available placeholder: %PlayerName%
	 * Default: ":yellow_circle: **%PlayerName%** timed out."
	 */
	FString PlayerTimeoutMessage{
		TEXT(":yellow_circle: **%PlayerName%** timed out.")
	};

	/**
	 * When true, player join/leave/timeout events are posted as rich Discord embeds
	 * (colour-coded with player info fields) instead of plain text messages.
	 * Join = green (3066993), Leave = red (15158332), Timeout = orange (16776960).
	 * Default: false (plain text).
	 */
	bool bUseEmbedsForPlayerEvents{ false };

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

	// ── Per-event channel routing ─────────────────────────────────────────────

	/**
	 * Snowflake ID of the Discord channel where game phase change announcements
	 * are posted.  Leave empty to post to the main bridged channel (ChannelId).
	 */
	FString PhaseEventsChannelId;

	/**
	 * Snowflake ID of the Discord channel where schematic/research unlock
	 * announcements are posted.  Leave empty to post to PhaseEventsChannelId,
	 * or to ChannelId when that is also empty.
	 */
	FString SchematicEventsChannelId;

	/**
	 * Snowflake ID of the Discord channel where ban notifications are posted.
	 * Leave empty to post to the main DiscordWebhookUrl used by BanSystem.
	 */
	FString BanEventsChannelId;

	// ── !stats / !playerstats commands ───────────────────────────────────────

	/**
	 * Prefix for the !stats Discord command (default: !stats).
	 * Discord users can type this to get a rich embed with server statistics.
	 * Leave empty to disable the command.
	 */
	FString StatsCommandPrefix{ TEXT("!stats") };

	/**
	 * Prefix for the !playerstats Discord command (default: !playerstats).
	 * Usage: !playerstats <in-game player name>
	 * Leave empty to disable the command.
	 */
	FString PlayerStatsCommandPrefix{ TEXT("!playerstats") };

	// ── Join reaction voting ──────────────────────────────────────────────────

	/**
	 * When true, DiscordBridge adds 👍 and 👎 reactions to the player-join
	 * embed/message so that server members can up/down vote new arrivals.
	 * Requires the bot to have "Add Reactions" and "Read Message History"
	 * permissions in the player-events channel.
	 * Default: false.
	 */
	bool bEnableJoinReactionVoting{ false };

	/**
	 * Number of 👎 reactions required within VoteWindowMinutes to kick
	 * the newly-joined player.  Set to 0 to disable automatic kick.
	 * Default: 0 (vote tracking only, no auto-kick).
	 */
	int32 VoteKickThreshold{ 0 };

	/**
	 * Number of minutes to watch for 👎 reactions before the vote expires.
	 * Default: 5.
	 */
	int32 VoteWindowMinutes{ 5 };

	// ── AFK kick ─────────────────────────────────────────────────────────────

	/**
	 * Number of minutes of inactivity before kicking an AFK player.
	 * "Inactivity" is defined as not building anything and (optionally) not
	 * moving, tracked via FGStatisticsSubsystem's building-built counter.
	 * Set to 0 to disable AFK kick entirely (default).
	 */
	int32 AfkKickMinutes{ 0 };

	/**
	 * Reason shown to the player when they are kicked for inactivity.
	 * Default: "Kicked for inactivity (AFK)."
	 */
	FString AfkKickReason{ TEXT("Kicked for inactivity (AFK).") };

	// ── Scheduled announcements ───────────────────────────────────────────────

	/**
	 * A single scheduled announcement entry.
	 * ScheduledAnnouncements is an array of these structs.
	 */

	/**
	 * Interval in minutes between repeating server announcements.
	 * When non-zero, DiscordBridge posts AnnouncementMessage to
	 * AnnouncementChannelId (falls back to ChannelId) at the configured interval.
	 * Set to 0 to disable scheduled announcements.
	 * Default: 0.
	 */
	int32 AnnouncementIntervalMinutes{ 0 };

	/**
	 * The message text to post as a periodic server announcement.
	 * HTML-style Discord markdown is supported (bold, italic, URLs, etc.).
	 * Only used when AnnouncementIntervalMinutes > 0.
	 */
	FString AnnouncementMessage{ TEXT("") };

	/**
	 * Channel ID to post scheduled announcements to.
	 * Falls back to ChannelId when empty.
	 */
	FString AnnouncementChannelId{ TEXT("") };

	// ── Embed mode flags ─────────────────────────────────────────────────────

	/**
	 * When true, game-phase change notifications are sent as rich Discord embeds
	 * (colour-coded, with title and description).
	 * When false, they are sent as plain text messages.
	 * Only has an effect when PhaseEventsChannelId is set.
	 * Default: true.
	 */
	bool bUseEmbedsForPhaseEvents{ true };

	/**
	 * When true, schematic-unlock notifications are sent as rich Discord embeds.
	 * When false, they are sent as plain text messages.
	 * Only has an effect when SchematicEventsChannelId is set.
	 * Default: true.
	 */
	bool bUseEmbedsForSchematicEvents{ true };

	// ── Webhook fallback ─────────────────────────────────────────────────────

	/**
	 * Secondary (fallback) Discord webhook URL.
	 * When non-empty and the primary bot send fails, DiscordBridge retries the
	 * message to this webhook URL.  Useful for posting to a second channel or
	 * alerting a backup logging server on primary failure.
	 * Leave empty to disable (default).
	 */
	FString FallbackWebhookUrl{ TEXT("") };

	// ── Slash commands ────────────────────────────────────────────────────────

	/**
	 * When true, DiscordBridge registers its built-in Discord application
	 * slash commands (/players, /stats, /server) with the Discord API on startup.
	 * Requires the bot to have "applications.commands" scope.
	 * Default: false.
	 */
	bool bEnableSlashCommands{ false };

	// ── Mute notifications ────────────────────────────────────────────────────

	/**
	 * When true, DiscordBridge posts a notification to the moderator channel
	 * (ModeratorChannelId, falling back to ChannelId) whenever a player is
	 * muted or unmuted via /mute or /unmute.
	 * Only has an effect when BanChatCommands mod is installed.
	 * Default: false.
	 */
	bool bNotifyMuteEvents{ false };

	/**
	 * Channel ID to post mute/unmute notifications to.
	 * Falls back to BanEventsChannelId, then ChannelId, when empty.
	 */
	FString ModeratorChannelId{ TEXT("") };

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
	 *   DiscordBridge.ini  – all settings (connection, chat, presence, whitelist)
	 *
	 * IMPORTANT FOR FUTURE MAINTAINERS: when adding a new field to
	 * FDiscordBridgeConfig, you must update DiscordBridgeConfig.cpp in ALL
	 * four places marked with the tag [BACKUP-SYNC]:
	 *   1. Primary config load   (Step 1 – read from DefaultDiscordBridge.ini)
	 *   2. Backup restore        (Step 2 – read from Saved/DiscordBridge/DiscordBridge.ini)
	 *   3. Primary patch-back    (Step 2 PatchLine calls – write back to primary)
	 *   4. Backup write          (Step 3 – write to Saved/DiscordBridge/DiscordBridge.ini)
	 * Also add the key to the DefaultContent template and the second-pass upgrade
	 * check so existing server configs pick up the new setting automatically.
	 * For TArray fields use ParseRawIniArray (primary load) / ParseRawIniArray on
	 * the already-loaded backup string (restore), PatchArrayLines (patch-back), and
	 * per-item +Key=value lines (backup write).
	 */
	static FDiscordBridgeConfig LoadOrCreate();

	/** Returns the absolute path to the primary INI config file (mod folder). */
	static FString GetModConfigFilePath();

	/** Returns the absolute path to the backup INI config file (Saved/Config/). */
	static FString GetBackupConfigFilePath();
};
