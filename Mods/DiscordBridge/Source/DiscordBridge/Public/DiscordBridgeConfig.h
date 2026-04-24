// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"

/**
 * A single scheduled announcement entry for the ScheduledAnnouncements array.
 * Each entry fires independently on its own interval.
 *
 * Config example (DefaultDiscordBridge.ini):
 *   +ScheduledAnnouncements=(IntervalMinutes=30,Message="Join our Discord!",ChannelId="")
 */
struct DISCORDBRIDGE_API FScheduledAnnouncement
{
	/** How often in minutes to post this announcement. 0 = disabled. */
	int32   IntervalMinutes = 0;

	/** The message text to post. */
	FString Message;

	/** Channel ID to post to. Falls back to ChannelId when empty. */
	FString ChannelId;
};

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
 * start if it does not exist.  Connection, chat, presence, and player-event
 * settings are in this file.  Whitelist settings have their own file:
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultWhitelist.ini
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
	 *    %Role%       – the highest-priority role label resolved from DiscordRoleLabels
	 *                   (empty string when the sender has no matching role)
	 *  Default: "[Discord] %Username%: %Message%" */
	FString DiscordToGameFormat{ TEXT("[Discord] %Username%: %Message%") };

	/**
	 * Role ID → label mappings used to resolve the %Role% placeholder in
	 * DiscordToGameFormat.  Each entry must be formatted as "roleId=Label".
	 * The first matching entry (in list order) wins; if the sender holds none of
	 * the listed roles the placeholder resolves to an empty string.
	 *
	 * Example entries in DefaultDiscordBridge.ini:
	 *   +DiscordRoleLabels=123456789012345678=Admin
	 *   +DiscordRoleLabels=987654321098765432=Moderator
	 */
	TArray<FString> DiscordRoleLabels;

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

	// ── In-game commands ──────────────────────────────────────────────────────
	// In-game chat commands are now registered as SML slash commands:
	//   /verify <code>              — link Discord account (account verification)
	//   /discord                    — show the Discord invite link
	//   /ingamewhitelist <subcmd>   — manage the whitelist from in-game chat

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
	 * Snowflake ID of the Discord channel where /players responses are posted.
	 * Leave empty to post to the main bridged channel (ChannelId).
	 */
	FString PlayersCommandChannelId;

	/**
	 * Discord invite link shown to players when they type /discord in the
	 * Satisfactory in-game chat.
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

	// ── /stats / /playerstats commands ───────────────────────────────────────

	// Server statistics and per-player stats are now available as Discord slash
	// commands (/stats and /playerstats) registered automatically by DiscordBridge.

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

	/**
	 * Array of additional scheduled announcements, each with an independent
	 * interval, message, and channel. These run in parallel with the legacy
	 * AnnouncementIntervalMinutes / AnnouncementMessage / AnnouncementChannelId
	 * fields (those still work for a single announcement).
	 *
	 * Config syntax (DefaultDiscordBridge.ini):
	 *   +ScheduledAnnouncements=(IntervalMinutes=60,Message="Rules: ...",ChannelId="")
	 *   +ScheduledAnnouncements=(IntervalMinutes=30,Message="Vote on maps at ...",ChannelId="111222333")
	 */
	TArray<FScheduledAnnouncement> ScheduledAnnouncements;

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
	 * slash commands (/players, /stats, /server, etc.) with the Discord API on startup.
	 * Requires the bot to have "applications.commands" scope.
	 * Default: true.
	 */
	bool bEnableSlashCommands{ true };

	// ── Mute notifications ────────────────────────────────────────────────────

	/**
	 * When true, DiscordBridge posts a notification to the moderator channel
	 * (ModeratorChannelId, falling back to ChannelId) whenever a player is
	 * muted or unmuted via /mute or /unmute.
	 * Only has an effect when BanChatCommands mod is installed.
	 * Default: false.
	 */
	bool bNotifyMuteEvents{ true };

	/**
	 * Channel ID to post mute/unmute notifications to.
	 * Falls back to BanEventsChannelId, then ChannelId, when empty.
	 */
	FString ModeratorChannelId{ TEXT("") };

	// ── Moderation log ────────────────────────────────────────────────────────

	/**
	 * Snowflake ID of a Discord channel where all moderation actions (bans, kicks,
	 * warns, mutes, unbans, unmutes) are mirrored regardless of source.
	 * Leave empty to disable the unified moderation log.
	 * Example: ModerationLogChannelId=111222333444555666
	 */
	FString ModerationLogChannelId{ TEXT("") };

	// ── Bot info / help channel ────────────────────────────────────────────────

	/**
	 * Snowflake ID of a dedicated Discord channel where the bot posts a
	 * comprehensive embed listing every feature and command when the server
	 * starts for the first time.  Discord users can also use /help in the
	 * main bridged channel to re-post the embed at any time.
	 *
	 * Recommended setup:
	 *   1. Create a read-only channel in your Discord server named e.g. #bot-commands.
	 *   2. Copy its channel ID (Developer Mode → right-click → Copy Channel ID).
	 *   3. Paste it here and restart the server.
	 *
	 * Leave empty to disable the automatic info post (you can still use /help
	 * in the main channel to get a response there).
	 * Example: BotInfoChannelId=111222333444555666777
	 */
	FString BotInfoChannelId{ TEXT("") };

	/**
	 * DM message sent to a player the first time they join the server.
	 * The bot looks up the player's Discord user ID from the whitelist role
	 * member cache (players whose in-game name matches a Discord display name
	 * of a member holding WhitelistRoleId).  If no matching Discord user is
	 * found the DM is silently skipped.
	 * Leave empty (default) to disable on-join DMs.
	 *
	 * Placeholder: %PlayerName% – the in-game name of the joining player.
	 *
	 * Example: WelcomeMessageDM=Welcome to the server, %PlayerName%! 🎉
	 */
	FString WelcomeMessageDM;

	/**
	 * In-game chat message sent privately to each player when they successfully
	 * join the server (after whitelist check passes).  Use this to advertise the
	 * available in-game commands such as /discord and /verify.
	 *
	 * The message is sent only to the joining player (not broadcast to all).
	 * If the SML Remote Call Object is not yet available for the player the
	 * message is silently skipped rather than broadcast to everyone.
	 *
	 * Leave empty (default) to disable the on-join hint.
	 *
	 * Placeholder: %PlayerName% – the in-game name of the joining player.
	 *
	 * Example:
	 *   JoinHintMessage=Welcome %PlayerName%! Type /discord for our Discord link. Type /commands to see available commands.
	 */
	FString JoinHintMessage;

	/**
	 * Master on/off switch for the in-game join broadcast.
	 * Set to False to suppress the broadcast entirely without having to clear
	 * the InGameJoinBroadcast message text.
	 * Defaults to True (enabled).
	 *
	 * Example: InGameJoinBroadcastEnabled=False
	 */
	bool bInGameJoinBroadcastEnabled{ true };

	/**
	 * In-game chat message broadcast to ALL connected players when a new player
	 * joins the server.  Unlike JoinHintMessage (which is a private hint only
	 * the joining player sees), this message appears in the shared game chat so
	 * everyone can see who just joined.
	 *
	 * This field is completely independent of the whitelist — it fires for every
	 * player that passes the join check regardless of whether the whitelist is
	 * enabled or disabled.  Sent via AFGChatManager::BroadcastChatMessage so it
	 * does not require the SML Remote Call Object to be available.
	 *
	 * Only used when bInGameJoinBroadcastEnabled is True.
	 * Leave empty (default) to disable the broadcast.
	 *
	 * Placeholder: %PlayerName% – the in-game name of the joining player.
	 *
	 * Example:
	 *   InGameJoinBroadcast=🎮 Welcome to the server, %PlayerName%!
	 */
	FString InGameJoinBroadcast;

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
	 *   DiscordBridge.ini  – all settings (connection, chat, presence)
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
