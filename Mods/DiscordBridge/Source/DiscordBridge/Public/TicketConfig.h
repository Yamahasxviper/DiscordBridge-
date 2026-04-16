// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTicketSystem, Log, All);

/**
 * FTicketConfig
 *
 * Plain-old-data struct that holds all settings loaded from
 * DefaultTickets.ini (and the Saved/TicketSystem/TicketSystem.ini backup).
 * Populated once per server start by FTicketConfig::Load().
 */
struct DISCORDBRIDGE_API FTicketConfig
{
	// ── Standalone Discord connectivity ────────────────────────────────────────

	/**
	 * Discord bot token used when running TicketSystem without DiscordBridge.
	 * Create a bot at https://discord.com/developers/applications, copy its
	 * token, and paste it here.  Leave empty when DiscordBridge is installed
	 * (DiscordBridge will inject itself as the provider instead).
	 *
	 * Required bot permissions:
	 *   Manage Channels – create and delete ticket channels
	 *   View Channel    – read channels
	 *   Send Messages   – post welcome and close-button messages
	 *
	 * Required Gateway intents (Discord Developer Portal → Bot → Privileged):
	 *   Server Members Intent  (needed for role info in MESSAGE_CREATE)
	 *   Message Content Intent (needed to read the !ticket-panel command)
	 */
	FString BotToken;

	/**
	 * Discord guild (server) ID where the bot operates.
	 * Leave empty to auto-detect from the READY event (recommended).
	 * Set this only if the bot does not receive a READY payload with guild info,
	 * which is rare and usually indicates a misconfigured intent.
	 *
	 * How to get the guild ID: enable Developer Mode in Discord
	 * (User Settings → Advanced → Developer Mode), right-click your server
	 * icon, and choose "Copy Server ID".
	 */
	FString GuildId;

	// ── Ticket channel ─────────────────────────────────────────────────────────

	/**
	 * Discord channel ID(s) where a notification is posted for each new ticket.
	 * Leave empty to skip the admin notification message.
	 * Comma-separated list supported: "id1,id2".
	 */
	FString TicketChannelId;

	// ── Ticket type toggles ───────────────────────────────────────────────────

	/** When true the Whitelist Request button appears on the ticket panel. */
	bool bTicketWhitelistEnabled{ true };

	/** When true the Help / Support button appears on the ticket panel. */
	bool bTicketHelpEnabled{ true };

	/** When true the Report a Player button appears on the ticket panel. */
	bool bTicketReportEnabled{ true };

	/** When true the Ban Appeal button appears on the ticket panel. */
	bool bTicketBanAppealEnabled{ true };

	/** When true the Mute Appeal button appears on the ticket panel. */
	bool bTicketMuteAppealEnabled{ true };

	// ── Admin notifications ───────────────────────────────────────────────────

	/**
	 * Discord role ID to @mention in every ticket notification and to grant
	 * view/write access inside every new ticket channel.
	 * Also used to authorise the "!ticket-panel" management command.
	 */
	FString TicketNotifyRoleId;

	// ── Button-based ticket panel ─────────────────────────────────────────────

	/**
	 * Discord channel ID where the bot posts the ticket selection panel.
	 * Run "!ticket-panel" (holding TicketNotifyRoleId) to post the panel.
	 * Leave empty to skip the panel feature.
	 */
	FString TicketPanelChannelId;

	/**
	 * Discord category ID under which new ticket channels are created.
	 * Leave empty to create channels at the server root.
	 */
	FString TicketCategoryId;

	// ── Ticket log / transcript channel ──────────────────────────────────────

	/**
	 * Snowflake ID of a Discord channel where a transcript is posted whenever
	 * a ticket channel is closed.  The transcript lists the ticket type, the
	 * opener's username, the date opened, and every message exchanged in the
	 * ticket channel.
	 * Leave empty (default) to disable transcript archiving.
	 *
	 * Example: TicketLogChannelId=111222333444555666
	 */
	FString TicketLogChannelId;

	// ── Custom ticket reasons ─────────────────────────────────────────────────

	/**
	 * Zero or more custom ticket reason buttons.
	 * Each entry must be formatted as "Label|Description".
	 * Example: "Bug Report|Report a bug or technical issue with the server"
	 */
	TArray<FString> CustomTicketReasons;

	// ── Inactive-ticket auto-close ────────────────────────────────────────────

	/**
	 * Number of hours of inactivity after which an open ticket channel is
	 * automatically closed (transcript archived if TicketLogChannelId is set,
	 * then the channel is deleted).
	 *
	 * Set to 0 (default) to disable automatic closure.
	 *
	 * Example: InactiveTicketTimeoutHours=24   ; close tickets with no messages for 24 hours
	 */
	float InactiveTicketTimeoutHours{ 0.0f };

	// ── Per-type category IDs ─────────────────────────────────────────────────

	/** Discord category ID for whitelist ticket channels. Falls back to TicketCategoryId when empty. */
	FString WhitelistCategoryId;
	/** Discord category ID for help ticket channels. Falls back to TicketCategoryId when empty. */
	FString HelpCategoryId;
	/** Discord category ID for report ticket channels. Falls back to TicketCategoryId when empty. */
	FString ReportCategoryId;
	/** Discord category ID for ban appeal ticket channels. Falls back to TicketCategoryId when empty. */
	FString AppealCategoryId;

	/** Discord category ID for mute appeal ticket channels. Falls back to TicketCategoryId when empty. */
	FString MuteAppealCategoryId;

	// ── Feedback/rating ───────────────────────────────────────────────────────

	/** When true, sends a DM star-rating request to the opener when a ticket is closed. */
	bool bTicketFeedbackEnabled{ false };

	// ── Macros / canned responses ─────────────────────────────────────────────

	/** List of canned response macros. Format: "name|response text". */
	TArray<FString> TicketMacros;

	// ── Cooldown ──────────────────────────────────────────────────────────────

	/** Minutes a user must wait after closing a ticket before opening another. 0 = disabled. */
	int32 TicketCooldownMinutes{ 0 };

	// ── Reopen grace period ───────────────────────────────────────────────────

	/** Minutes after close during which the opener can reopen the ticket. 0 = disabled (immediate delete). */
	int32 TicketReopenGracePeriodMinutes{ 0 };

	// ── Multi-ticket support ──────────────────────────────────────────────────

	/** When true, users may have one ticket open per type rather than one total. */
	bool bAllowMultipleTicketTypes{ false };

	// ── Auto-refresh panel ────────────────────────────────────────────────────

	/** When true, delete and re-post the ticket panel on server startup. */
	bool bAutoRefreshPanel{ false };

	// ── DM on staff reply ─────────────────────────────────────────────────────

	/** When true, DM the ticket opener when staff sends a message in their ticket channel. */
	bool bDmOpenerOnStaffReply{ false };

	// ── SLA / Response-time tracking ──────────────────────────────────────────

	/**
	 * Post a warning to TicketChannelId if no staff has replied in a ticket
	 * within this many minutes.  0 = disabled.
	 */
	int32 TicketSlaWarningMinutes{ 0 };

	// ── Ticket escalation ─────────────────────────────────────────────────────

	/** Discord role ID to @mention when a ticket is escalated via !ticket-escalate. */
	FString TicketEscalationRoleId;

	/** Discord category ID to move escalated ticket channels to.  Leave empty to skip. */
	FString TicketEscalationCategoryId;

	// ── Multi-field ticket templates ──────────────────────────────────────────

	/**
	 * Multi-field ticket templates.  Format: "TypeSlug|Field1Label|Field2Label|...".
	 * Example: "report|Player Name|What happened|Evidence link"
	 * Config key: TicketTemplate= (one per line).
	 */
	TArray<FString> TicketTemplates;

	// ── Per-type auto-responses ───────────────────────────────────────────────

	/**
	 * Bot message posted automatically to a new ticket channel once created.
	 * Format: "TypeSlug|Auto-response message text".
	 * Example: "whitelist|Please provide your in-game name and how you found the server."
	 * Config key: TicketAutoResponse= (one per line).
	 */
	TArray<FString> TicketAutoResponses;

	// ── Ban appeal cooldown ───────────────────────────────────────────────────

	/**
	 * Days a player must wait after a denied ban appeal before submitting another.
	 * Set to 0 (default) to disable the cooldown.
	 * Example: BanAppealCooldownDays=7  ; player must wait 7 days after denial
	 */
	int32 BanAppealCooldownDays{ 0 };

	// ── Repeat-offender appeal guard ──────────────────────────────────────────

	/**
	 * Maximum number of lifetime denied ban appeals a player may submit.
	 * Once a player has had this many appeals denied, further appeal tickets are
	 * blocked.  Set to 0 (default) to disable the limit.
	 * Example: MaxLifetimeAppeals=3
	 */
	int32 MaxLifetimeAppeals{ 0 };

	// ── Static helpers ────────────────────────────────────────────────────────

	/** Load settings from DefaultTickets.ini (+ backup) and return a populated config. */
	static FTicketConfig Load();

	/**
	 * Creates DefaultTickets.ini with an annotated template if the file is absent
	 * or comment-free (Alpakit-stripped).  Call once at subsystem start before Load().
	 */
	static void RestoreDefaultConfigIfNeeded();

	/** Returns the absolute path to the primary config file (DefaultTickets.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/TicketSystem/TicketSystem.ini). */
	static FString GetBackupFilePath();

	/**
	 * Split a comma-separated channel ID string into individual channel IDs.
	 * Leading/trailing whitespace around each ID is trimmed.
	 */
	static TArray<FString> ParseChannelIds(const FString& CommaList);
};
