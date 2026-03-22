// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTicketSystem, Log, All);

/**
 * FTicketConfig
 *
 * Plain-old-data struct that holds all settings loaded from
 * DefaultTickets.ini (and the Saved/Config/TicketSystem.ini backup).
 * Populated once per server start by FTicketConfig::Load().
 */
struct TICKETSYSTEM_API FTicketConfig
{
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

	/** Returns the absolute path to the backup config file (Saved/Config/TicketSystem.ini). */
	static FString GetBackupFilePath();

	/**
	 * Split a comma-separated channel ID string into individual channel IDs.
	 * Leading/trailing whitespace around each ID is trimmed.
	 */
	static TArray<FString> ParseChannelIds(const FString& CommaList);
};
