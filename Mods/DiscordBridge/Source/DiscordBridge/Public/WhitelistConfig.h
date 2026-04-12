// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogWhitelistConfig, Log, All);

/**
 * FWhitelistConfig
 *
 * Plain-old-data struct that holds every whitelist-related setting.
 * Loaded once per server start from DefaultWhitelist.ini
 * (with a backup in Saved/DiscordBridge/Whitelist.ini).
 *
 * PRIMARY config (edit this one):
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultWhitelist.ini
 *
 * BACKUP config (auto-managed, extra safety net):
 *   <ServerRoot>/FactoryGame/Saved/DiscordBridge/Whitelist.ini
 * Written automatically on every server start.  If the primary file is ever
 * missing or has no [Whitelist] section (e.g. after a manual deletion or a
 * direct Alpakit dev-mode deploy), the mod falls back to this backup.
 */
struct DISCORDBRIDGE_API FWhitelistConfig
{
	// ── Core ──────────────────────────────────────────────────────────────────

	/**
	 * When true, the whitelist is enabled on every server start, overriding any
	 * runtime change made via /whitelist on / /whitelist off Discord commands.
	 * Default: false (all players can join).
	 */
	bool bWhitelistEnabled{ false };

	/**
	 * Snowflake ID of the Discord role whose members are allowed to run
	 * /whitelist management commands.
	 * Leave empty (or unset) to disable /whitelist commands entirely – no
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
	 * Snowflake ID of the Discord role used to identify whitelisted members.
	 * Leave empty to disable Discord role integration.
	 *
	 * When set:
	 *  • Messages received on WhitelistChannelId are relayed to the game only
	 *    when the sender holds this role.
	 *  • The `/whitelist role add/remove <user_id>` commands assign or revoke
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

	// ── Kick messages ────────────────────────────────────────────────────────

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
	 * Default: "You are not on the server whitelist. Contact the server admin to be added."
	 */
	FString WhitelistKickReason{
		TEXT("You are not on the server whitelist. Contact the server admin to be added.")
	};

	// ── Events channel ───────────────────────────────────────────────────────

	/**
	 * Snowflake ID of a dedicated Discord channel where whitelist events (add/remove/enable/disable)
	 * are posted as embeds. Leave empty to disable whitelist event notifications.
	 */
	FString WhitelistEventsChannelId;

	// ── Slot limit ───────────────────────────────────────────────────────────

	/**
	 * Maximum number of whitelist slots. 0 = unlimited.
	 * When set, the /whitelist add command will refuse to add more players than this limit.
	 */
	int32 MaxWhitelistSlots{ 0 };

	// ── Role sync ────────────────────────────────────────────────────────────

	/**
	 * When true, DiscordBridge automatically syncs the in-game whitelist with the WhitelistRoleId
	 * Discord role. Members who receive the role are auto-added; members who lose it are auto-removed.
	 */
	bool bSyncWhitelistWithRole{ false };

	// ── Application system ───────────────────────────────────────────────────

	/**
	 * Snowflake ID of the Discord channel where `/whitelist apply` application embeds are posted.
	 * Leave empty to disable the application flow entirely.
	 */
	FString WhitelistApplicationChannelId;

	/**
	 * When true, any Discord user may run `/whitelist apply <InGameName>` to submit a
	 * whitelist application, which is posted as an embed with Approve/Deny buttons to
	 * WhitelistApplicationChannelId.
	 * Default: false.
	 */
	bool bWhitelistApplyEnabled{ false };

	/**
	 * DM message sent to the Discord user whose whitelist application is approved (via
	 * the Approve button or `wl_approve` interaction).  Leave empty to disable DMs.
	 *
	 * Placeholder: %PlayerName% — the in-game name that was approved.
	 *
	 * Example: WhitelistApprovedDmMessage=✅ Your whitelist application for **%PlayerName%** was approved!
	 */
	FString WhitelistApprovedDmMessage;

	// ── Expiry warnings ──────────────────────────────────────────────────────

	/**
	 * How many hours before a timed whitelist entry expires to post a warning to
	 * WhitelistEventsChannelId (falls back to ChannelId).
	 * Set to 0 to disable expiry warnings.
	 * Default: 24.0 (warn 24 hours before expiry).
	 */
	float WhitelistExpiryWarningHours{ 24.0f };

	// ── Verification / account linking ───────────────────────────────────────

	/**
	 * When true, enables the `/whitelist link <InGameName>` Discord command that lets
	 * Discord users link their in-game account via a one-time 6-digit verification code
	 * typed in-game with `/verify <code>`.
	 * Default: false.
	 */
	bool bWhitelistVerificationEnabled{ false };

	/**
	 * Snowflake ID of the Discord channel where `/whitelist link` commands are accepted.
	 * Leave empty to allow the command in any channel.
	 */
	FString WhitelistVerificationChannelId;

	// ── Static helpers ───────────────────────────────────────────────────────

	/** Load settings from DefaultWhitelist.ini (+ backup) and return a populated config. */
	static FWhitelistConfig Load();

	/**
	 * Creates DefaultWhitelist.ini with an annotated template if the file is absent
	 * or comment-free (Alpakit-stripped).  Call once at subsystem start before Load().
	 */
	static void RestoreDefaultConfigIfNeeded();

	/** Returns the absolute path to the primary config file (DefaultWhitelist.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/DiscordBridge/Whitelist.ini). */
	static FString GetBackupFilePath();
};
