// Copyright Yamahasxviper. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"

/**
 * Configuration for the ServerWhitelist mod.
 *
 * Primary config file (edit this one):
 *   <ServerRoot>/FactoryGame/Mods/ServerWhitelist/Config/DefaultServerWhitelist.ini
 *
 * This file is NOT shipped in the mod package, so mod updates will never
 * overwrite it.  The mod creates it with defaults on the first server start.
 */
struct SERVERWHITELIST_API FServerWhitelistConfig
{
	/** When true, only whitelisted players can join the server. */
	bool bWhitelistEnabled{ false };

	/** Message shown to a kicked player in their Disconnected screen. */
	FString WhitelistKickReason{
		TEXT("You are not on this server's whitelist. Contact the server admin to be added.")
	};

	/**
	 * Prefix that triggers whitelist management commands when typed in the
	 * Satisfactory in-game chat.  Set to empty to disable in-game commands.
	 * Default: "!whitelist"
	 */
	FString InGameCommandPrefix{ TEXT("!whitelist") };

	/**
	 * In-game player names that are allowed to run !whitelist management commands.
	 * Comparison is case-insensitive.  Add each admin on its own +AdminPlayerNames= line.
	 */
	TArray<FString> AdminPlayerNames;

	/**
	 * Optional initial player list.  Applied only when ServerWhitelist.json does
	 * not yet exist (first server start after installing the mod).
	 * After that, the JSON file is authoritative and these entries are ignored.
	 */
	TArray<FString> InitialWhitelistedPlayers;

	/**
	 * Load (or create) the INI config from the mod Config folder.
	 * If the file does not exist it is created with default values so operators
	 * always have a well-commented template to edit.
	 */
	static FServerWhitelistConfig LoadOrCreate();

	/** Returns the absolute path to the primary INI config file. */
	static FString GetConfigFilePath();
};
