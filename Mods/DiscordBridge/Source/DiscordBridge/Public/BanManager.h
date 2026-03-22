#pragma once
#include "CoreMinimal.h"

/**
 * Manages the server ban list for the DiscordBridge mod.
 *
 * Config is stored at <ProjectSavedDir>/ServerBanlist.json.
 * The file is created with defaults on first use and written to disk
 * immediately on every change so bans survive server restarts.
 *
 * The enabled/disabled state is always driven by the INI config value passed to
 * Load() on every startup.  Runtime `!ban on` / `!ban off` commands update the
 * in-memory state for the current session; on the next restart the INI setting
 * takes effect again (so it acts as the persistent, authoritative toggle).
 * The ban list (player names) is still persisted in the JSON file across restarts.
 *
 * No additional dependency beyond Core/Json — uses UE4 Json + FFileHelper.
 *
 * Example file:
 *   {
 *     "enabled": true,
 *     "players": ["alice", "badguy"]
 *   }
 */
class DISCORDBRIDGE_API FBanManager
{
public:
	/**
	 * Load (or create) the ban list file from disk. Call once at startup,
	 * after the INI config has been loaded.
	 *
	 * @param bDefaultEnabled  The BanSystemEnabled value from the INI config.
	 *                         Applied on every startup — ban list players are read
	 *                         from the JSON file, but the enabled/disabled state
	 *                         always comes from this parameter.
	 */
	static void Load(bool bDefaultEnabled = true);

	/** Persist the current state to disk immediately. */
	static void Save();

	/** Returns true if the ban system is currently active. */
	static bool IsEnabled();

	/** Enable or disable the ban system and save. */
	static void SetEnabled(bool bEnabled);

	/**
	 * Returns true if the given player name is banned.
	 * Comparison is case-insensitive.
	 */
	static bool IsBanned(const FString& PlayerName);

	/**
	 * Bans a player and saves. Returns false if already banned.
	 */
	static bool BanPlayer(const FString& PlayerName);

	/**
	 * Unbans a player and saves. Returns false if not found.
	 */
	static bool UnbanPlayer(const FString& PlayerName);

	/** Returns a copy of the current ban list. */
	static TArray<FString> GetAll();

private:
	static FString GetFilePath();

	static bool bEnabled;
	static TArray<FString> Players; // stored lower-case for comparison
};
