#pragma once
#include "CoreMinimal.h"

/**
 * Manages the server whitelist for the DiscordBridge mod.
 *
 * Config is stored at <ProjectSavedDir>/ServerWhitelist.json.
 * The file is created with defaults on first use and written to disk
 * immediately on every change so changes survive server restarts.
 *
 * No additional dependency beyond Core/Json — uses UE4 Json + FFileHelper.
 *
 * Example file:
 *   {
 *     "enabled": false,
 *     "players": ["Alice", "Bob"]
 *   }
 */
class DISCORDBRIDGE_API FWhitelistManager
{
public:
	/**
	 * Load (or create) the whitelist file from disk. Call once at startup,
	 * after the INI config has been loaded.
	 *
	 * @param bDefaultEnabled  The WhitelistEnabled value from the INI config.
	 *                         Applied on every startup — the player list is read
	 *                         from the JSON file, but the enabled/disabled state
	 *                         always comes from this parameter so operators can
	 *                         toggle the whitelist by editing the config and
	 *                         restarting the server.
	 */
	static void Load(bool bDefaultEnabled = false);

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

	/**
	 * Adds a player and saves. Returns false if already listed.
	 */
	static bool AddPlayer(const FString& PlayerName);

	/**
	 * Removes a player and saves. Returns false if not found.
	 */
	static bool RemovePlayer(const FString& PlayerName);

	/** Returns a copy of the current whitelist. */
	static TArray<FString> GetAll();

private:
	static FString GetFilePath();

	static bool bEnabled;
	static TArray<FString> Players; // stored lower-case for comparison
};
