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

	// ── Custom ticket reasons ─────────────────────────────────────────────────

	/**
	 * Zero or more custom ticket reason buttons.
	 * Each entry must be formatted as "Label|Description".
	 * Example: "Bug Report|Report a bug or technical issue with the server"
	 */
	TArray<FString> CustomTicketReasons;

	// ── Static helpers ────────────────────────────────────────────────────────

	/** Load settings from DefaultTickets.ini (+ backup) and return a populated config. */
	static FTicketConfig Load();

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
