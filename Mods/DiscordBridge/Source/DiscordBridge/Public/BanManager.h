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
 * The ban list (player names and platform IDs) is still persisted in the JSON
 * file across restarts.
 *
 * No additional dependency beyond Core/Json — uses UE4 Json + FFileHelper.
 *
 * Example file:
 *   {
 *     "enabled": true,
 *     "players": ["alice", "badguy"],
 *     "platform_ids": ["76561198123456789", "0002abcdef1234567890abcdef123456"]
 *   }
 *
 * Platform IDs are the stable unique identifiers assigned by the player's
 * online platform:
 *   - Steam players (DefaultPlatformService=Steam):  Steam64 ID
 *       e.g. 76561198123456789  (17 decimal digits, starts with "765")
 *   - Epic / EOS players (DefaultPlatformService=EOS): EOS Product User ID
 *       e.g. 0002abcdef1234567890abcdef123456  (hex alphanumeric string)
 *   When the server uses EOS, Steam players also get an EOS PUID rather than
 *   exposing their Steam64 ID.
 *
 * Banning by platform ID is more robust than by name because players cannot
 * change their platform ID, even if they change their display name.
 */
class DISCORDBRIDGE_API FBanManager
{
public:
	// ── Platform ID type detection ────────────────────────────────────────────

	/** Platform type inferred from a stored platform ID string. */
	enum class EPlatformIdType : uint8
	{
		Unknown = 0,
		Steam,  // Steam64 ID: 17 decimal digits, starts with "765"
		Epic,   // EOS Product User ID: hex alphanumeric, not Steam64 format
	};

	/**
	 * Infer the online platform from the format of a platform ID string.
	 *
	 * Steam64 IDs:  17 decimal digits that start with "765".
	 *               Example: 76561198123456789
	 * EOS PUIDs:    Hex alphanumeric strings (0-9, a-f) that do NOT match
	 *               the Steam64 format.
	 *               Example: 0002abcdef1234567890abcdef123456
	 * Unknown:      Any other format (not purely hex, wrong length, etc.).
	 */
	static EPlatformIdType GetPlatformIdType(const FString& PlatformId);

	/**
	 * Returns a human-readable label for a platform ID.
	 *   EPlatformIdType::Steam   → "Steam"
	 *   EPlatformIdType::Epic    → "EOS PUID"
	 *   EPlatformIdType::Unknown → "Unknown"
	 */
	static FString GetPlatformTypeLabel(const FString& PlatformId);

	/**
	 * Returns true if the given string is a valid Steam64 ID format.
	 * Steam64 IDs are exactly 17 decimal digits and begin with "765".
	 * Example: 76561198123456789
	 */
	static bool IsValidSteam64Id(const FString& Id);

	/**
	 * Normalizes a platform ID from the X-FactoryGame-PlayerId binary-hex format
	 * to the canonical form used by the ban system.
	 *
	 * The X-FactoryGame-PlayerId header is BytesToHex([type_byte, account_bytes]):
	 *   - Steam  (type 0x06): "06" + 16 hex chars (big-endian uint64 SteamID64)
	 *                         18 chars total → decimal Steam64 ID string
	 *                         e.g. "060011001199aa1234" → "76561198123456789"
	 *   - EOS    (type 0x01): "01" + 32 hex chars (raw 16-byte EOS ProductUserId)
	 *                         34 chars total → 32-char EOS PUID hex string
	 *                         e.g. "010002abcdef1234567890abcdef123456" → "0002abcdef1234567890abcdef123456"
	 *
	 * Any ID that is not in one of the recognized X-FactoryGame-PlayerId formats
	 * is returned unchanged (canonical Steam64 IDs and EOS PUIDs pass through as-is).
	 *
	 * This function is called automatically by BanPlatformId(), UnbanPlatformId(),
	 * and IsPlatformIdBanned() so that raw header values supplied by an admin
	 * (e.g. captured from server logs or a network trace) are transparently
	 * converted before being stored or compared.
	 *
	 * @param RawId  The raw platform ID string, which may be in X-FactoryGame-PlayerId
	 *               binary-hex format or already in canonical form.
	 * @return       The canonical platform ID (Steam64 decimal or EOS PUID hex),
	 *               or the original string if no conversion was possible.
	 */
	static FString NormalizePlatformId(const FString& RawId);

	// ── Lifecycle ─────────────────────────────────────────────────────────────

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

	// ── Name-based banning ────────────────────────────────────────────────────

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

	/** Returns a copy of the current player name ban list. */
	static TArray<FString> GetAll();

	// ── Platform ID (Steam / Epic Games) banning ──────────────────────────────

	/**
	 * Returns true if the given platform unique ID is banned.
	 * PlatformId is the stable identifier from the player's online platform:
	 *   Steam (DefaultPlatformService=Steam) → Steam64 ID (17-digit decimal)
	 *   EOS / Epic (DefaultPlatformService=EOS)  → EOS Product User ID (hex string)
	 * Comparison is case-insensitive.
	 */
	static bool IsPlatformIdBanned(const FString& PlatformId);

	/**
	 * Bans a player by platform ID and saves. Returns false if already banned.
	 * @param PlatformId  Steam64 ID or EOS Product User ID.
	 */
	static bool BanPlatformId(const FString& PlatformId);

	/**
	 * Unbans a player by platform ID and saves. Returns false if not found.
	 * @param PlatformId  Steam64 ID or EOS Product User ID.
	 */
	static bool UnbanPlatformId(const FString& PlatformId);

	/** Returns a copy of the current platform ID ban list. */
	static TArray<FString> GetAllPlatformIds();

	/**
	 * Returns all platform IDs that match the given platform type.
	 * Use EPlatformIdType::Steam to get all Steam64 IDs,
	 * EPlatformIdType::Epic to get all EOS PUIDs,
	 * EPlatformIdType::Unknown to get unrecognised entries.
	 */
	static TArray<FString> GetPlatformIdsByType(EPlatformIdType Type);

private:
	static FString GetFilePath();

	static bool bEnabled;
	static TArray<FString> Players;     // stored lower-case for comparison
	static TArray<FString> PlatformIds; // stored lower-case for comparison
};
