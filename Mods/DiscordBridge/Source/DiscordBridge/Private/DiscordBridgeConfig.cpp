// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridgeConfig.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	static const TCHAR* ConfigSection = TEXT("DiscordBridge");

	// Returns the INI value when the key exists (including empty string values).
	// Use for optional settings where an empty value intentionally disables the feature.
	FString GetIniStringOrDefault(const FConfigFile& Cfg,
	                              const FString& Key,
	                              const FString& Default)
	{
		FString Value;
		return Cfg.GetString(ConfigSection, *Key, Value) ? Value : Default;
	}

	// Returns the INI value only when non-empty; falls back to Default otherwise.
	// Use for format/reason strings where leaving a setting blank means "use the default".
	FString GetIniStringOrFallback(const FConfigFile& Cfg,
	                               const FString& Key,
	                               const FString& Default)
	{
		FString Value;
		return (Cfg.GetString(ConfigSection, *Key, Value) && !Value.IsEmpty()) ? Value : Default;
	}

	bool GetIniBoolOrDefault(const FConfigFile& Cfg,
	                         const FString& Key,
	                         bool Default)
	{
		FString StrValue;
		if (!Cfg.GetString(ConfigSection, *Key, StrValue) || StrValue.IsEmpty())
			return Default;
		bool Value = Default;
		Cfg.GetBool(ConfigSection, *Key, Value);
		return Value;
	}

	float GetIniFloatOrDefault(const FConfigFile& Cfg,
	                           const FString& Key,
	                           float Default)
	{
		FString Value;
		if (Cfg.GetString(ConfigSection, *Key, Value) && !Value.IsEmpty())
		{
			return FCString::Atof(*Value);
		}
		return Default;
	}

	int32 GetIniIntOrDefault(const FConfigFile& Cfg,
	                         const FString& Key,
	                         int32 Default)
	{
		FString Value;
		if (Cfg.GetString(ConfigSection, *Key, Value) && !Value.IsEmpty())
		{
			return FCString::Atoi(*Value);
		}
		return Default;
	}

	// ── Raw INI readers (used for the backup file) ────────────────────────────
	// FConfigFile::GetString can expand %property% references in values, which
	// corrupts user-defined format strings that intentionally use %placeholders%
	// (e.g. %ServerName%, %PlayerName%, %Message%).  Since ServerName IS a key
	// in [DiscordBridge], FConfigFile would silently replace %ServerName% with
	// the current server name when reading back the backup, and PatchLine would
	// then write that expanded (corrupted) value permanently into the primary
	// config.  Reading the backup with raw string operations avoids all such
	// processing and guarantees values are preserved exactly as written.

	// Parses the named section of a raw INI file content string into a key→value
	// map.  Values are stored verbatim (no escape-sequence or %property%
	// expansion).  Duplicate keys are resolved in favour of the last occurrence.
	TMap<FString, FString> ParseRawIniSection(const FString& RawContent,
	                                          const FString& Section)
	{
		TMap<FString, FString> Result;
		const FString SectionHeader = FString(TEXT("[")) + Section + TEXT("]");

		bool bInSection = false;
		int32 Pos       = 0;

		while (Pos < RawContent.Len())
		{
			int32 LineEnd = RawContent.Find(
				TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			const bool bLastLine = (LineEnd == INDEX_NONE);
			if (bLastLine)
				LineEnd = RawContent.Len();

			// Strip the trailing \r for CRLF files before processing.
			FString Line = RawContent.Mid(Pos, LineEnd - Pos);
			if (!Line.IsEmpty() && Line[Line.Len() - 1] == TEXT('\r'))
				Line.RemoveAt(Line.Len() - 1, 1, /*bAllowShrinking=*/false);

			Pos = bLastLine ? RawContent.Len() : (LineEnd + 1);

			const FString Trimmed = Line.TrimStart();
			if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT(";")) || Trimmed.StartsWith(TEXT("#")))
				continue; // skip blank lines and comments

			if (Trimmed.StartsWith(TEXT("[")))
			{
				bInSection = (Trimmed.TrimEnd() == SectionHeader);
				continue;
			}

			if (bInSection)
			{
				int32 EqPos = INDEX_NONE;
				if (Line.FindChar(TEXT('='), EqPos))
				{
					const FString Key   = Line.Left(EqPos).TrimStartAndEnd();
					const FString Value = Line.Mid(EqPos + 1); // keep verbatim
					if (!Key.IsEmpty())
						Result.Add(Key, Value);
				}
			}
		}

		return Result;
	}

	// Raw-map equivalents of the FConfigFile helpers above.

	FString GetRawStringOrDefault(const TMap<FString, FString>& Raw,
	                              const FString& Key,
	                              const FString& Default)
	{
		const FString* Found = Raw.Find(Key);
		return Found ? *Found : Default;
	}

	FString GetRawStringOrFallback(const TMap<FString, FString>& Raw,
	                               const FString& Key,
	                               const FString& Default)
	{
		const FString* Found = Raw.Find(Key);
		return (Found && !Found->IsEmpty()) ? *Found : Default;
	}

	bool GetRawBoolOrDefault(const TMap<FString, FString>& Raw,
	                         const FString& Key,
	                         bool Default)
	{
		const FString* Found = Raw.Find(Key);
		if (!Found || Found->IsEmpty())
			return Default;
		const FString Trimmed = Found->TrimStartAndEnd();
		if (Trimmed.Equals(TEXT("True"),  ESearchCase::IgnoreCase) || Trimmed == TEXT("1"))
			return true;
		if (Trimmed.Equals(TEXT("False"), ESearchCase::IgnoreCase) || Trimmed == TEXT("0"))
			return false;
		return Default;
	}

	float GetRawFloatOrDefault(const TMap<FString, FString>& Raw,
	                           const FString& Key,
	                           float Default)
	{
		const FString* Found = Raw.Find(Key);
		return (Found && !Found->IsEmpty()) ? FCString::Atof(**Found) : Default;
	}

	int32 GetRawIntOrDefault(const TMap<FString, FString>& Raw,
	                         const FString& Key,
	                         int32 Default)
	{
		const FString* Found = Raw.Find(Key);
		return (Found && !Found->IsEmpty()) ? FCString::Atoi(**Found) : Default;
	}
} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// FDiscordBridgeConfig
// ─────────────────────────────────────────────────────────────────────────────

FString FDiscordBridgeConfig::GetModConfigFilePath()
{
	// The primary config lives in the mod's own Config folder so it is the
	// first place a server operator would look.  On a deployed server:
	//   <ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
	// NOTE: Alpakit/SMM mod updates overwrite this file; the mod automatically
	// saves a backup to GetBackupConfigFilePath() so credentials survive upgrades.
	return FPaths::Combine(FPaths::ProjectDir(),
	                       TEXT("Mods"), TEXT("DiscordBridge"),
	                       TEXT("Config"), TEXT("DefaultDiscordBridge.ini"));
}

FString FDiscordBridgeConfig::GetBackupConfigFilePath()
{
	// Backup config in Saved/Config/ – never touched by Alpakit mod updates.
	// Written automatically whenever the primary config has valid credentials.
	// On a deployed server:
	//   <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("DiscordBridge.ini"));
}

FDiscordBridgeConfig FDiscordBridgeConfig::LoadOrCreate()
{
	FDiscordBridgeConfig Config;
	const FString ModFilePath    = GetModConfigFilePath();
	const FString BackupFilePath = GetBackupConfigFilePath();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// ── Step 1: load the primary config (mod folder) ─────────────────────────
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field, add a read
	// line here AND update Steps 2 (restore + PatchLine) and 3 (backup write).
	bool bLoadedFromMod = false;
	if (PlatformFile.FileExists(*ModFilePath))
	{
		FConfigFile ConfigFile;
		ConfigFile.Read(ModFilePath);

		Config.BotToken             = GetIniStringOrDefault(ConfigFile, TEXT("BotToken"),             TEXT(""));
		Config.ChannelId            = GetIniStringOrDefault(ConfigFile, TEXT("ChannelId"),            TEXT(""));
		Config.ServerName           = GetIniStringOrDefault(ConfigFile, TEXT("ServerName"),           Config.ServerName);
		Config.GameToDiscordFormat  = GetIniStringOrFallback(ConfigFile, TEXT("GameToDiscordFormat"),  Config.GameToDiscordFormat);
		Config.DiscordToGameFormat  = GetIniStringOrFallback(ConfigFile, TEXT("DiscordToGameFormat"),  Config.DiscordToGameFormat);

		// Backward compatibility: if the old separate DiscordSenderFormat key is
		// present and the operator has not overridden DiscordToGameFormat to a value
		// that already mentions %Message%, synthesise a combined format so existing
		// configs continue to render as before.
		{
			FString OldSenderFmt;
			if (ConfigFile.GetString(ConfigSection, TEXT("DiscordSenderFormat"), OldSenderFmt) && !OldSenderFmt.IsEmpty())
			{
				// Only auto-combine when DiscordToGameFormat is still the default
				// (i.e. the operator has not explicitly customised it).
				if (Config.DiscordToGameFormat == TEXT("[Discord] %Username%: %Message%"))
				{
					Config.DiscordToGameFormat = OldSenderFmt + TEXT(": %Message%");
				}
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: 'DiscordSenderFormat' is deprecated. "
				            "Use 'DiscordToGameFormat' to control the full in-game line. "
				            "Effective format is now: \"%s\""), *Config.DiscordToGameFormat);
			}
		}
		Config.bIgnoreBotMessages   = GetIniBoolOrDefault  (ConfigFile, TEXT("IgnoreBotMessages"),
		                              GetIniBoolOrDefault  (ConfigFile, TEXT("bIgnoreBotMessages"),   Config.bIgnoreBotMessages));
		Config.bServerStatusMessagesEnabled = GetIniBoolOrDefault(ConfigFile, TEXT("ServerStatusMessagesEnabled"), Config.bServerStatusMessagesEnabled);
		Config.StatusChannelId      = GetIniStringOrDefault(ConfigFile, TEXT("StatusChannelId"),      Config.StatusChannelId);
		Config.ServerOnlineMessage  = GetIniStringOrDefault(ConfigFile, TEXT("ServerOnlineMessage"),  Config.ServerOnlineMessage);
		Config.ServerOfflineMessage = GetIniStringOrDefault(ConfigFile, TEXT("ServerOfflineMessage"), Config.ServerOfflineMessage);
		Config.bShowPlayerCountInPresence      = GetIniBoolOrDefault  (ConfigFile, TEXT("ShowPlayerCountInPresence"),
		                                         GetIniBoolOrDefault  (ConfigFile, TEXT("bShowPlayerCountInPresence"),      Config.bShowPlayerCountInPresence));
		Config.PlayerCountPresenceFormat       = GetIniStringOrFallback(ConfigFile, TEXT("PlayerCountPresenceFormat"),       Config.PlayerCountPresenceFormat);
		Config.PlayerCountUpdateIntervalSeconds = GetIniFloatOrDefault (ConfigFile, TEXT("PlayerCountUpdateIntervalSeconds"), Config.PlayerCountUpdateIntervalSeconds);
		Config.PlayerCountActivityType         = GetIniIntOrDefault   (ConfigFile, TEXT("PlayerCountActivityType"),         Config.PlayerCountActivityType);
		Config.WhitelistCommandRoleId          = GetIniStringOrDefault(ConfigFile, TEXT("WhitelistCommandRoleId"),          Config.WhitelistCommandRoleId);
		Config.BanCommandRoleId                = GetIniStringOrDefault(ConfigFile, TEXT("BanCommandRoleId"),                Config.BanCommandRoleId);
		Config.WhitelistCommandPrefix          = GetIniStringOrDefault(ConfigFile, TEXT("WhitelistCommandPrefix"),          Config.WhitelistCommandPrefix);
		Config.WhitelistRoleId                 = GetIniStringOrDefault(ConfigFile, TEXT("WhitelistRoleId"),                 Config.WhitelistRoleId);
		Config.WhitelistChannelId              = GetIniStringOrDefault(ConfigFile, TEXT("WhitelistChannelId"),              Config.WhitelistChannelId);
		Config.WhitelistKickDiscordMessage     = GetIniStringOrDefault(ConfigFile, TEXT("WhitelistKickDiscordMessage"),     Config.WhitelistKickDiscordMessage);
		Config.WhitelistKickReason             = GetIniStringOrFallback(ConfigFile, TEXT("WhitelistKickReason"),             Config.WhitelistKickReason);
		Config.bWhitelistEnabled               = GetIniBoolOrDefault  (ConfigFile, TEXT("WhitelistEnabled"),               Config.bWhitelistEnabled);
		Config.bBanSystemEnabled               = GetIniBoolOrDefault  (ConfigFile, TEXT("BanSystemEnabled"),               Config.bBanSystemEnabled);
		Config.BanCommandPrefix                = GetIniStringOrDefault(ConfigFile, TEXT("BanCommandPrefix"),                Config.BanCommandPrefix);
		Config.BanChannelId                    = GetIniStringOrDefault(ConfigFile, TEXT("BanChannelId"),                    Config.BanChannelId);
		Config.bBanCommandsEnabled             = GetIniBoolOrDefault  (ConfigFile, TEXT("BanCommandsEnabled"),             Config.bBanCommandsEnabled);
		Config.BanKickDiscordMessage           = GetIniStringOrDefault(ConfigFile, TEXT("BanKickDiscordMessage"),           Config.BanKickDiscordMessage);
		Config.BanKickReason                   = GetIniStringOrFallback(ConfigFile, TEXT("BanKickReason"),                   Config.BanKickReason);
		Config.InGameWhitelistCommandPrefix    = GetIniStringOrDefault(ConfigFile, TEXT("InGameWhitelistCommandPrefix"),    Config.InGameWhitelistCommandPrefix);
		Config.InGameBanCommandPrefix          = GetIniStringOrDefault(ConfigFile, TEXT("InGameBanCommandPrefix"),          Config.InGameBanCommandPrefix);
		Config.BanSystemSteamBanDiscordMessage   = GetIniStringOrDefault(ConfigFile, TEXT("BanSystemSteamBanDiscordMessage"),   Config.BanSystemSteamBanDiscordMessage);
		Config.BanSystemSteamUnbanDiscordMessage = GetIniStringOrDefault(ConfigFile, TEXT("BanSystemSteamUnbanDiscordMessage"), Config.BanSystemSteamUnbanDiscordMessage);
		Config.BanSystemEOSBanDiscordMessage     = GetIniStringOrDefault(ConfigFile, TEXT("BanSystemEOSBanDiscordMessage"),     Config.BanSystemEOSBanDiscordMessage);
		Config.BanSystemEOSUnbanDiscordMessage   = GetIniStringOrDefault(ConfigFile, TEXT("BanSystemEOSUnbanDiscordMessage"),   Config.BanSystemEOSUnbanDiscordMessage);

		// Trim leading/trailing whitespace from credential fields to prevent
		// subtle mismatches when operators accidentally include spaces.
		Config.BotToken  = Config.BotToken.TrimStartAndEnd();
		Config.ChannelId = Config.ChannelId.TrimStartAndEnd();

		bLoadedFromMod = true;
		UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Loaded config from %s"), *ModFilePath);

		// When BotToken is empty the file may not have been configured yet.
		// Only rewrite with the annotated template when the file has no '#'
		// comment lines, which indicates Alpakit stripped them during packaging
		// and the operator cannot see the setting descriptions.  If the file
		// already contains '#' characters it is either a previously-written
		// annotated template or a user-configured file; in that case do NOT
		// overwrite it so the operator's other settings (server name, online/
		// offline messages, format strings, etc.) are preserved even when
		// BotToken has not been filled in yet.
		if (Config.BotToken.IsEmpty())
		{
			FString ModFileRaw;
			FFileHelper::LoadFileToString(ModFileRaw, *ModFilePath);
			if (!ModFileRaw.Contains(TEXT("#")))
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Config at '%s' has no BotToken and no comment "
				            "lines (Alpakit-stripped or comment-free) – rewriting with "
				            "annotated template so operators can see setting descriptions."),
				       *ModFilePath);
				bLoadedFromMod = false; // fall through to the DefaultContent write below
			}
			else
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Config at '%s' has no BotToken – "
				            "Discord bridge will not start until BotToken is configured."),
				       *ModFilePath);
				// bLoadedFromMod stays true; all other settings loaded above are used.
			}
		}
		else
		{
			// Detect configs written before the whitelist / ban system was added
			// (upgrade scenario).  If either section's key-value pair is absent
			// from the file, append the missing sections so server operators can
			// see and configure the new settings without losing their existing ones.
			FString TmpVal;
			const bool bFileHasWhitelist = ConfigFile.GetString(ConfigSection, TEXT("WhitelistEnabled"), TmpVal);
			const bool bFileHasBan       = ConfigFile.GetString(ConfigSection, TEXT("BanSystemEnabled"), TmpVal);

			if (!bFileHasWhitelist || !bFileHasBan)
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Config at '%s' is missing whitelist/ban settings "
				            "(older version detected). Appending new settings."),
				       *ModFilePath);

				FString ExistingContent;
				FFileHelper::LoadFileToString(ExistingContent, *ModFilePath);

				FString AppendContent;

				if (!bFileHasWhitelist)
				{
					AppendContent +=
						TEXT("\n")
						TEXT("# -- WHITELIST (added by mod update) -----------------------------------------\n")
						TEXT("# Controls the built-in server whitelist, manageable via Discord commands.\n")
						TEXT("#\n")
						TEXT("# The whitelist and the ban system are COMPLETELY INDEPENDENT of each other.\n")
						TEXT("# You can use either one, both, or neither:\n")
						TEXT("#\n")
						TEXT("#   Whitelist only:   WhitelistEnabled=True,  BanSystemEnabled=False\n")
						TEXT("#   Ban system only:  WhitelistEnabled=False, BanSystemEnabled=True\n")
						TEXT("#   Both:             WhitelistEnabled=True,  BanSystemEnabled=True\n")
						TEXT("#   Neither:          WhitelistEnabled=False, BanSystemEnabled=False\n")
						TEXT("#\n")
						TEXT("# Whether the whitelist is active. Applied on every server restart.\n")
						TEXT("# Default: False (all players can join).\n")
						TEXT("WhitelistEnabled=False\n")
						TEXT("#\n")
						TEXT("# Snowflake ID of the Discord role whose members may run !whitelist commands.\n")
						TEXT("# Leave empty (default) to disable !whitelist commands for all Discord users.\n")
						TEXT("WhitelistCommandRoleId=\n")
						TEXT("#\n")
						TEXT("# Prefix that triggers whitelist commands in the bridged Discord channel.\n")
						TEXT("# Set to empty to disable Discord-based whitelist management entirely.\n")
						TEXT("WhitelistCommandPrefix=!whitelist\n")
						TEXT("#\n")
						TEXT("# Snowflake ID of the Discord role assigned to whitelisted members.\n")
						TEXT("# Leave empty to disable Discord role integration.\n")
						TEXT("WhitelistRoleId=\n")
						TEXT("#\n")
						TEXT("# Snowflake ID of a dedicated Discord channel for whitelisted members.\n")
						TEXT("# Leave empty to disable the whitelist-only channel.\n")
						TEXT("WhitelistChannelId=\n")
						TEXT("#\n")
						TEXT("# Message posted to Discord when a non-whitelisted player is kicked.\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("# Placeholder: %PlayerName% - in-game name of the kicked player.\n")
						TEXT("WhitelistKickDiscordMessage=:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.\n")
						TEXT("#\n")
						TEXT("# Reason shown in-game to the player when they are kicked for not being whitelisted.\n")
						TEXT("WhitelistKickReason=You are not on this server's whitelist. Contact the server admin to be added.\n")
						TEXT("#\n")
						TEXT("# Prefix that triggers whitelist commands in the in-game chat.\n")
						TEXT("# Set to empty to disable in-game whitelist commands.\n")
						TEXT("InGameWhitelistCommandPrefix=!whitelist\n");
				}

				if (!bFileHasBan)
				{
					AppendContent +=
						TEXT("\n")
						TEXT("# -- BAN SYSTEM (added by mod update) ----------------------------------------\n")
						TEXT("# Controls the built-in player ban system, manageable via Discord commands.\n")
						TEXT("# Bans are stored in <ServerRoot>/FactoryGame/Saved/ServerBanlist.json.\n")
						TEXT("#\n")
						TEXT("# The ban system and the whitelist are COMPLETELY INDEPENDENT of each other.\n")
						TEXT("#\n")
						TEXT("# Whether the ban system is active. Applied on every server restart.\n")
						TEXT("# Default: True (ban enforcement is on by default).\n")
						TEXT("BanSystemEnabled=True\n")
						TEXT("#\n")
						TEXT("# Snowflake ID of the Discord role whose members may run !ban commands.\n")
						TEXT("# Leave empty (default) to disable !ban commands for all Discord users.\n")
						TEXT("BanCommandRoleId=\n")
						TEXT("#\n")
						TEXT("# Prefix that triggers ban commands in the bridged Discord channel.\n")
						TEXT("# Set to empty to disable Discord-based ban management entirely.\n")
						TEXT("BanCommandPrefix=!ban\n")
						TEXT("#\n")
						TEXT("# Snowflake ID of a dedicated Discord channel for ban management.\n")
						TEXT("# Leave empty to disable the ban-only channel.\n")
						TEXT("BanChannelId=\n")
						TEXT("#\n")
						TEXT("# Master on/off switch for the ban command interface.\n")
						TEXT("# Default: True\n")
						TEXT("BanCommandsEnabled=True\n")
						TEXT("#\n")
						TEXT("# Message posted to Discord when a banned player is kicked.\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("# Placeholder: %PlayerName% - in-game name of the kicked player.\n")
						TEXT("BanKickDiscordMessage=:hammer: **%PlayerName%** is banned from this server and was kicked.\n")
						TEXT("#\n")
						TEXT("# Reason shown in-game to the player when they are kicked for being banned.\n")
						TEXT("BanKickReason=You are banned from this server.\n")
						TEXT("#\n")
						TEXT("# Prefix that triggers ban commands in the in-game chat.\n")
						TEXT("# Set to empty to disable in-game ban commands.\n")
						TEXT("InGameBanCommandPrefix=!ban\n");
				}

				if (FFileHelper::SaveStringToFile(ExistingContent + AppendContent, *ModFilePath))
				{
					UE_LOG(LogTemp, Log,
					       TEXT("DiscordBridge: Updated '%s' with whitelist/ban settings. "
					            "Review and configure them, then restart the server."),
					       *ModFilePath);
				}
				else
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("DiscordBridge: Could not update '%s' with whitelist/ban settings."),
					       *ModFilePath);
				}
			}

			// Second pass: detect individual settings that were added in later updates
			// but may be absent from configs that already have the whitelist/ban sections.
			// Only appends the specific missing keys so no existing custom values are lost.
			{
				FString AppendContent2;

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("WhitelistCommandRoleId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# WhitelistCommandRoleId (added by mod update) --------------------------\n")
						TEXT("# Snowflake ID of the Discord role whose members may run !whitelist commands.\n")
						TEXT("# Leave empty (default) to disable !whitelist commands for all Discord users.\n")
						TEXT("WhitelistCommandRoleId=\n");
				}

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("WhitelistRoleId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# WhitelistRoleId (added by mod update) ---------------------------------\n")
						TEXT("# Snowflake ID of the Discord role assigned to whitelisted members.\n")
						TEXT("# Leave empty to disable Discord role integration.\n")
						TEXT("WhitelistRoleId=\n");
				}

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("WhitelistChannelId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# WhitelistChannelId (added by mod update) --------------------------------\n")
						TEXT("# Snowflake ID of a dedicated Discord channel for whitelisted members.\n")
						TEXT("# Leave empty to disable the whitelist-only channel.\n")
						TEXT("WhitelistChannelId=\n");
				}

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("WhitelistKickDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# WhitelistKickDiscordMessage (added by mod update) -------------------\n")
						TEXT("# Message posted to Discord when a non-whitelisted player is kicked.\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("# Placeholder: %PlayerName% - in-game name of the kicked player.\n")
						TEXT("WhitelistKickDiscordMessage=:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.\n");
				}

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("WhitelistKickReason"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# WhitelistKickReason (added by mod update) ----------------------------\n")
						TEXT("# Reason shown in-game to the player when kicked for not being whitelisted.\n")
						TEXT("WhitelistKickReason=You are not on this server's whitelist. Contact the server admin to be added.\n");
				}

				if (bFileHasWhitelist &&
				    !ConfigFile.GetString(ConfigSection, TEXT("InGameWhitelistCommandPrefix"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# InGameWhitelistCommandPrefix (added by mod update) -------------------\n")
						TEXT("# Prefix that triggers whitelist commands when typed in the in-game chat.\n")
						TEXT("# Set to empty to disable in-game whitelist commands.\n")
						TEXT("InGameWhitelistCommandPrefix=!whitelist\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanCommandRoleId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanCommandRoleId (added by mod update) --------------------------------\n")
						TEXT("# Snowflake ID of the Discord role whose members may run !ban commands.\n")
						TEXT("# Leave empty (default) to disable !ban commands for all Discord users.\n")
						TEXT("BanCommandRoleId=\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanCommandPrefix"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanCommandPrefix (added by mod update) --------------------------------\n")
						TEXT("# Prefix that triggers ban commands in the bridged Discord channel.\n")
						TEXT("# Set to empty to disable Discord-based ban management entirely.\n")
						TEXT("BanCommandPrefix=!ban\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanKickDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanKickDiscordMessage (added by mod update) ----------------------------\n")
						TEXT("# Message posted to Discord when a banned player is kicked.\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("# Placeholder: %PlayerName% - in-game name of the kicked player.\n")
						TEXT("BanKickDiscordMessage=:hammer: **%PlayerName%** is banned from this server and was kicked.\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanKickReason"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanKickReason (added by mod update) -----------------------------------------\n")
						TEXT("# Reason shown in-game to the player when kicked for being banned.\n")
						TEXT("BanKickReason=You are banned from this server.\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("InGameBanCommandPrefix"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# InGameBanCommandPrefix (added by mod update) -------------------------\n")
						TEXT("# Prefix that triggers ban commands when typed in the in-game chat.\n")
						TEXT("# Set to empty to disable in-game ban commands.\n")
						TEXT("InGameBanCommandPrefix=!ban\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanChannelId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanChannelId (added by mod update) -----------------------------------\n")
						TEXT("# Snowflake ID of a dedicated Discord channel for ban management.\n")
						TEXT("# Leave empty to disable the ban-only channel.\n")
						TEXT("BanChannelId=\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanCommandsEnabled"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanCommandsEnabled (added by mod update) ----------------------------\n")
						TEXT("# When True (default), !ban Discord and in-game commands are enabled.\n")
						TEXT("# Set to False to disable ban commands while still enforcing bans.\n")
						TEXT("BanCommandsEnabled=True\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanSystemSteamBanDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanSystemSteamBanDiscordMessage (added by mod update) ----------------\n")
						TEXT("# Message posted to Discord when BanSystem bans a player by Steam64 ID.\n")
						TEXT("# Placeholders: %PlayerId%, %Reason%, %BannedBy%\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("BanSystemSteamBanDiscordMessage=:hammer: **BanSystem** - Steam ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanSystemSteamUnbanDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanSystemSteamUnbanDiscordMessage (added by mod update) --------------\n")
						TEXT("# Message posted to Discord when BanSystem unbans a player by Steam64 ID.\n")
						TEXT("# Placeholder: %PlayerId%\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("BanSystemSteamUnbanDiscordMessage=:white_check_mark: **BanSystem** - Steam ID `%PlayerId%` has been unbanned.\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanSystemEOSBanDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanSystemEOSBanDiscordMessage (added by mod update) ------------------\n")
						TEXT("# Message posted to Discord when BanSystem bans a player by EOS Product User ID.\n")
						TEXT("# Placeholders: %PlayerId%, %Reason%, %BannedBy%\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("BanSystemEOSBanDiscordMessage=:hammer: **BanSystem** - EOS ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%\n");
				}

				if (bFileHasBan &&
				    !ConfigFile.GetString(ConfigSection, TEXT("BanSystemEOSUnbanDiscordMessage"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# BanSystemEOSUnbanDiscordMessage (added by mod update) ----------------\n")
						TEXT("# Message posted to Discord when BanSystem unbans a player by EOS Product User ID.\n")
						TEXT("# Placeholder: %PlayerId%\n")
						TEXT("# Leave empty to disable this notification.\n")
						TEXT("BanSystemEOSUnbanDiscordMessage=:white_check_mark: **BanSystem** - EOS ID `%PlayerId%` has been unbanned.\n");
				}

				if (!ConfigFile.GetString(ConfigSection, TEXT("ServerStatusMessagesEnabled"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# ServerStatusMessagesEnabled (added by mod update) -------------------\n")
						TEXT("# Master on/off switch for server status messages (online/offline).\n")
						TEXT("# Set to False to disable all online/offline notifications.\n")
						TEXT("# Default: True\n")
						TEXT("ServerStatusMessagesEnabled=True\n");
				}

				if (!ConfigFile.GetString(ConfigSection, TEXT("StatusChannelId"), TmpVal))
				{
					AppendContent2 +=
						TEXT("\n")
						TEXT("# StatusChannelId (added by mod update) --------------------------------\n")
						TEXT("# Snowflake ID of a dedicated Discord channel for server status messages.\n")
						TEXT("# Leave empty to post status messages to the main bridged channel (ChannelId).\n")
						TEXT("StatusChannelId=\n");
				}

				if (!AppendContent2.IsEmpty())
				{
					UE_LOG(LogTemp, Log,
					       TEXT("DiscordBridge: Config at '%s' is missing individual settings "
					            "(older version detected). Appending missing entries."),
					       *ModFilePath);

					FString ExistingContent2;
					FFileHelper::LoadFileToString(ExistingContent2, *ModFilePath);

					if (FFileHelper::SaveStringToFile(ExistingContent2 + AppendContent2, *ModFilePath))
					{
						UE_LOG(LogTemp, Log,
						       TEXT("DiscordBridge: Updated '%s' with missing settings. "
						            "Review and configure them, then restart the server."),
						       *ModFilePath);
					}
					else
					{
						UE_LOG(LogTemp, Warning,
						       TEXT("DiscordBridge: Could not update '%s' with missing settings."),
						       *ModFilePath);
					}
				}
			}
		}
	}

	if (!bLoadedFromMod)
	{
		if (!PlatformFile.FileExists(*ModFilePath))
		{
			// Primary config missing – create it with defaults so the operator has
			// a ready-made file to fill in.
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Config file not found at '%s'. "
			            "Creating it with defaults."), *ModFilePath);
		}

				const FString DefaultContent =
			TEXT("[DiscordBridge]\n")
			TEXT("# DiscordBridge - Primary Configuration File\n")
			TEXT("# ===========================================\n")
			TEXT("# 1. Set BotToken and ChannelId below.\n")
			TEXT("# 2. Restart the server. The bridge starts automatically.\n")
			TEXT("# Note: this file is NOT overwritten by mod updates, so your settings persist\n")
			TEXT("#   across version upgrades. A backup is still written automatically to\n")
			TEXT("#   <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini on every server start.\n")
			TEXT("# Bot setup: Discord Developer Portal -> your app -> Bot\n")
			TEXT("#   - Enable Server Members and Message Content intents.\n")
			TEXT("#   - Invite the bot with Send Messages + Read Message History permissions.\n")
			TEXT("#   - Enable Developer Mode in Discord, right-click the channel, Copy Channel ID.\n")
			TEXT("\n")
			TEXT("# -- CONNECTION ---------------------------------------------------------------\n")
			TEXT("# Discord bot token (Bot -> Token in Developer Portal). Treat as a password.\n")
			TEXT("BotToken=\n")
			TEXT("# Snowflake ID of the Discord text channel to bridge with in-game chat.\n")
			TEXT("ChannelId=\n")
			TEXT("# Display name for this server. Used as the %ServerName% placeholder.\n")
			TEXT("ServerName=\n")
			TEXT("\n")
			TEXT("# -- CHAT CUSTOMISATION -------------------------------------------------------\n")
			TEXT("# Format for game -> Discord. Placeholders: %ServerName%, %PlayerName%, %Message%\n")
			TEXT("# Default: **%PlayerName%**: %Message%\n")
			TEXT("GameToDiscordFormat=\n")
			TEXT("# Format for Discord -> game. Placeholders: %Username%, %PlayerName%, %Message%\n")
			TEXT("# Default: [Discord] %Username%: %Message%\n")
			TEXT("DiscordToGameFormat=\n")
			TEXT("\n")
			TEXT("# -- BEHAVIOUR ----------------------------------------------------------------\n")
			TEXT("# When True, messages from bot accounts are ignored (prevents echo loops).\n")
			TEXT("# Default: True\n")
			TEXT("IgnoreBotMessages=\n")
			TEXT("\n")
			TEXT("# -- SERVER STATUS MESSAGES ---------------------------------------------------\n")
			TEXT("# Master on/off switch for server status messages. Default: True\n")
			TEXT("# Set to False to disable all online/offline notifications.\n")
			TEXT("ServerStatusMessagesEnabled=True\n")
			TEXT("# Snowflake ID of a dedicated Discord channel for server status messages.\n")
			TEXT("# Leave empty to post status messages to the main bridged channel (ChannelId).\n")
			TEXT("# How to get it: enable Developer Mode in Discord, right-click the channel, Copy Channel ID.\n")
			TEXT("StatusChannelId=\n")
			TEXT("# Message posted to Discord when the server starts. Leave empty to disable.\n")
			TEXT("ServerOnlineMessage=:green_circle: Server is now **online**!\n")
			TEXT("# Message posted to Discord when the server stops. Leave empty to disable.\n")
			TEXT("ServerOfflineMessage=:red_circle: Server is now **offline**.\n")
			TEXT("\n")
			TEXT("# -- PLAYER COUNT PRESENCE ----------------------------------------------------\n")
			TEXT("# When True, the bot's Discord status shows the current player count.\n")
			TEXT("# Default: True\n")
			TEXT("ShowPlayerCountInPresence=\n")
			TEXT("# Text shown in the bot's Discord presence. Placeholders: %PlayerCount%, %ServerName%\n")
			TEXT("# Default: Satisfactory with %PlayerCount% players\n")
			TEXT("PlayerCountPresenceFormat=\n")
			TEXT("# How often (in seconds) to refresh the player count. Minimum 15. Default 60.\n")
			TEXT("PlayerCountUpdateIntervalSeconds=\n")
			TEXT("# Activity type: 0=Playing, 2=Listening to, 3=Watching, 5=Competing in. Default 0.\n")
			TEXT("PlayerCountActivityType=\n")
			TEXT("\n")
			TEXT("# -- WHITELIST ----------------------------------------------------------------\n")
			TEXT("# Whether the whitelist is active. Default: False (all players can join).\n")
			TEXT("WhitelistEnabled=False\n")
			TEXT("# Snowflake ID of the Discord role whose members may run !whitelist commands.\n")
			TEXT("WhitelistCommandRoleId=\n")
			TEXT("# Prefix for whitelist commands in Discord. Default: !whitelist\n")
			TEXT("WhitelistCommandPrefix=!whitelist\n")
			TEXT("# Snowflake ID of the Discord role assigned to whitelisted members.\n")
			TEXT("WhitelistRoleId=\n")
			TEXT("# Snowflake ID of a dedicated Discord channel for whitelisted members.\n")
			TEXT("WhitelistChannelId=\n")
			TEXT("# Message posted to Discord when a non-whitelisted player is kicked.\n")
			TEXT("WhitelistKickDiscordMessage=:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.\n")
			TEXT("# Reason shown in-game to the player when kicked for not being whitelisted.\n")
			TEXT("WhitelistKickReason=\n")
			TEXT("# Prefix for whitelist commands in the in-game chat. Default: !whitelist\n")
			TEXT("InGameWhitelistCommandPrefix=!whitelist\n")
			TEXT("\n")
			TEXT("# -- BAN SYSTEM ---------------------------------------------------------------\n")
			TEXT("# Whether the ban system is active. Default: True (ban list is enforced).\n")
			TEXT("BanSystemEnabled=True\n")
			TEXT("# Snowflake ID of the Discord role whose members may run !ban commands.\n")
			TEXT("BanCommandRoleId=\n")
			TEXT("# Prefix for ban commands in Discord. Default: !ban\n")
			TEXT("BanCommandPrefix=!ban\n")
			TEXT("# Snowflake ID of a dedicated Discord channel for ban management.\n")
			TEXT("BanChannelId=\n")
			TEXT("# Master on/off switch for ban commands. Default: True\n")
			TEXT("BanCommandsEnabled=True\n")
			TEXT("# Message posted to Discord when a banned player is kicked.\n")
			TEXT("BanKickDiscordMessage=:hammer: **%PlayerName%** is banned from this server and was kicked.\n")
			TEXT("# Reason shown in-game to the player when kicked for being banned.\n")
			TEXT("BanKickReason=\n")
			TEXT("# Prefix for ban commands in the in-game chat. Default: !ban\n")
			TEXT("InGameBanCommandPrefix=!ban\n")
			TEXT("\n")
			TEXT("# -- BANSYSTEM MOD INTEGRATION -----------------------------------------------\n")
			TEXT("# Messages posted to Discord when the BanSystem mod issues or removes a ban\n")
			TEXT("# via its in-game chat commands (/steamban, /steamunban, /eosban, /eosunban,\n")
			TEXT("# /banbyname).  If BanSystem is not installed these settings are ignored.\n")
			TEXT("# Leave any message empty to disable that specific Discord notification.\n")
			TEXT("# Placeholders for ban messages:   %PlayerId%, %Reason%, %BannedBy%\n")
			TEXT("# Placeholder  for unban messages: %PlayerId%\n")
			TEXT("BanSystemSteamBanDiscordMessage=:hammer: **BanSystem** - Steam ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%\n")
			TEXT("BanSystemSteamUnbanDiscordMessage=:white_check_mark: **BanSystem** - Steam ID `%PlayerId%` has been unbanned.\n")
			TEXT("BanSystemEOSBanDiscordMessage=:hammer: **BanSystem** - EOS ID `%PlayerId%` was banned by **%BannedBy%** - Reason: %Reason%\n")
			TEXT("BanSystemEOSUnbanDiscordMessage=:white_check_mark: **BanSystem** - EOS ID `%PlayerId%` has been unbanned.\n");

		// Ensure the Config directory exists before writing.
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(ModFilePath));

		if (FFileHelper::SaveStringToFile(DefaultContent, *ModFilePath))
		{
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Wrote default config to '%s'. "
			            "Set BotToken and ChannelId in that file, then restart "
			            "the server to enable the Discord bridge."), *ModFilePath);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("DiscordBridge: Could not write default config to '%s'."),
			       *ModFilePath);
		}
	}

	// ── Step 2: fall back to backup when credentials are missing ──────────────
	// This happens after a mod update resets the primary config file.
	// All settings (connection, chat, presence, whitelist, ban) are read from
	// the single DiscordBridge.ini backup so they survive mod updates.
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field, add a restore
	// line below AND a PatchLine call further down AND update Steps 1 and 3.
	//
	// IMPORTANT: the backup is read with raw string parsing (ParseRawIniSection)
	// instead of FConfigFile::Read + GetString.  FConfigFile expands %property%
	// references in values against keys in the same section.  Because ServerName
	// IS a key in [DiscordBridge], any format string that contains %ServerName%
	// (e.g. GameToDiscordFormat=**[%ServerName%] %PlayerName%**: %Message%) would
	// have %ServerName% silently replaced with the actual server name.  PatchLine
	// would then write that expanded value permanently into the primary config,
	// destroying the dynamic placeholder.  Raw parsing avoids all such processing.
	if ((Config.BotToken.IsEmpty() || Config.ChannelId.IsEmpty()) &&
	    PlatformFile.FileExists(*BackupFilePath))
	{
		FString BackupFileContent;
		FFileHelper::LoadFileToString(BackupFileContent, *BackupFilePath);
		const TMap<FString, FString> BackupValues =
			ParseRawIniSection(BackupFileContent, TEXT("DiscordBridge"));

		const bool bHadToken   = !Config.BotToken.IsEmpty();
		const bool bHadChannel = !Config.ChannelId.IsEmpty();

		if (Config.BotToken.IsEmpty())
		{
			Config.BotToken = GetRawStringOrDefault(BackupValues, TEXT("BotToken"), TEXT("")).TrimStartAndEnd();
		}
		if (Config.ChannelId.IsEmpty())
		{
			Config.ChannelId = GetRawStringOrDefault(BackupValues, TEXT("ChannelId"), TEXT("")).TrimStartAndEnd();
		}

		// Restore all other user-customised settings from the backup so that
		// message formats and status messages also survive a mod update that
		// resets the primary config to its shipped defaults.
		Config.ServerName           = GetRawStringOrDefault(BackupValues, TEXT("ServerName"),           Config.ServerName);
		Config.GameToDiscordFormat  = GetRawStringOrFallback(BackupValues, TEXT("GameToDiscordFormat"),  Config.GameToDiscordFormat);
		Config.DiscordToGameFormat  = GetRawStringOrFallback(BackupValues, TEXT("DiscordToGameFormat"),  Config.DiscordToGameFormat);
		Config.bIgnoreBotMessages   = GetRawBoolOrDefault  (BackupValues, TEXT("IgnoreBotMessages"),
		                              GetRawBoolOrDefault  (BackupValues, TEXT("bIgnoreBotMessages"),   Config.bIgnoreBotMessages));
		Config.bServerStatusMessagesEnabled = GetRawBoolOrDefault(BackupValues, TEXT("ServerStatusMessagesEnabled"), Config.bServerStatusMessagesEnabled);
		Config.StatusChannelId      = GetRawStringOrDefault(BackupValues, TEXT("StatusChannelId"),      Config.StatusChannelId);
		Config.ServerOnlineMessage  = GetRawStringOrDefault(BackupValues, TEXT("ServerOnlineMessage"),  Config.ServerOnlineMessage);
		Config.ServerOfflineMessage = GetRawStringOrDefault(BackupValues, TEXT("ServerOfflineMessage"), Config.ServerOfflineMessage);
		Config.bShowPlayerCountInPresence       = GetRawBoolOrDefault  (BackupValues, TEXT("ShowPlayerCountInPresence"),
		                                          GetRawBoolOrDefault  (BackupValues, TEXT("bShowPlayerCountInPresence"),       Config.bShowPlayerCountInPresence));
		Config.PlayerCountPresenceFormat        = GetRawStringOrFallback(BackupValues, TEXT("PlayerCountPresenceFormat"),        Config.PlayerCountPresenceFormat);
		Config.PlayerCountUpdateIntervalSeconds = GetRawFloatOrDefault (BackupValues, TEXT("PlayerCountUpdateIntervalSeconds"), Config.PlayerCountUpdateIntervalSeconds);
		Config.PlayerCountActivityType          = GetRawIntOrDefault   (BackupValues, TEXT("PlayerCountActivityType"),          Config.PlayerCountActivityType);
		Config.WhitelistCommandRoleId           = GetRawStringOrDefault(BackupValues, TEXT("WhitelistCommandRoleId"),           Config.WhitelistCommandRoleId);
		Config.BanCommandRoleId                 = GetRawStringOrDefault(BackupValues, TEXT("BanCommandRoleId"),                 Config.BanCommandRoleId);
		Config.WhitelistCommandPrefix           = GetRawStringOrDefault(BackupValues, TEXT("WhitelistCommandPrefix"),           Config.WhitelistCommandPrefix);
		Config.WhitelistRoleId                  = GetRawStringOrDefault(BackupValues, TEXT("WhitelistRoleId"),                  Config.WhitelistRoleId);
		Config.WhitelistChannelId               = GetRawStringOrDefault(BackupValues, TEXT("WhitelistChannelId"),               Config.WhitelistChannelId);
		Config.WhitelistKickDiscordMessage      = GetRawStringOrDefault(BackupValues, TEXT("WhitelistKickDiscordMessage"),      Config.WhitelistKickDiscordMessage);
		Config.WhitelistKickReason              = GetRawStringOrFallback(BackupValues, TEXT("WhitelistKickReason"),              Config.WhitelistKickReason);
		Config.bWhitelistEnabled                = GetRawBoolOrDefault  (BackupValues, TEXT("WhitelistEnabled"),                Config.bWhitelistEnabled);
		Config.bBanSystemEnabled                = GetRawBoolOrDefault  (BackupValues, TEXT("BanSystemEnabled"),                Config.bBanSystemEnabled);
		Config.BanCommandPrefix                 = GetRawStringOrDefault(BackupValues, TEXT("BanCommandPrefix"),                 Config.BanCommandPrefix);
		Config.BanChannelId                     = GetRawStringOrDefault(BackupValues, TEXT("BanChannelId"),                     Config.BanChannelId);
		Config.bBanCommandsEnabled              = GetRawBoolOrDefault  (BackupValues, TEXT("BanCommandsEnabled"),              Config.bBanCommandsEnabled);
		Config.BanKickDiscordMessage            = GetRawStringOrDefault(BackupValues, TEXT("BanKickDiscordMessage"),            Config.BanKickDiscordMessage);
		Config.BanKickReason                    = GetRawStringOrFallback(BackupValues, TEXT("BanKickReason"),                    Config.BanKickReason);
		Config.InGameWhitelistCommandPrefix     = GetRawStringOrDefault(BackupValues, TEXT("InGameWhitelistCommandPrefix"),     Config.InGameWhitelistCommandPrefix);
		Config.InGameBanCommandPrefix           = GetRawStringOrDefault(BackupValues, TEXT("InGameBanCommandPrefix"),           Config.InGameBanCommandPrefix);
		Config.BanSystemSteamBanDiscordMessage   = GetRawStringOrDefault(BackupValues, TEXT("BanSystemSteamBanDiscordMessage"),   Config.BanSystemSteamBanDiscordMessage);
		Config.BanSystemSteamUnbanDiscordMessage = GetRawStringOrDefault(BackupValues, TEXT("BanSystemSteamUnbanDiscordMessage"), Config.BanSystemSteamUnbanDiscordMessage);
		Config.BanSystemEOSBanDiscordMessage     = GetRawStringOrDefault(BackupValues, TEXT("BanSystemEOSBanDiscordMessage"),     Config.BanSystemEOSBanDiscordMessage);
		Config.BanSystemEOSUnbanDiscordMessage   = GetRawStringOrDefault(BackupValues, TEXT("BanSystemEOSUnbanDiscordMessage"),   Config.BanSystemEOSUnbanDiscordMessage);

		// Only log the "restored from backup" message when credentials were
		// actually recovered (i.e. previously blank in primary but now non-empty
		// from the backup). Avoid a misleading message when the backup also has
		// blank credentials (e.g. first server start before credentials are set).
		const bool bRestoredToken   = !bHadToken   && !Config.BotToken.IsEmpty();
		const bool bRestoredChannel = !bHadChannel && !Config.ChannelId.IsEmpty();
		if (bRestoredToken || bRestoredChannel)
		{
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Primary config '%s' had no credentials – "
			            "restored from backup at '%s'. "
			            "Writing all settings back to the primary config so they persist there."),
			       *ModFilePath, *BackupFilePath);

			// Write every restored setting back into the primary config file so
			// operators always see their current settings there and do not have to
			// re-enter them manually after every mod update.
			// Uses a line-level replace: find "Key=<anything>\n" at the start of a
			// line and replace it with "Key=<Value>\n", preserving line endings and
			// all surrounding comment lines.  This handles both empty-value template
			// lines (e.g. "BotToken=\n") and lines with default values already set
			// (e.g. "WhitelistEnabled=False\n") so every user-customised setting
			// survives subsequent restarts after a mod update.
			// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field, add a
			// PatchLine call below so the restored value is written back here too.
			FString PrimaryContent;
			if (FFileHelper::LoadFileToString(PrimaryContent, *ModFilePath))
			{
				// Replace the line "Key=<current>\n" (at the start of a line) with
				// "Key=Value\n", preserving \r\n vs \n line endings.
				auto PatchLine = [&](const TCHAR* Key, const FString& Value)
				{
					const FString Prefix = FString(Key) + TEXT("=");
					for (int32 From = 0;;)
					{
						const int32 At = PrimaryContent.Find(
							Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
						if (At == INDEX_NONE)
							break;
						// Only replace when the key appears at the start of a line.
						if (At == 0 || PrimaryContent[At - 1] == '\n')
						{
							const int32 NL = PrimaryContent.Find(
								TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
							if (NL != INDEX_NONE)
							{
								const bool bHasCR = NL > 0 && PrimaryContent[NL - 1] == '\r';
								PrimaryContent = PrimaryContent.Left(At)
									+ Prefix + Value
									+ (bHasCR ? TEXT("\r\n") : TEXT("\n"))
									+ PrimaryContent.Mid(NL + 1);
							}
							return;
						}
						From = At + 1;
					}
				};

				PatchLine(TEXT("BotToken"),                      Config.BotToken);
				PatchLine(TEXT("ChannelId"),                     Config.ChannelId);
				PatchLine(TEXT("ServerName"),                    Config.ServerName);
				PatchLine(TEXT("GameToDiscordFormat"),           Config.GameToDiscordFormat);
				PatchLine(TEXT("DiscordToGameFormat"),           Config.DiscordToGameFormat);
				PatchLine(TEXT("IgnoreBotMessages"),             Config.bIgnoreBotMessages ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("ServerStatusMessagesEnabled"),   Config.bServerStatusMessagesEnabled ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("StatusChannelId"),               Config.StatusChannelId);
				PatchLine(TEXT("ServerOnlineMessage"),           Config.ServerOnlineMessage);
				PatchLine(TEXT("ServerOfflineMessage"),          Config.ServerOfflineMessage);
				PatchLine(TEXT("ShowPlayerCountInPresence"),     Config.bShowPlayerCountInPresence ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("PlayerCountPresenceFormat"),     Config.PlayerCountPresenceFormat);
				PatchLine(TEXT("PlayerCountUpdateIntervalSeconds"), *FString::SanitizeFloat(Config.PlayerCountUpdateIntervalSeconds));
				PatchLine(TEXT("PlayerCountActivityType"),       *FString::FromInt(Config.PlayerCountActivityType));
				PatchLine(TEXT("WhitelistEnabled"),              Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("WhitelistCommandRoleId"),        Config.WhitelistCommandRoleId);
				PatchLine(TEXT("WhitelistCommandPrefix"),        Config.WhitelistCommandPrefix);
				PatchLine(TEXT("WhitelistRoleId"),               Config.WhitelistRoleId);
				PatchLine(TEXT("WhitelistChannelId"),            Config.WhitelistChannelId);
				PatchLine(TEXT("WhitelistKickDiscordMessage"),   Config.WhitelistKickDiscordMessage);
				PatchLine(TEXT("WhitelistKickReason"),           Config.WhitelistKickReason);
				PatchLine(TEXT("InGameWhitelistCommandPrefix"),  Config.InGameWhitelistCommandPrefix);
				PatchLine(TEXT("BanSystemEnabled"),              Config.bBanSystemEnabled ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("BanCommandRoleId"),              Config.BanCommandRoleId);
				PatchLine(TEXT("BanCommandPrefix"),              Config.BanCommandPrefix);
				PatchLine(TEXT("BanChannelId"),                  Config.BanChannelId);
				PatchLine(TEXT("BanCommandsEnabled"),            Config.bBanCommandsEnabled ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("BanKickDiscordMessage"),         Config.BanKickDiscordMessage);
				PatchLine(TEXT("BanKickReason"),                 Config.BanKickReason);
				PatchLine(TEXT("InGameBanCommandPrefix"),        Config.InGameBanCommandPrefix);
				PatchLine(TEXT("BanSystemSteamBanDiscordMessage"),   Config.BanSystemSteamBanDiscordMessage);
				PatchLine(TEXT("BanSystemSteamUnbanDiscordMessage"), Config.BanSystemSteamUnbanDiscordMessage);
				PatchLine(TEXT("BanSystemEOSBanDiscordMessage"),     Config.BanSystemEOSBanDiscordMessage);
				PatchLine(TEXT("BanSystemEOSUnbanDiscordMessage"),   Config.BanSystemEOSUnbanDiscordMessage);

				if (FFileHelper::SaveStringToFile(PrimaryContent, *ModFilePath))
				{
					UE_LOG(LogTemp, Log,
					       TEXT("DiscordBridge: Updated primary config at '%s' with all restored settings."),
					       *ModFilePath);
				}
				else
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("DiscordBridge: Could not write restored settings back to '%s'. "
					            "The bridge will still function using the backup."),
					       *ModFilePath);
				}
			}
		}
	}

	// ── Step 3: write up-to-date backup ─────────────────────────────────────────
	// Write a single backup file with all settings on every server start so they
	// survive the next mod update.  The backup is written even when BotToken/
	// ChannelId are still blank so settings like message formats are preserved
	// regardless of credential state.
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field, append a line
	// for it here so it is included in the backup that survives mod updates.
	{
		// NOTE: String concatenation is used intentionally instead of FString::Printf
		// to prevent user-configured values that contain '%' characters (e.g.
		// %PlayerName%, %Message%, %PlayerCount%) from being misinterpreted as
		// printf format specifiers, which would corrupt the written file.
		const FString BackupContent =
			FString(TEXT("[DiscordBridge]\n"))
			+ TEXT("# Auto-generated backup of ") + ModFilePath + TEXT("\n")
			+ TEXT("# This file is read automatically when the primary config is missing credentials.\n")
			+ TEXT("BotToken=") + Config.BotToken + TEXT("\n")
			+ TEXT("ChannelId=") + Config.ChannelId + TEXT("\n")
			+ TEXT("ServerName=") + Config.ServerName + TEXT("\n")
			+ TEXT("GameToDiscordFormat=") + Config.GameToDiscordFormat + TEXT("\n")
			+ TEXT("DiscordToGameFormat=") + Config.DiscordToGameFormat + TEXT("\n")
			+ TEXT("IgnoreBotMessages=") + (Config.bIgnoreBotMessages ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("ServerStatusMessagesEnabled=") + (Config.bServerStatusMessagesEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("StatusChannelId=") + Config.StatusChannelId + TEXT("\n")
			+ TEXT("ServerOnlineMessage=") + Config.ServerOnlineMessage + TEXT("\n")
			+ TEXT("ServerOfflineMessage=") + Config.ServerOfflineMessage + TEXT("\n")
			+ TEXT("ShowPlayerCountInPresence=") + (Config.bShowPlayerCountInPresence ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("PlayerCountPresenceFormat=") + Config.PlayerCountPresenceFormat + TEXT("\n")
			+ TEXT("PlayerCountUpdateIntervalSeconds=") + FString::SanitizeFloat(Config.PlayerCountUpdateIntervalSeconds) + TEXT("\n")
			+ TEXT("PlayerCountActivityType=") + FString::FromInt(Config.PlayerCountActivityType) + TEXT("\n")
			+ TEXT("WhitelistEnabled=") + (Config.bWhitelistEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("WhitelistCommandRoleId=") + Config.WhitelistCommandRoleId + TEXT("\n")
			+ TEXT("WhitelistCommandPrefix=") + Config.WhitelistCommandPrefix + TEXT("\n")
			+ TEXT("WhitelistRoleId=") + Config.WhitelistRoleId + TEXT("\n")
			+ TEXT("WhitelistChannelId=") + Config.WhitelistChannelId + TEXT("\n")
			+ TEXT("WhitelistKickDiscordMessage=") + Config.WhitelistKickDiscordMessage + TEXT("\n")
			+ TEXT("WhitelistKickReason=") + Config.WhitelistKickReason + TEXT("\n")
			+ TEXT("InGameWhitelistCommandPrefix=") + Config.InGameWhitelistCommandPrefix + TEXT("\n")
			+ TEXT("BanSystemEnabled=") + (Config.bBanSystemEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("BanCommandRoleId=") + Config.BanCommandRoleId + TEXT("\n")
			+ TEXT("BanCommandPrefix=") + Config.BanCommandPrefix + TEXT("\n")
			+ TEXT("BanChannelId=") + Config.BanChannelId + TEXT("\n")
			+ TEXT("BanCommandsEnabled=") + (Config.bBanCommandsEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("BanKickDiscordMessage=") + Config.BanKickDiscordMessage + TEXT("\n")
			+ TEXT("BanKickReason=") + Config.BanKickReason + TEXT("\n")
			+ TEXT("InGameBanCommandPrefix=") + Config.InGameBanCommandPrefix + TEXT("\n")
			+ TEXT("BanSystemSteamBanDiscordMessage=") + Config.BanSystemSteamBanDiscordMessage + TEXT("\n")
			+ TEXT("BanSystemSteamUnbanDiscordMessage=") + Config.BanSystemSteamUnbanDiscordMessage + TEXT("\n")
			+ TEXT("BanSystemEOSBanDiscordMessage=") + Config.BanSystemEOSBanDiscordMessage + TEXT("\n")
			+ TEXT("BanSystemEOSUnbanDiscordMessage=") + Config.BanSystemEOSUnbanDiscordMessage + TEXT("\n");

		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(BackupFilePath));

		if (FFileHelper::SaveStringToFile(BackupContent, *BackupFilePath))
		{
			if (Config.BotToken.IsEmpty())
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Wrote backup config at '%s' (credentials not yet configured)."),
				       *BackupFilePath);
			}
			else
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Updated backup config at '%s'."), *BackupFilePath);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("DiscordBridge: Could not write backup config to '%s'."),
			       *BackupFilePath);
		}
	}

	return Config;
}
