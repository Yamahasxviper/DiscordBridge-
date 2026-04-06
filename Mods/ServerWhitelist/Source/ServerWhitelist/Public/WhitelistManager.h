// Copyright Yamahasxviper. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

/**
 * Manages the server whitelist JSON file.
 *
 * Data is stored at <ProjectSavedDir>/ServerWhitelist.json.
 * The file is created with defaults on first use and written to disk
 * immediately on every change so changes survive server restarts.
 *
 * This file is intentionally compatible with DiscordBridge's WhitelistManager –
 * both mods read/write the same ServerWhitelist.json so the player list stays
 * in sync when both mods are installed.
 *
 * Example file:
 *   {
 *     "enabled": false,
 *     "players": ["alice", "bob"]
 *   }
 */
class SERVERWHITELIST_API FWhitelistManager
{
public:
	/**
	 * Load (or create) the whitelist file from disk.
	 * Call once at startup after the INI config has been loaded.
	 *
	 * @param bDefaultEnabled  Initial enabled state, taken from the INI config.
	 *                         Applied only when no JSON file exists yet.
	 *                         After the first start, the saved JSON state is used.
	 * @param InitialPlayers   Optional pre-populated player list applied only when
	 *                         no JSON file exists yet (first server start).
	 */
	static void Load(bool bDefaultEnabled = false,
	                 const TArray<FString>& InitialPlayers = TArray<FString>());

	/** Persist the current state to disk immediately. */
	static void Save();

	/** Returns true if the whitelist is currently active. */
	static bool IsEnabled();

	/** Enable or disable the whitelist and save. */
	static void SetEnabled(bool bEnabled);

	/**
	 * Returns true if the given player name is on the whitelist.
	 * Comparison is case-insensitive.
	 */
	static bool IsWhitelisted(const FString& PlayerName);

	/** Adds a player and saves. Returns false if already listed. */
	static bool AddPlayer(const FString& PlayerName);

	/** Removes a player and saves. Returns false if not found. */
	static bool RemovePlayer(const FString& PlayerName);

	/** Returns a copy of the current whitelist. */
	static TArray<FString> GetAll();

private:
	static FString GetFilePath();

	static bool           bEnabled;
	static TArray<FString> Players; // stored lower-case for comparison
};
