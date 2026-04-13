// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Logging/LogMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogInGameMessagesConfig, Log, All);

/**
 * A single in-game broadcast message entry.
 *
 * Config example (DefaultInGameMessages.ini):
 *   +Messages=(IntervalMinutes=10,SenderName="[Server]",Message="Welcome! Join our Discord: https://discord.gg/example")
 */
struct DISCORDBRIDGE_API FInGameBroadcastMessage
{
	/** How often in minutes to broadcast this message to in-game chat. 0 = disabled. */
	int32 IntervalMinutes = 0;

	/** The sender name shown in the in-game chat widget (e.g. "[Server]", "[Admin]"). */
	FString SenderName{ TEXT("[Server]") };

	/**
	 * The message text to broadcast in-game.
	 * Supports web links (https://...), server links (discord.gg/...),
	 * and Unicode emoji characters.
	 */
	FString Message;
};

/**
 * FInGameMessagesConfig
 *
 * Plain-old-data struct that holds all in-game broadcast message settings.
 * Loaded once per server start from DefaultInGameMessages.ini
 * (with a backup in Saved/DiscordBridge/InGameMessages.ini).
 *
 * PRIMARY config (edit this one):
 *   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultInGameMessages.ini
 *
 * BACKUP config (auto-managed, extra safety net):
 *   <ServerRoot>/FactoryGame/Saved/DiscordBridge/InGameMessages.ini
 * Written automatically on every server start.  If the primary file is ever
 * missing or has no [InGameMessages] section (e.g. after a manual deletion or a
 * direct Alpakit dev-mode deploy), the mod falls back to this backup.
 */
struct DISCORDBRIDGE_API FInGameMessagesConfig
{
	// ── Global toggle ────────────────────────────────────────────────────────

	/**
	 * Master switch for the in-game broadcast feature.
	 * When false, no scheduled messages are sent to in-game chat regardless
	 * of individual message entries.
	 * Default: true.
	 */
	bool bEnabled{ true };

	/**
	 * Default sender name used when a message entry does not specify its own.
	 * Default: "[Server]".
	 */
	FString DefaultSenderName{ TEXT("[Server]") };

	// ── Message entries ──────────────────────────────────────────────────────

	/**
	 * Array of scheduled in-game broadcast messages.
	 * Each entry is independent with its own interval, sender name, and text.
	 *
	 * Config syntax (DefaultInGameMessages.ini):
	 *   +Messages=(IntervalMinutes=10,SenderName="[Server]",Message="Welcome to our server!")
	 *   +Messages=(IntervalMinutes=30,SenderName="[Admin]",Message="Visit https://example.com for rules")
	 */
	TArray<FInGameBroadcastMessage> Messages;

	// ── Static helpers ───────────────────────────────────────────────────────

	/** Load settings from DefaultInGameMessages.ini (+ backup) and return a populated config. */
	static FInGameMessagesConfig Load();

	/**
	 * Creates DefaultInGameMessages.ini with an annotated template if the file is absent
	 * or comment-free (Alpakit-stripped).  Call once at subsystem start before Load().
	 */
	static void RestoreDefaultConfigIfNeeded();

	/** Returns the absolute path to the primary config file (DefaultInGameMessages.ini). */
	static FString GetConfigFilePath();

	/** Returns the absolute path to the backup config file (Saved/DiscordBridge/InGameMessages.ini). */
	static FString GetBackupFilePath();
};
