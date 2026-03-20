// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridgeConfig.h"
#include "DiscordBridge.h"

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
			if (Trimmed.IsEmpty() || Trimmed.StartsWith(TEXT(";")))
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

	// Parses all values for a given key from a raw INI file content string.
	// Unlike ParseRawIniSection (which uses a map and keeps only the last value),
	// this function returns every occurrence of "Key=Value" so multi-value
	// settings like TicketReason= can have more than one entry.
	TArray<FString> ParseRawIniArray(const FString& RawContent, const FString& Key)
	{
		TArray<FString> Results;
		const FString Prefix = Key + TEXT("=");

		int32 Pos = 0;
		while (Pos < RawContent.Len())
		{
			int32 LineEnd = RawContent.Find(
				TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Pos);
			const bool bLastLine = (LineEnd == INDEX_NONE);
			if (bLastLine)
				LineEnd = RawContent.Len();

			FString Line = RawContent.Mid(Pos, LineEnd - Pos);
			if (!Line.IsEmpty() && Line[Line.Len() - 1] == TEXT('\r'))
				Line.RemoveAt(Line.Len() - 1, 1, /*bAllowShrinking=*/false);

			Pos = bLastLine ? RawContent.Len() : (LineEnd + 1);

			const FString Trimmed = Line.TrimStart();
			if (Trimmed.StartsWith(Prefix))
			{
				FString Value = Trimmed.Mid(Prefix.Len()).TrimEnd();
				if (!Value.IsEmpty())
					Results.Add(Value);
			}
		}

		return Results;
	}
} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
// FDiscordBridgeConfig
// ─────────────────────────────────────────────────────────────────────────────

TArray<FString> FDiscordBridgeConfig::ParseChannelIds(const FString& Raw)
{
	TArray<FString> Ids;
	if (Raw.IsEmpty())
	{
		return Ids;
	}
	Raw.ParseIntoArray(Ids, TEXT(","), /*bCullEmpty=*/false);
	for (FString& Id : Ids)
	{
		Id = Id.TrimStartAndEnd();
	}
	Ids.RemoveAll([](const FString& S) { return S.IsEmpty(); });
	return Ids;
}

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

FString FDiscordBridgeConfig::GetWhitelistConfigFilePath()
{
	return FPaths::Combine(FPaths::ProjectDir(),
	                       TEXT("Mods"), TEXT("DiscordBridge"),
	                       TEXT("Config"), TEXT("DefaultWhitelist.ini"));
}

FString FDiscordBridgeConfig::GetBanConfigFilePath()
{
	return FPaths::Combine(FPaths::ProjectDir(),
	                       TEXT("Mods"), TEXT("DiscordBridge"),
	                       TEXT("Config"), TEXT("DefaultBan.ini"));
}

FString FDiscordBridgeConfig::GetTicketConfigFilePath()
{
	return FPaths::Combine(FPaths::ProjectDir(),
	                       TEXT("Mods"), TEXT("DiscordBridge"),
	                       TEXT("Config"), TEXT("DefaultTickets.ini"));
}

FString FDiscordBridgeConfig::GetBackupConfigFilePath()
{
	// Backup for DefaultDiscordBridge.ini – in Saved/Config/ (never touched by
	// Alpakit mod updates).  On a deployed server:
	//   <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("DiscordBridge.ini"));
}

FString FDiscordBridgeConfig::GetWhitelistBackupFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("Whitelist.ini"));
}

FString FDiscordBridgeConfig::GetBanBackupFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("Ban.ini"));
}

FString FDiscordBridgeConfig::GetTicketBackupFilePath()
{
	return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config"), TEXT("Tickets.ini"));
}

FDiscordBridgeConfig FDiscordBridgeConfig::LoadOrCreate()
{
	FDiscordBridgeConfig Config;
	const FString ModFilePath          = GetModConfigFilePath();
	const FString WhitelistFilePath    = GetWhitelistConfigFilePath();
	const FString BanFilePath          = GetBanConfigFilePath();
	const FString TicketFilePath       = GetTicketConfigFilePath();
	const FString BackupFilePath       = GetBackupConfigFilePath();
	const FString WhitelistBackupPath  = GetWhitelistBackupFilePath();
	const FString BanBackupPath        = GetBanBackupFilePath();
	const FString TicketBackupPath     = GetTicketBackupFilePath();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	// Helper lambda: load the [DiscordBridge] section of a raw INI backup file
	// into a key→value map.  Returns an empty map when the file does not exist.
	auto LoadBackup = [&](const FString& Path) -> TMap<FString, FString>
	{
		TMap<FString, FString> Result;
		if (PlatformFile.FileExists(*Path))
		{
			FString Content;
			FFileHelper::LoadFileToString(Content, *Path);
			Result = ParseRawIniSection(Content, TEXT("DiscordBridge"));
		}
		return Result;
	};

	// Pre-load all backup files once so they can be used without re-reading
	// the same file multiple times throughout LoadOrCreate().
	const TMap<FString, FString> MainBackupValues     = LoadBackup(BackupFilePath);
	const TMap<FString, FString> WhitelistBackupRaw   = LoadBackup(WhitelistBackupPath);
	const TMap<FString, FString> BanBackupRaw         = LoadBackup(BanBackupPath);
	const TMap<FString, FString> TicketBackupRaw      = LoadBackup(TicketBackupPath);

	// For each secondary feature, if its own backup does not exist yet (e.g. the
	// server is upgrading from a pre-split install), fall back to the old combined
	// DiscordBridge.ini backup which contains all settings in one place.
	const TMap<FString, FString>& WhitelistBackupValues = WhitelistBackupRaw.IsEmpty() ? MainBackupValues : WhitelistBackupRaw;
	const TMap<FString, FString>& BanBackupValues       = BanBackupRaw.IsEmpty()       ? MainBackupValues : BanBackupRaw;
	const TMap<FString, FString>& TicketBackupValues    = TicketBackupRaw.IsEmpty()    ? MainBackupValues : TicketBackupRaw;

	// ── Step 1a: load the primary config (mod folder) ────────────────────────
	// Contains: connection, chat, server status, presence, server info,
	// server status notifications, and server control settings.
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field to the main
	// config, add a read line here AND update Steps 2 (restore + PatchLine)
	// and 3 (backup write).
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

		Config.bIgnoreBotMessages   = GetIniBoolOrDefault  (ConfigFile, TEXT("IgnoreBotMessages"),
		                              GetIniBoolOrDefault  (ConfigFile, TEXT("bIgnoreBotMessages"),   Config.bIgnoreBotMessages));
		Config.bServerStatusMessagesEnabled = GetIniBoolOrDefault(ConfigFile, TEXT("ServerStatusMessagesEnabled"), Config.bServerStatusMessagesEnabled);
		Config.StatusChannelId      = GetIniStringOrDefault(ConfigFile, TEXT("StatusChannelId"),      Config.StatusChannelId);
		Config.ServerOnlineMessage  = GetIniStringOrDefault(ConfigFile, TEXT("ServerOnlineMessage"),  Config.ServerOnlineMessage);
		Config.ServerOfflineMessage = GetIniStringOrDefault(ConfigFile, TEXT("ServerOfflineMessage"), Config.ServerOfflineMessage);
		Config.ServerCrashMessage   = GetIniStringOrDefault(ConfigFile, TEXT("ServerCrashMessage"),   Config.ServerCrashMessage);
		Config.PlayerJoinMessage              = GetIniStringOrDefault(ConfigFile, TEXT("PlayerJoinMessage"),              Config.PlayerJoinMessage);
		Config.PlayerLeaveMessage             = GetIniStringOrDefault(ConfigFile, TEXT("PlayerLeaveMessage"),             Config.PlayerLeaveMessage);
		Config.PlayerConnectingMessage        = GetIniStringOrDefault(ConfigFile, TEXT("PlayerConnectingMessage"),        Config.PlayerConnectingMessage);
		Config.PlayerConnectionDroppedMessage = GetIniStringOrDefault(ConfigFile, TEXT("PlayerConnectionDroppedMessage"), Config.PlayerConnectionDroppedMessage);
		Config.bShowPlayerCountInPresence      = GetIniBoolOrDefault  (ConfigFile, TEXT("ShowPlayerCountInPresence"),
		                                         GetIniBoolOrDefault  (ConfigFile, TEXT("bShowPlayerCountInPresence"),      Config.bShowPlayerCountInPresence));
		Config.PlayerCountPresenceFormat       = GetIniStringOrFallback(ConfigFile, TEXT("PlayerCountPresenceFormat"),       Config.PlayerCountPresenceFormat);
		Config.PlayerCountUpdateIntervalSeconds = GetIniFloatOrDefault (ConfigFile, TEXT("PlayerCountUpdateIntervalSeconds"), Config.PlayerCountUpdateIntervalSeconds);
		Config.PlayerCountActivityType         = GetIniIntOrDefault   (ConfigFile, TEXT("PlayerCountActivityType"),         Config.PlayerCountActivityType);
		Config.ServerInfoCommandPrefix         = GetIniStringOrDefault(ConfigFile, TEXT("ServerInfoCommandPrefix"),         Config.ServerInfoCommandPrefix);
		Config.InGameServerInfoCommandPrefix   = GetIniStringOrDefault(ConfigFile, TEXT("InGameServerInfoCommandPrefix"),   Config.InGameServerInfoCommandPrefix);
		Config.ServerStatusNotifyRoleId        = GetIniStringOrDefault(ConfigFile, TEXT("ServerStatusNotifyRoleId"),        Config.ServerStatusNotifyRoleId);
		Config.ServerControlCommandPrefix      = GetIniStringOrDefault(ConfigFile, TEXT("ServerControlCommandPrefix"),      Config.ServerControlCommandPrefix);
		Config.ServerControlCommandRoleId      = GetIniStringOrDefault(ConfigFile, TEXT("ServerControlCommandRoleId"),      Config.ServerControlCommandRoleId);

		// Trim leading/trailing whitespace from credential fields to prevent
		// subtle mismatches when operators accidentally include spaces.
		Config.BotToken  = Config.BotToken.TrimStartAndEnd();
		Config.ChannelId = Config.ChannelId.TrimStartAndEnd();

		bLoadedFromMod = true;
		UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Loaded config from %s"), *ModFilePath);

		// When BotToken is empty the file may not have been configured yet.
		// Only rewrite with the annotated template when the file has no ';'
		// comment lines, which indicates Alpakit stripped them during packaging
		// and the operator cannot see the setting descriptions.  If the file
		// already contains ';' characters it is either a previously-written
		// annotated template or a user-configured file; in that case do NOT
		// overwrite it so the operator's other settings (server name, online/
		// offline messages, format strings, etc.) are preserved even when
		// BotToken has not been filled in yet.
		if (Config.BotToken.IsEmpty())
		{
			FString ModFileRaw;
			FFileHelper::LoadFileToString(ModFileRaw, *ModFilePath);
			if (!ModFileRaw.Contains(TEXT(";")))
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Config at '%s' has no BotToken and no comment "
				            "lines (Alpakit-stripped or comment-free) – rewriting with "
				            "annotated template so operators can see setting descriptions."),
				       *ModFilePath);
				bLoadedFromMod = false; // fall through to the DefaultContent write below
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Config at '%s' has no BotToken – "
				            "Discord bridge will not start until BotToken is configured."),
				       *ModFilePath);
				// bLoadedFromMod stays true; all other settings loaded above are used.
			}
		}
	}

	if (!bLoadedFromMod)
	{
		if (!PlatformFile.FileExists(*ModFilePath))
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Config file not found at '%s'. "
			            "Creating it with defaults."), *ModFilePath);
		}

		const FString DefaultContent =
			TEXT("[DiscordBridge]\n")
			TEXT("; DiscordBridge - Primary Configuration File\n")
			TEXT("; ===========================================\n")
			TEXT("; 1. Set BotToken and ChannelId below.\n")
			TEXT("; 2. Restart the server. The bridge starts automatically.\n")
			TEXT("; Backup: <ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini (auto-saved)\n")
			TEXT(";   The mod backs up your settings automatically. After a mod update that\n")
			TEXT(";   resets this file, settings are restored here on the next server start.\n")
			TEXT("; Bot setup: Discord Developer Portal -> your app -> Bot\n")
			TEXT(";   - Enable Presence, Server Members and Message Content intents.\n")
			TEXT(";   - Invite the bot with Send Messages + Read Message History permissions.\n")
			TEXT(";   - Enable Developer Mode in Discord, right-click the channel, Copy Channel ID.\n")
			TEXT(";\n")
			TEXT("; Other config files in this same folder:\n")
			TEXT(";   DefaultWhitelist.ini  - whitelist settings\n")
			TEXT(";   DefaultBan.ini        - ban system settings\n")
			TEXT(";   DefaultTickets.ini    - ticket system settings (incl. custom ticket reasons)\n")
			TEXT("\n")
			TEXT("; -- CONNECTION ---------------------------------------------------------------\n")
			TEXT("; Discord bot token (Bot -> Token in Developer Portal). Treat as a password.\n")
			TEXT("BotToken=\n")
			TEXT("; Discord channel ID to bridge with in-game chat.\n")
			TEXT("ChannelId=\n")
			TEXT("; Display name for this server. Used as the %ServerName% placeholder.\n")
			TEXT("ServerName=\n")
			TEXT("\n")
			TEXT("; -- CHAT CUSTOMISATION -------------------------------------------------------\n")
			TEXT("; Format for game -> Discord. Placeholders: %ServerName%, %PlayerName%, %Message%\n")
			TEXT("; Default: **%PlayerName%**: %Message%\n")
			TEXT("GameToDiscordFormat=\n")
			TEXT("; Format for Discord -> game. Placeholders: %Username%, %PlayerName%, %Message%\n")
			TEXT("; Default: [Discord] %Username%: %Message%\n")
			TEXT("DiscordToGameFormat=\n")
			TEXT("\n")
			TEXT("; -- BEHAVIOUR ----------------------------------------------------------------\n")
			TEXT("; When True, messages from bot accounts are ignored (prevents echo loops).\n")
			TEXT("; Default: True\n")
			TEXT("IgnoreBotMessages=\n")
			TEXT("\n")
			TEXT("; -- SERVER STATUS MESSAGES ---------------------------------------------------\n")
			TEXT("; Master on/off switch for server status messages. Default: True\n")
			TEXT("; Set to False to disable all online/offline notifications.\n")
			TEXT("ServerStatusMessagesEnabled=True\n")
			TEXT("; Discord channel ID of a dedicated channel for server status messages.\n")
			TEXT("; Leave empty to post status messages to the main bridged channel (ChannelId).\n")
			TEXT("StatusChannelId=\n")
			TEXT("; Message posted to Discord when the server starts. Leave empty to disable.\n")
			TEXT("ServerOnlineMessage=:green_circle: Server is now **online**!\n")
			TEXT("; Message posted to Discord when the server stops. Leave empty to disable.\n")
			TEXT("ServerOfflineMessage=:red_circle: Server is now **offline**.\n")
			TEXT("; Message posted to Discord when the server crashes unexpectedly.\n")
			TEXT("; Leave empty to disable. Default: :warning: **Server crashed!** ...\n")
			TEXT("ServerCrashMessage=:warning: **Server crashed!** The server encountered an unexpected error.\n")
			TEXT("\n")
			TEXT("; -- PLAYER JOIN / LEAVE NOTIFICATIONS ----------------------------------------\n")
			TEXT("; Message posted to Discord when a player joins the server. Placeholder: %PlayerName%\n")
			TEXT("; Leave empty to disable. Default: :arrow_right: **%PlayerName%** joined the server.\n")
			TEXT("PlayerJoinMessage=:arrow_right: **%PlayerName%** joined the server.\n")
			TEXT("; Message posted to Discord when a player leaves the server (including timeouts).\n")
			TEXT("; Placeholder: %PlayerName%. Leave empty to disable.\n")
			TEXT("; Default: :arrow_left: **%PlayerName%** left the server.\n")
			TEXT("PlayerLeaveMessage=:arrow_left: **%PlayerName%** left the server.\n")
			TEXT("; Message posted to Discord when a new network connection is detected before\n")
			TEXT("; login completes. Placeholder: %RemoteAddr% (IP:port).\n")
			TEXT("; Leave empty (default) to disable this notification.\n")
			TEXT("; Example: PlayerConnectingMessage=:satellite: New connection from %RemoteAddr%\n")
			TEXT("PlayerConnectingMessage=\n")
			TEXT("; Message posted to Discord when a pre-login connection is dropped.\n")
			TEXT("; Placeholder: %RemoteAddr% (IP:port). Leave empty (default) to disable.\n")
			TEXT("; Example: PlayerConnectionDroppedMessage=:warning: Connection from %RemoteAddr% dropped before login.\n")
			TEXT("PlayerConnectionDroppedMessage=\n")
			TEXT("\n")
			TEXT("; -- PLAYER COUNT PRESENCE ----------------------------------------------------\n")
			TEXT("; When True, the bot's Discord status shows the current player count.\n")
			TEXT("; Default: True\n")
			TEXT("ShowPlayerCountInPresence=\n")
			TEXT("; Text shown in the bot's Discord presence. Placeholders: %PlayerCount%, %ServerName%\n")
			TEXT("; Default: Satisfactory with %PlayerCount% players\n")
			TEXT("PlayerCountPresenceFormat=\n")
			TEXT("; How often (in seconds) to refresh the player count. Minimum 15. Default 60.\n")
			TEXT("PlayerCountUpdateIntervalSeconds=\n")
			TEXT("; Activity type: 0=Playing, 2=Listening to, 3=Watching, 5=Competing in. Default 0.\n")
			TEXT("PlayerCountActivityType=\n")
			TEXT("\n")
			TEXT("; -- SERVER INFO COMMANDS -----------------------------------------------------\n")
			TEXT("; Prefix for server-information commands in the bridged Discord channel.\n")
			TEXT("; These commands are open to all channel members (no role required).\n")
			TEXT("; Set to empty to disable server-info commands entirely.\n")
			TEXT("; Supported: !server players, !server status, !server eos, !server help\n")
			TEXT("; Default: !server\n")
			TEXT("ServerInfoCommandPrefix=!server\n")
			TEXT("; Prefix for server-information commands in the in-game chat.\n")
			TEXT("; Set to empty to disable in-game server-info commands.\n")
			TEXT("; Supported: !server players, !server status, !server eos, !server help\n")
			TEXT("; Default: !server\n")
			TEXT("InGameServerInfoCommandPrefix=!server\n")
			TEXT("\n")
			TEXT("; -- SERVER STATUS NOTIFICATIONS ----------------------------------------------\n")
			TEXT("; Discord role ID to @mention in every server online/offline\n")
			TEXT("; status message (ServerOnlineMessage / ServerOfflineMessage).\n")
			TEXT("; Leave empty to post status messages without any @mention.\n")
			TEXT("; Example: ServerStatusNotifyRoleId=123456789012345678\n")
			TEXT("ServerStatusNotifyRoleId=\n")
			TEXT("\n")
			TEXT("; -- SERVER CONTROL COMMANDS -------------------------------------------------\n")
			TEXT("; Admin-only commands that allow authorised Discord members to stop or\n")
			TEXT("; restart the server directly from the bridged Discord channel.\n")
			TEXT("; IMPORTANT: set ServerControlCommandRoleId to a Discord admin role ID.\n")
			TEXT("; When ServerControlCommandRoleId is empty, ALL control commands are\n")
			TEXT("; disabled regardless of the prefix setting (deny-by-default).\n")
			TEXT(";\n")
			TEXT("; Prefix for server control commands in the bridged Discord channel.\n")
			TEXT("; Set to empty to disable the command group entirely.\n")
			TEXT("; Supported commands:\n")
			TEXT(";   !admin start   - confirm the server is already online\n")
			TEXT(";   !admin stop    - gracefully shut down the server process\n")
			TEXT(";   !admin restart - shut down so the process manager can restart it\n")
			TEXT("; Default: !admin\n")
			TEXT("ServerControlCommandPrefix=!admin\n")
			TEXT(";\n")
			TEXT("; Discord role ID whose members may run !admin commands.\n")
			TEXT("; Leave empty (default) to disable these commands for all Discord users.\n")
			TEXT("; How to get it: Discord Settings -> Advanced -> Developer Mode, then\n")
			TEXT("; right-click the role in Server Settings -> Roles -> Copy Role ID.\n")
			TEXT("; Example: ServerControlCommandRoleId=123456789012345678\n")
			TEXT("ServerControlCommandRoleId=\n");

		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(ModFilePath));
		if (FFileHelper::SaveStringToFile(DefaultContent, *ModFilePath,
		                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Wrote default config to '%s'. "
			            "Set BotToken and ChannelId in that file, then restart "
			            "the server to enable the Discord bridge."), *ModFilePath);
		}
		else
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Could not write default config to '%s'."),
			       *ModFilePath);
		}
	}

	// ── Helper: write an INI file for a secondary feature config ─────────────
	// Generates the annotated content string, substitutes actual Config values
	// using a line-level replace (same PatchLine pattern as the main restore),
	// then writes to disk.  Called for whitelist / ban / ticket files when they
	// are missing or when Alpakit stripped their comment lines.
	auto WriteSecondaryFile = [&](const FString& FilePath, FString Content,
	                              TFunction<void(TFunction<void(const TCHAR*, const FString&)>)> PatchFn)
	{
		auto PatchLine = [&Content](const TCHAR* Key, const FString& Value)
		{
			const FString Prefix = FString(Key) + TEXT("=");
			for (int32 From = 0;;)
			{
				const int32 At = Content.Find(
					Prefix, ESearchCase::CaseSensitive, ESearchDir::FromStart, From);
				if (At == INDEX_NONE)
					break;
				if (At == 0 || Content[At - 1] == TEXT('\n'))
				{
					const int32 NL = Content.Find(
						TEXT("\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, At);
					if (NL != INDEX_NONE)
					{
						const bool bHasCR = NL > 0 && Content[NL - 1] == TEXT('\r');
						Content = Content.Left(At)
							+ Prefix + Value
							+ (bHasCR ? TEXT("\r\n") : TEXT("\n"))
							+ Content.Mid(NL + 1);
					}
					return;
				}
				From = At + 1;
			}
		};
		PatchFn(PatchLine);
		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(FilePath));
		FFileHelper::SaveStringToFile(Content, *FilePath,
		                              FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	};

	// ── Step 1b: load the whitelist config (DefaultWhitelist.ini) ────────────
	// [BACKUP-SYNC] When adding a whitelist field: update the read block, the
	// DefaultWhitelistContent template, the PatchFn lambda, and Step 3 backup.
	{
		const bool bWhitelistFileExists = PlatformFile.FileExists(*WhitelistFilePath);
		bool bShouldCreateWhitelistFile = !bWhitelistFileExists;

		if (bWhitelistFileExists)
		{
			FString WhitelistRaw;
			FFileHelper::LoadFileToString(WhitelistRaw, *WhitelistFilePath);
			FConfigFile WhitelistCfg;
			WhitelistCfg.Read(WhitelistFilePath);

			Config.bWhitelistEnabled            = GetIniBoolOrDefault  (WhitelistCfg, TEXT("WhitelistEnabled"),            Config.bWhitelistEnabled);
			Config.WhitelistCommandRoleId        = GetIniStringOrDefault(WhitelistCfg, TEXT("WhitelistCommandRoleId"),       Config.WhitelistCommandRoleId);
			Config.WhitelistCommandPrefix        = GetIniStringOrDefault(WhitelistCfg, TEXT("WhitelistCommandPrefix"),       Config.WhitelistCommandPrefix);
			Config.WhitelistRoleId               = GetIniStringOrDefault(WhitelistCfg, TEXT("WhitelistRoleId"),              Config.WhitelistRoleId);
			Config.WhitelistChannelId            = GetIniStringOrDefault(WhitelistCfg, TEXT("WhitelistChannelId"),           Config.WhitelistChannelId);
			Config.WhitelistKickDiscordMessage   = GetIniStringOrDefault(WhitelistCfg, TEXT("WhitelistKickDiscordMessage"),  Config.WhitelistKickDiscordMessage);
			Config.WhitelistKickReason           = GetIniStringOrFallback(WhitelistCfg, TEXT("WhitelistKickReason"),          Config.WhitelistKickReason);
			Config.InGameWhitelistCommandPrefix  = GetIniStringOrDefault(WhitelistCfg, TEXT("InGameWhitelistCommandPrefix"), Config.InGameWhitelistCommandPrefix);

			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Loaded whitelist config from %s"), *WhitelistFilePath);

			// If Alpakit stripped all comment lines, recreate the annotated template.
			if (!WhitelistRaw.Contains(TEXT(";")))
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Whitelist config at '%s' has no comment lines "
				            "(Alpakit-stripped) – rewriting with annotated template."),
				       *WhitelistFilePath);
				bShouldCreateWhitelistFile = true;
			}
		}
		else
		{
			// File missing – restore user values from the whitelist-specific backup.
			// WhitelistBackupValues already falls back to the old combined backup
			// (MainBackupValues) when the dedicated whitelist backup does not exist,
			// so existing installs upgrading from the pre-split format are handled.
			Config.bWhitelistEnabled            = GetRawBoolOrDefault  (WhitelistBackupValues, TEXT("WhitelistEnabled"),            Config.bWhitelistEnabled);
			Config.WhitelistCommandRoleId        = GetRawStringOrDefault(WhitelistBackupValues, TEXT("WhitelistCommandRoleId"),       Config.WhitelistCommandRoleId);
			Config.WhitelistCommandPrefix        = GetRawStringOrDefault(WhitelistBackupValues, TEXT("WhitelistCommandPrefix"),       Config.WhitelistCommandPrefix);
			Config.WhitelistRoleId               = GetRawStringOrDefault(WhitelistBackupValues, TEXT("WhitelistRoleId"),              Config.WhitelistRoleId);
			Config.WhitelistChannelId            = GetRawStringOrDefault(WhitelistBackupValues, TEXT("WhitelistChannelId"),           Config.WhitelistChannelId);
			Config.WhitelistKickDiscordMessage   = GetRawStringOrDefault(WhitelistBackupValues, TEXT("WhitelistKickDiscordMessage"),  Config.WhitelistKickDiscordMessage);
			Config.WhitelistKickReason           = GetRawStringOrFallback(WhitelistBackupValues, TEXT("WhitelistKickReason"),          Config.WhitelistKickReason);
			Config.InGameWhitelistCommandPrefix  = GetRawStringOrDefault(WhitelistBackupValues, TEXT("InGameWhitelistCommandPrefix"), Config.InGameWhitelistCommandPrefix);

			if (!WhitelistBackupValues.IsEmpty())
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Whitelist config not found at '%s' – restored from backup."), *WhitelistFilePath);
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Whitelist config not found at '%s' – using defaults."), *WhitelistFilePath);
			}
		}

		if (bShouldCreateWhitelistFile)
		{
			const FString DefaultWhitelistContent =
				TEXT("[DiscordBridge]\n")
				TEXT("; DiscordBridge - Whitelist Configuration\n")
				TEXT("; ========================================\n")
				TEXT("; This file controls the built-in server whitelist.\n")
				TEXT("; Backup: <ServerRoot>/FactoryGame/Saved/Config/Whitelist.ini (auto-saved)\n")
				TEXT(";\n")
				TEXT("; -- WHITELIST ----------------------------------------------------------------\n")
				TEXT("; Whether the whitelist is active. Default: False (all players can join).\n")
				TEXT("WhitelistEnabled=False\n")
				TEXT("; Discord role ID whose members may run !whitelist commands.\n")
				TEXT("WhitelistCommandRoleId=\n")
				TEXT("; Prefix for whitelist commands in Discord. Default: !whitelist\n")
				TEXT("WhitelistCommandPrefix=!whitelist\n")
				TEXT("; Discord role ID assigned to whitelisted members.\n")
				TEXT("WhitelistRoleId=\n")
				TEXT("; Discord channel ID of a dedicated channel for whitelisted members.\n")
				TEXT("WhitelistChannelId=\n")
				TEXT("; Message posted to Discord when a non-whitelisted player is kicked.\n")
				TEXT("WhitelistKickDiscordMessage=:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.\n")
				TEXT("; Reason shown in-game to the player when kicked for not being whitelisted.\n")
				TEXT("WhitelistKickReason=\n")
				TEXT("; Prefix for whitelist commands in the in-game chat. Default: !whitelist\n")
				TEXT("InGameWhitelistCommandPrefix=!whitelist\n");

			WriteSecondaryFile(WhitelistFilePath, DefaultWhitelistContent,
				[&](TFunction<void(const TCHAR*, const FString&)> Patch)
				{
					Patch(TEXT("WhitelistEnabled"),           Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("WhitelistCommandRoleId"),     Config.WhitelistCommandRoleId);
					Patch(TEXT("WhitelistCommandPrefix"),     Config.WhitelistCommandPrefix);
					Patch(TEXT("WhitelistRoleId"),            Config.WhitelistRoleId);
					Patch(TEXT("WhitelistChannelId"),         Config.WhitelistChannelId);
					Patch(TEXT("WhitelistKickDiscordMessage"), Config.WhitelistKickDiscordMessage);
					Patch(TEXT("WhitelistKickReason"),        Config.WhitelistKickReason);
					Patch(TEXT("InGameWhitelistCommandPrefix"), Config.InGameWhitelistCommandPrefix);
				});
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Wrote whitelist config to '%s'."), *WhitelistFilePath);
		}
	}

	// ── Step 1c: load the ban config (DefaultBan.ini) ────────────────────────
	// [BACKUP-SYNC] When adding a ban field: update the read block, the
	// DefaultBanContent template, the PatchFn lambda, and Step 3 backup.
	{
		const bool bBanFileExists = PlatformFile.FileExists(*BanFilePath);
		bool bShouldCreateBanFile = !bBanFileExists;

		if (bBanFileExists)
		{
			FString BanRaw;
			FFileHelper::LoadFileToString(BanRaw, *BanFilePath);
			FConfigFile BanCfg;
			BanCfg.Read(BanFilePath);

			Config.bBanSystemEnabled      = GetIniBoolOrDefault  (BanCfg, TEXT("BanSystemEnabled"),   Config.bBanSystemEnabled);
			Config.BanCommandRoleId       = GetIniStringOrDefault(BanCfg, TEXT("BanCommandRoleId"),    Config.BanCommandRoleId);
			Config.BanCommandPrefix       = GetIniStringOrDefault(BanCfg, TEXT("BanCommandPrefix"),    Config.BanCommandPrefix);
			Config.BanChannelId           = GetIniStringOrDefault(BanCfg, TEXT("BanChannelId"),        Config.BanChannelId);
			Config.bBanCommandsEnabled    = GetIniBoolOrDefault  (BanCfg, TEXT("BanCommandsEnabled"),  Config.bBanCommandsEnabled);
			Config.BanKickDiscordMessage  = GetIniStringOrDefault(BanCfg, TEXT("BanKickDiscordMessage"), Config.BanKickDiscordMessage);
			Config.BanKickReason          = GetIniStringOrFallback(BanCfg, TEXT("BanKickReason"),       Config.BanKickReason);
			Config.BanScanIntervalSeconds = GetIniFloatOrDefault  (BanCfg, TEXT("BanScanIntervalSeconds"), Config.BanScanIntervalSeconds);
			Config.InGameBanCommandPrefix = GetIniStringOrDefault(BanCfg, TEXT("InGameBanCommandPrefix"), Config.InGameBanCommandPrefix);
			Config.KickCommandRoleId      = GetIniStringOrDefault(BanCfg, TEXT("KickCommandRoleId"),   Config.KickCommandRoleId);
			Config.KickCommandPrefix      = GetIniStringOrDefault(BanCfg, TEXT("KickCommandPrefix"),   Config.KickCommandPrefix);
			Config.bKickCommandsEnabled   = GetIniBoolOrDefault  (BanCfg, TEXT("KickCommandsEnabled"), Config.bKickCommandsEnabled);
			Config.KickDiscordMessage     = GetIniStringOrDefault(BanCfg, TEXT("KickDiscordMessage"),  Config.KickDiscordMessage);
			Config.KickReason             = GetIniStringOrFallback(BanCfg, TEXT("KickReason"),          Config.KickReason);
			Config.InGameKickCommandPrefix = GetIniStringOrDefault(BanCfg, TEXT("InGameKickCommandPrefix"), Config.InGameKickCommandPrefix);

			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Loaded ban config from %s"), *BanFilePath);

			if (!BanRaw.Contains(TEXT(";")))
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Ban config at '%s' has no comment lines "
				            "(Alpakit-stripped) – rewriting with annotated template."),
				       *BanFilePath);
				bShouldCreateBanFile = true;
			}
		}
		else
		{
			Config.bBanSystemEnabled      = GetRawBoolOrDefault  (BanBackupValues, TEXT("BanSystemEnabled"),      Config.bBanSystemEnabled);
			Config.BanCommandRoleId       = GetRawStringOrDefault(BanBackupValues, TEXT("BanCommandRoleId"),       Config.BanCommandRoleId);
			Config.BanCommandPrefix       = GetRawStringOrDefault(BanBackupValues, TEXT("BanCommandPrefix"),       Config.BanCommandPrefix);
			Config.BanChannelId           = GetRawStringOrDefault(BanBackupValues, TEXT("BanChannelId"),           Config.BanChannelId);
			Config.bBanCommandsEnabled    = GetRawBoolOrDefault  (BanBackupValues, TEXT("BanCommandsEnabled"),    Config.bBanCommandsEnabled);
			Config.BanKickDiscordMessage  = GetRawStringOrDefault(BanBackupValues, TEXT("BanKickDiscordMessage"),  Config.BanKickDiscordMessage);
			Config.BanKickReason          = GetRawStringOrFallback(BanBackupValues, TEXT("BanKickReason"),          Config.BanKickReason);
			Config.BanScanIntervalSeconds = GetRawFloatOrDefault  (BanBackupValues, TEXT("BanScanIntervalSeconds"), Config.BanScanIntervalSeconds);
			Config.InGameBanCommandPrefix = GetRawStringOrDefault(BanBackupValues, TEXT("InGameBanCommandPrefix"), Config.InGameBanCommandPrefix);
			Config.KickCommandRoleId      = GetRawStringOrDefault(BanBackupValues, TEXT("KickCommandRoleId"),      Config.KickCommandRoleId);
			Config.KickCommandPrefix      = GetRawStringOrDefault(BanBackupValues, TEXT("KickCommandPrefix"),      Config.KickCommandPrefix);
			Config.bKickCommandsEnabled   = GetRawBoolOrDefault  (BanBackupValues, TEXT("KickCommandsEnabled"),   Config.bKickCommandsEnabled);
			Config.KickDiscordMessage     = GetRawStringOrDefault(BanBackupValues, TEXT("KickDiscordMessage"),     Config.KickDiscordMessage);
			Config.KickReason             = GetRawStringOrFallback(BanBackupValues, TEXT("KickReason"),             Config.KickReason);
			Config.InGameKickCommandPrefix = GetRawStringOrDefault(BanBackupValues, TEXT("InGameKickCommandPrefix"), Config.InGameKickCommandPrefix);

			if (!BanBackupValues.IsEmpty())
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Ban config not found at '%s' – restored from backup."), *BanFilePath);
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Ban config not found at '%s' – using defaults."), *BanFilePath);
			}
		}

		if (bShouldCreateBanFile)
		{
			const FString DefaultBanContent =
				TEXT("[DiscordBridge]\n")
				TEXT("; DiscordBridge - Ban System Configuration\n")
				TEXT("; =========================================\n")
				TEXT("; This file controls the built-in player ban system.\n")
				TEXT("; Bans are stored in: <ServerRoot>/FactoryGame/Saved/ServerBanlist.json\n")
				TEXT("; Backup: <ServerRoot>/FactoryGame/Saved/Config/Ban.ini (auto-saved)\n")
				TEXT(";\n")
				TEXT("; -- BAN SYSTEM ---------------------------------------------------------------\n")
				TEXT("; Whether the ban system is active. Default: True (ban list is enforced).\n")
				TEXT("BanSystemEnabled=True\n")
				TEXT("; Discord role ID whose members may run !ban commands.\n")
				TEXT("BanCommandRoleId=\n")
				TEXT("; Prefix for ban commands in Discord. Default: !ban\n")
				TEXT("BanCommandPrefix=!ban\n")
				TEXT("; Discord channel ID of a dedicated channel for ban management.\n")
				TEXT("BanChannelId=\n")
				TEXT("; Master on/off switch for ban commands. Default: True\n")
				TEXT("BanCommandsEnabled=True\n")
				TEXT("; Message posted to Discord when a banned player is kicked.\n")
				TEXT("BanKickDiscordMessage=:hammer: **%PlayerName%** is banned from this server and was kicked.\n")
				TEXT("; Reason shown in-game to the player when kicked for being banned.\n")
				TEXT("BanKickReason=\n")
				TEXT("; How often (seconds) to scan connected players for bans. 0 = disabled. Default: 300 (5 min).\n")
				TEXT("BanScanIntervalSeconds=300\n")
				TEXT("; Prefix for ban commands in the in-game chat. Default: !ban\n")
				TEXT("InGameBanCommandPrefix=!ban\n")
				TEXT(";\n")
				TEXT("; -- KICK COMMAND -------------------------------------------------------------\n")
				TEXT("; The kick command kicks a player WITHOUT banning them.\n")
				TEXT("; The player can reconnect immediately after being kicked.\n")
				TEXT(";\n")
				TEXT("; Discord role ID whose members may run !kick commands.\n")
				TEXT("; Leave empty to disable kick commands for everyone (deny-by-default).\n")
				TEXT("KickCommandRoleId=\n")
				TEXT("; Prefix for kick commands in Discord. Default: !kick\n")
				TEXT("; Usage: !kick <PlayerName>            (uses KickReason below)\n")
				TEXT(";        !kick <PlayerName> <reason>   (custom reason in the command)\n")
				TEXT("KickCommandPrefix=!kick\n")
				TEXT("; Master on/off switch for kick commands. Default: True\n")
				TEXT("KickCommandsEnabled=True\n")
				TEXT("; Message posted to Discord when a player is kicked via !kick.\n")
				TEXT("; Placeholders: %PlayerName%, %Reason%\n")
				TEXT("KickDiscordMessage=:boot: **%PlayerName%** was kicked by an admin.\n")
				TEXT("; Default reason shown in-game to a kicked player when no reason is given.\n")
				TEXT("; The player sees this in the Disconnected screen. Leave empty for the built-in default.\n")
				TEXT("KickReason=\n")
				TEXT("; Prefix for kick commands in the in-game chat. Default: !kick\n")
				TEXT("InGameKickCommandPrefix=!kick\n");

			WriteSecondaryFile(BanFilePath, DefaultBanContent,
				[&](TFunction<void(const TCHAR*, const FString&)> Patch)
				{
					Patch(TEXT("BanSystemEnabled"),      Config.bBanSystemEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("BanCommandRoleId"),      Config.BanCommandRoleId);
					Patch(TEXT("BanCommandPrefix"),      Config.BanCommandPrefix);
					Patch(TEXT("BanChannelId"),          Config.BanChannelId);
					Patch(TEXT("BanCommandsEnabled"),    Config.bBanCommandsEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("BanKickDiscordMessage"), Config.BanKickDiscordMessage);
					Patch(TEXT("BanKickReason"),         Config.BanKickReason);
					Patch(TEXT("BanScanIntervalSeconds"), *FString::SanitizeFloat(Config.BanScanIntervalSeconds));
					Patch(TEXT("InGameBanCommandPrefix"), Config.InGameBanCommandPrefix);
					Patch(TEXT("KickCommandRoleId"),     Config.KickCommandRoleId);
					Patch(TEXT("KickCommandPrefix"),     Config.KickCommandPrefix);
					Patch(TEXT("KickCommandsEnabled"),   Config.bKickCommandsEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("KickDiscordMessage"),    Config.KickDiscordMessage);
					Patch(TEXT("KickReason"),            Config.KickReason);
					Patch(TEXT("InGameKickCommandPrefix"), Config.InGameKickCommandPrefix);
				});
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Wrote ban config to '%s'."), *BanFilePath);
		}
	}

	// ── Step 1d: load the ticket config (DefaultTickets.ini) ─────────────────
	// [BACKUP-SYNC] When adding a ticket field: update the read block, the
	// DefaultTicketContent template, the PatchFn lambda, and Step 3 backup.
	{
		const bool bTicketFileExists = PlatformFile.FileExists(*TicketFilePath);
		bool bShouldCreateTicketFile = !bTicketFileExists;

		if (bTicketFileExists)
		{
			FString TicketRaw;
			FFileHelper::LoadFileToString(TicketRaw, *TicketFilePath);
			FConfigFile TicketCfg;
			TicketCfg.Read(TicketFilePath);

			Config.TicketChannelId          = GetIniStringOrDefault(TicketCfg, TEXT("TicketChannelId"),        Config.TicketChannelId);
			Config.bTicketWhitelistEnabled  = GetIniBoolOrDefault  (TicketCfg, TEXT("TicketWhitelistEnabled"), Config.bTicketWhitelistEnabled);
			Config.bTicketHelpEnabled       = GetIniBoolOrDefault  (TicketCfg, TEXT("TicketHelpEnabled"),      Config.bTicketHelpEnabled);
			Config.bTicketReportEnabled     = GetIniBoolOrDefault  (TicketCfg, TEXT("TicketReportEnabled"),    Config.bTicketReportEnabled);
			Config.TicketNotifyRoleId       = GetIniStringOrDefault(TicketCfg, TEXT("TicketNotifyRoleId"),     Config.TicketNotifyRoleId);
			Config.TicketPanelChannelId     = GetIniStringOrDefault(TicketCfg, TEXT("TicketPanelChannelId"),   Config.TicketPanelChannelId);
			Config.TicketCategoryId         = GetIniStringOrDefault(TicketCfg, TEXT("TicketCategoryId"),       Config.TicketCategoryId);

			// Multi-value: read every TicketReason= line from the raw file content.
			Config.CustomTicketReasons = ParseRawIniArray(TicketRaw, TEXT("TicketReason"));

			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Loaded ticket config from %s"), *TicketFilePath);

			if (!TicketRaw.Contains(TEXT(";")))
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Ticket config at '%s' has no comment lines "
				            "(Alpakit-stripped) – rewriting with annotated template."),
				       *TicketFilePath);
				bShouldCreateTicketFile = true;
			}
		}
		else
		{
			Config.TicketChannelId          = GetRawStringOrDefault(TicketBackupValues, TEXT("TicketChannelId"),        Config.TicketChannelId);
			Config.bTicketWhitelistEnabled  = GetRawBoolOrDefault  (TicketBackupValues, TEXT("TicketWhitelistEnabled"), Config.bTicketWhitelistEnabled);
			Config.bTicketHelpEnabled       = GetRawBoolOrDefault  (TicketBackupValues, TEXT("TicketHelpEnabled"),      Config.bTicketHelpEnabled);
			Config.bTicketReportEnabled     = GetRawBoolOrDefault  (TicketBackupValues, TEXT("TicketReportEnabled"),    Config.bTicketReportEnabled);
			Config.TicketNotifyRoleId       = GetRawStringOrDefault(TicketBackupValues, TEXT("TicketNotifyRoleId"),     Config.TicketNotifyRoleId);
			Config.TicketPanelChannelId     = GetRawStringOrDefault(TicketBackupValues, TEXT("TicketPanelChannelId"),   Config.TicketPanelChannelId);
			Config.TicketCategoryId         = GetRawStringOrDefault(TicketBackupValues, TEXT("TicketCategoryId"),       Config.TicketCategoryId);

			// Restore custom ticket reasons from backup (multi-value key).
			{
				FString TicketBackupRawContent;
				if (PlatformFile.FileExists(*TicketBackupPath))
					FFileHelper::LoadFileToString(TicketBackupRawContent, *TicketBackupPath);
				else if (PlatformFile.FileExists(*BackupFilePath))
					FFileHelper::LoadFileToString(TicketBackupRawContent, *BackupFilePath);
				Config.CustomTicketReasons = ParseRawIniArray(TicketBackupRawContent, TEXT("TicketReason"));
			}

			if (!TicketBackupValues.IsEmpty())
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Ticket config not found at '%s' – restored from backup."), *TicketFilePath);
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Ticket config not found at '%s' – using defaults."), *TicketFilePath);
			}
		}

		if (bShouldCreateTicketFile)
		{
			FString DefaultTicketContent =
				TEXT("[DiscordBridge]\n")
				TEXT("; DiscordBridge - Ticket System Configuration\n")
				TEXT("; ============================================\n")
				TEXT("; This file controls the support-ticket system.\n")
				TEXT("; Backup: <ServerRoot>/FactoryGame/Saved/Config/Tickets.ini (auto-saved)\n")
				TEXT(";\n")
				TEXT("; -- TICKET / SUPPORT SYSTEM --------------------------------------------------\n")
				TEXT("; Discord channel ID where submitted tickets are posted. Leave empty for main channel.\n")
				TEXT("TicketChannelId=\n")
				TEXT("; Individual ticket type toggles. Default: True for all three.\n")
				TEXT("TicketWhitelistEnabled=True\n")
				TEXT("TicketHelpEnabled=True\n")
				TEXT("TicketReportEnabled=True\n")
				TEXT("; Discord role ID to @mention in every ticket notification. Leave empty to disable.\n")
				TEXT("; This role also gets view/write access to all created ticket channels.\n")
				TEXT("TicketNotifyRoleId=\n")
				TEXT("; Channel where the !admin ticket-panel button message is posted. Leave empty for main channel.\n")
				TEXT("TicketPanelChannelId=\n")
				TEXT("; Discord category ID to place created ticket channels under. Leave empty for no category.\n")
				TEXT("TicketCategoryId=\n")
				TEXT(";\n")
				TEXT("; -- CUSTOM TICKET REASONS ----------------------------------------------------\n")
				TEXT("; Add custom ticket reason buttons to the panel. Format: TicketReason=Label|Description\n")
				TEXT(";   Label       - text shown on the Discord button (keep short, ~40 chars max)\n")
				TEXT(";   Description - brief summary shown in the panel message body\n")
				TEXT("; You can add as many TicketReason= lines as you need (max 25 total buttons).\n")
				TEXT("; Example:\n")
				TEXT(";   TicketReason=Bug Report|Report a bug or technical issue\n")
				TEXT(";   TicketReason=Suggestion|Submit a suggestion or feature request\n");

			// Append any existing custom reasons so they survive a template rewrite.
			for (const FString& Reason : Config.CustomTicketReasons)
			{
				DefaultTicketContent += TEXT("TicketReason=") + Reason + TEXT("\n");
			}

			WriteSecondaryFile(TicketFilePath, DefaultTicketContent,
				[&](TFunction<void(const TCHAR*, const FString&)> Patch)
				{
					Patch(TEXT("TicketChannelId"),         Config.TicketChannelId);
					Patch(TEXT("TicketWhitelistEnabled"),  Config.bTicketWhitelistEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("TicketHelpEnabled"),       Config.bTicketHelpEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("TicketReportEnabled"),     Config.bTicketReportEnabled ? TEXT("True") : TEXT("False"));
					Patch(TEXT("TicketNotifyRoleId"),      Config.TicketNotifyRoleId);
					Patch(TEXT("TicketPanelChannelId"),    Config.TicketPanelChannelId);
					Patch(TEXT("TicketCategoryId"),        Config.TicketCategoryId);
				});
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Wrote ticket config to '%s'."), *TicketFilePath);
		}
	}

	// ── Step 2: fall back to main backup when credentials are missing ─────────
	// This happens after a mod update resets DefaultDiscordBridge.ini.
	// Only main-config settings (connection/chat/presence/server control) are
	// restored here; whitelist/ban/ticket settings were already restored from
	// their own dedicated backup files in Steps 1b-1d above.
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field to the main
	// config, add a restore line below AND a PatchLine call further down.
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
	    !MainBackupValues.IsEmpty())
	{
		const bool bHadToken   = !Config.BotToken.IsEmpty();
		const bool bHadChannel = !Config.ChannelId.IsEmpty();

		if (Config.BotToken.IsEmpty())
		{
			Config.BotToken = GetRawStringOrDefault(MainBackupValues, TEXT("BotToken"), TEXT("")).TrimStartAndEnd();
		}
		if (Config.ChannelId.IsEmpty())
		{
			Config.ChannelId = GetRawStringOrDefault(MainBackupValues, TEXT("ChannelId"), TEXT("")).TrimStartAndEnd();
		}

		// Restore main-config user-customised settings from the backup so that
		// message formats and status messages also survive a mod update that
		// resets the primary config to its shipped defaults.
		Config.ServerName           = GetRawStringOrDefault(MainBackupValues, TEXT("ServerName"),           Config.ServerName);
		Config.GameToDiscordFormat  = GetRawStringOrFallback(MainBackupValues, TEXT("GameToDiscordFormat"),  Config.GameToDiscordFormat);
		Config.DiscordToGameFormat  = GetRawStringOrFallback(MainBackupValues, TEXT("DiscordToGameFormat"),  Config.DiscordToGameFormat);
		Config.bIgnoreBotMessages   = GetRawBoolOrDefault  (MainBackupValues, TEXT("IgnoreBotMessages"),
		                              GetRawBoolOrDefault  (MainBackupValues, TEXT("bIgnoreBotMessages"),   Config.bIgnoreBotMessages));
		Config.bServerStatusMessagesEnabled = GetRawBoolOrDefault(MainBackupValues, TEXT("ServerStatusMessagesEnabled"), Config.bServerStatusMessagesEnabled);
		Config.StatusChannelId      = GetRawStringOrDefault(MainBackupValues, TEXT("StatusChannelId"),      Config.StatusChannelId);
		Config.ServerOnlineMessage  = GetRawStringOrDefault(MainBackupValues, TEXT("ServerOnlineMessage"),  Config.ServerOnlineMessage);
		Config.ServerOfflineMessage = GetRawStringOrDefault(MainBackupValues, TEXT("ServerOfflineMessage"), Config.ServerOfflineMessage);
		Config.ServerCrashMessage   = GetRawStringOrDefault(MainBackupValues, TEXT("ServerCrashMessage"),   Config.ServerCrashMessage);
		Config.PlayerJoinMessage              = GetRawStringOrDefault(MainBackupValues, TEXT("PlayerJoinMessage"),              Config.PlayerJoinMessage);
		Config.PlayerLeaveMessage             = GetRawStringOrDefault(MainBackupValues, TEXT("PlayerLeaveMessage"),             Config.PlayerLeaveMessage);
		Config.PlayerConnectingMessage        = GetRawStringOrDefault(MainBackupValues, TEXT("PlayerConnectingMessage"),        Config.PlayerConnectingMessage);
		Config.PlayerConnectionDroppedMessage = GetRawStringOrDefault(MainBackupValues, TEXT("PlayerConnectionDroppedMessage"), Config.PlayerConnectionDroppedMessage);
		Config.bShowPlayerCountInPresence       = GetRawBoolOrDefault  (MainBackupValues, TEXT("ShowPlayerCountInPresence"),
		                                          GetRawBoolOrDefault  (MainBackupValues, TEXT("bShowPlayerCountInPresence"),       Config.bShowPlayerCountInPresence));
		Config.PlayerCountPresenceFormat        = GetRawStringOrFallback(MainBackupValues, TEXT("PlayerCountPresenceFormat"),        Config.PlayerCountPresenceFormat);
		Config.PlayerCountUpdateIntervalSeconds = GetRawFloatOrDefault (MainBackupValues, TEXT("PlayerCountUpdateIntervalSeconds"), Config.PlayerCountUpdateIntervalSeconds);
		Config.PlayerCountActivityType          = GetRawIntOrDefault   (MainBackupValues, TEXT("PlayerCountActivityType"),          Config.PlayerCountActivityType);
		Config.ServerInfoCommandPrefix          = GetRawStringOrDefault(MainBackupValues, TEXT("ServerInfoCommandPrefix"),          Config.ServerInfoCommandPrefix);
		Config.InGameServerInfoCommandPrefix    = GetRawStringOrDefault(MainBackupValues, TEXT("InGameServerInfoCommandPrefix"),    Config.InGameServerInfoCommandPrefix);
		Config.ServerStatusNotifyRoleId         = GetRawStringOrDefault(MainBackupValues, TEXT("ServerStatusNotifyRoleId"),         Config.ServerStatusNotifyRoleId);
		Config.ServerControlCommandPrefix       = GetRawStringOrDefault(MainBackupValues, TEXT("ServerControlCommandPrefix"),       Config.ServerControlCommandPrefix);
		Config.ServerControlCommandRoleId       = GetRawStringOrDefault(MainBackupValues, TEXT("ServerControlCommandRoleId"),       Config.ServerControlCommandRoleId);

		// Only log the "restored from backup" message when credentials were
		// actually recovered (i.e. previously blank in primary but now non-empty
		// from the backup). Avoid a misleading message when the backup also has
		// blank credentials (e.g. first server start before credentials are set).
		const bool bRestoredToken   = !bHadToken   && !Config.BotToken.IsEmpty();
		const bool bRestoredChannel = !bHadChannel && !Config.ChannelId.IsEmpty();
		if (bRestoredToken || bRestoredChannel)
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Primary config '%s' had no credentials – "
			            "restored from backup at '%s'. "
			            "Writing main settings back to the primary config so they persist there."),
			       *ModFilePath, *BackupFilePath);

			// Write every restored main setting back into the primary config file
			// so operators always see their current settings there and do not have
			// to re-enter them manually after every mod update.
			// Whitelist/ban/ticket settings are written back to their own separate
			// primary files by the WriteSecondaryFile calls in Steps 1b-1d.
			// [BACKUP-SYNC] When adding a new main-config field, add a PatchLine call here.
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

				PatchLine(TEXT("BotToken"),                         Config.BotToken);
				PatchLine(TEXT("ChannelId"),                        Config.ChannelId);
				PatchLine(TEXT("ServerName"),                       Config.ServerName);
				PatchLine(TEXT("GameToDiscordFormat"),              Config.GameToDiscordFormat);
				PatchLine(TEXT("DiscordToGameFormat"),              Config.DiscordToGameFormat);
				PatchLine(TEXT("IgnoreBotMessages"),                Config.bIgnoreBotMessages ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("ServerStatusMessagesEnabled"),      Config.bServerStatusMessagesEnabled ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("StatusChannelId"),                  Config.StatusChannelId);
				PatchLine(TEXT("ServerOnlineMessage"),              Config.ServerOnlineMessage);
				PatchLine(TEXT("ServerOfflineMessage"),             Config.ServerOfflineMessage);
				PatchLine(TEXT("ServerCrashMessage"),               Config.ServerCrashMessage);
				PatchLine(TEXT("PlayerJoinMessage"),                Config.PlayerJoinMessage);
				PatchLine(TEXT("PlayerLeaveMessage"),               Config.PlayerLeaveMessage);
				PatchLine(TEXT("PlayerConnectingMessage"),          Config.PlayerConnectingMessage);
				PatchLine(TEXT("PlayerConnectionDroppedMessage"),   Config.PlayerConnectionDroppedMessage);
				PatchLine(TEXT("ShowPlayerCountInPresence"),        Config.bShowPlayerCountInPresence ? TEXT("True") : TEXT("False"));
				PatchLine(TEXT("PlayerCountPresenceFormat"),        Config.PlayerCountPresenceFormat);
				PatchLine(TEXT("PlayerCountUpdateIntervalSeconds"), *FString::SanitizeFloat(Config.PlayerCountUpdateIntervalSeconds));
				PatchLine(TEXT("PlayerCountActivityType"),          *FString::FromInt(Config.PlayerCountActivityType));
				PatchLine(TEXT("ServerInfoCommandPrefix"),          Config.ServerInfoCommandPrefix);
				PatchLine(TEXT("InGameServerInfoCommandPrefix"),    Config.InGameServerInfoCommandPrefix);
				PatchLine(TEXT("ServerStatusNotifyRoleId"),         Config.ServerStatusNotifyRoleId);
				PatchLine(TEXT("ServerControlCommandPrefix"),       Config.ServerControlCommandPrefix);
				PatchLine(TEXT("ServerControlCommandRoleId"),       Config.ServerControlCommandRoleId);

				if (FFileHelper::SaveStringToFile(PrimaryContent, *ModFilePath,
				                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
				{
					UE_LOG(LogDiscordBridge, Log,
					       TEXT("DiscordBridge: Updated primary config at '%s' with all restored main settings."),
					       *ModFilePath);
				}
				else
				{
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge: Could not write restored settings back to '%s'. "
					            "The bridge will still function using the backup."),
					       *ModFilePath);
				}
			}
		}
	}

	// ── Step 3: write up-to-date backup files ────────────────────────────────
	// Write four separate backup files (one per config file) on every server
	// start so settings survive the next mod update.  Backups are written even
	// when BotToken/ChannelId are still blank so all other settings are
	// preserved regardless of credential state.
	// [BACKUP-SYNC] When adding a new FDiscordBridgeConfig field, append a line
	// to the appropriate backup content string below.
	//
	// NOTE: String concatenation is used intentionally instead of FString::Printf
	// to prevent user-configured values that contain '%' characters (e.g.
	// %PlayerName%, %Message%, %PlayerCount%) from being misinterpreted as
	// printf format specifiers, which would corrupt the written file.

	// Create the Saved/Config/ directory once (shared by all four backup files).
	PlatformFile.CreateDirectoryTree(*FPaths::GetPath(BackupFilePath));

	// -- Main config backup (Saved/Config/DiscordBridge.ini) ------------------
	{
		const FString BackupContent =
			FString(TEXT("[DiscordBridge]\n"))
			+ TEXT("; Auto-generated backup of DefaultDiscordBridge.ini\n")
			+ TEXT("; This file is read automatically when the primary config is missing credentials.\n")
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
			+ TEXT("ServerCrashMessage=") + Config.ServerCrashMessage + TEXT("\n")
			+ TEXT("PlayerJoinMessage=") + Config.PlayerJoinMessage + TEXT("\n")
			+ TEXT("PlayerLeaveMessage=") + Config.PlayerLeaveMessage + TEXT("\n")
			+ TEXT("PlayerConnectingMessage=") + Config.PlayerConnectingMessage + TEXT("\n")
			+ TEXT("PlayerConnectionDroppedMessage=") + Config.PlayerConnectionDroppedMessage + TEXT("\n")
			+ TEXT("ShowPlayerCountInPresence=") + (Config.bShowPlayerCountInPresence ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("PlayerCountPresenceFormat=") + Config.PlayerCountPresenceFormat + TEXT("\n")
			+ TEXT("PlayerCountUpdateIntervalSeconds=") + FString::SanitizeFloat(Config.PlayerCountUpdateIntervalSeconds) + TEXT("\n")
			+ TEXT("PlayerCountActivityType=") + FString::FromInt(Config.PlayerCountActivityType) + TEXT("\n")
			+ TEXT("ServerInfoCommandPrefix=") + Config.ServerInfoCommandPrefix + TEXT("\n")
			+ TEXT("InGameServerInfoCommandPrefix=") + Config.InGameServerInfoCommandPrefix + TEXT("\n")
			+ TEXT("ServerStatusNotifyRoleId=") + Config.ServerStatusNotifyRoleId + TEXT("\n")
			+ TEXT("ServerControlCommandPrefix=") + Config.ServerControlCommandPrefix + TEXT("\n")
			+ TEXT("ServerControlCommandRoleId=") + Config.ServerControlCommandRoleId + TEXT("\n");

		if (FFileHelper::SaveStringToFile(BackupContent, *BackupFilePath,
		                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			if (Config.BotToken.IsEmpty())
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Wrote main backup at '%s' (credentials not yet configured)."), *BackupFilePath);
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Updated main backup at '%s'."), *BackupFilePath);
			}
		}
		else
		{
			UE_LOG(LogDiscordBridge, Warning, TEXT("DiscordBridge: Could not write main backup to '%s'."), *BackupFilePath);
		}
	}

	// -- Whitelist backup (Saved/Config/Whitelist.ini) -------------------------
	{
		const FString WhitelistBackupContent =
			FString(TEXT("[DiscordBridge]\n"))
			+ TEXT("; Auto-generated backup of DefaultWhitelist.ini\n")
			+ TEXT("WhitelistEnabled=") + (Config.bWhitelistEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("WhitelistCommandRoleId=") + Config.WhitelistCommandRoleId + TEXT("\n")
			+ TEXT("WhitelistCommandPrefix=") + Config.WhitelistCommandPrefix + TEXT("\n")
			+ TEXT("WhitelistRoleId=") + Config.WhitelistRoleId + TEXT("\n")
			+ TEXT("WhitelistChannelId=") + Config.WhitelistChannelId + TEXT("\n")
			+ TEXT("WhitelistKickDiscordMessage=") + Config.WhitelistKickDiscordMessage + TEXT("\n")
			+ TEXT("WhitelistKickReason=") + Config.WhitelistKickReason + TEXT("\n")
			+ TEXT("InGameWhitelistCommandPrefix=") + Config.InGameWhitelistCommandPrefix + TEXT("\n");

		if (FFileHelper::SaveStringToFile(WhitelistBackupContent, *WhitelistBackupPath,
		                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Updated whitelist backup at '%s'."), *WhitelistBackupPath);
		}
		else
		{
			UE_LOG(LogDiscordBridge, Warning, TEXT("DiscordBridge: Could not write whitelist backup to '%s'."), *WhitelistBackupPath);
		}
	}

	// -- Ban backup (Saved/Config/Ban.ini) ------------------------------------
	{
		const FString BanBackupContent =
			FString(TEXT("[DiscordBridge]\n"))
			+ TEXT("; Auto-generated backup of DefaultBan.ini\n")
			+ TEXT("BanSystemEnabled=") + (Config.bBanSystemEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("BanCommandRoleId=") + Config.BanCommandRoleId + TEXT("\n")
			+ TEXT("BanCommandPrefix=") + Config.BanCommandPrefix + TEXT("\n")
			+ TEXT("BanChannelId=") + Config.BanChannelId + TEXT("\n")
			+ TEXT("BanCommandsEnabled=") + (Config.bBanCommandsEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("BanKickDiscordMessage=") + Config.BanKickDiscordMessage + TEXT("\n")
			+ TEXT("BanKickReason=") + Config.BanKickReason + TEXT("\n")
			+ TEXT("BanScanIntervalSeconds=") + FString::SanitizeFloat(Config.BanScanIntervalSeconds) + TEXT("\n")
			+ TEXT("InGameBanCommandPrefix=") + Config.InGameBanCommandPrefix + TEXT("\n")
			+ TEXT("KickCommandRoleId=") + Config.KickCommandRoleId + TEXT("\n")
			+ TEXT("KickCommandPrefix=") + Config.KickCommandPrefix + TEXT("\n")
			+ TEXT("KickCommandsEnabled=") + (Config.bKickCommandsEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("KickDiscordMessage=") + Config.KickDiscordMessage + TEXT("\n")
			+ TEXT("KickReason=") + Config.KickReason + TEXT("\n")
			+ TEXT("InGameKickCommandPrefix=") + Config.InGameKickCommandPrefix + TEXT("\n");

		if (FFileHelper::SaveStringToFile(BanBackupContent, *BanBackupPath,
		                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Updated ban backup at '%s'."), *BanBackupPath);
		}
		else
		{
			UE_LOG(LogDiscordBridge, Warning, TEXT("DiscordBridge: Could not write ban backup to '%s'."), *BanBackupPath);
		}
	}

	// -- Ticket backup (Saved/Config/Tickets.ini) -----------------------------
	{
		FString TicketBackupContent =
			FString(TEXT("[DiscordBridge]\n"))
			+ TEXT("; Auto-generated backup of DefaultTickets.ini\n")
			+ TEXT("TicketChannelId=") + Config.TicketChannelId + TEXT("\n")
			+ TEXT("TicketWhitelistEnabled=") + (Config.bTicketWhitelistEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("TicketHelpEnabled=") + (Config.bTicketHelpEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("TicketReportEnabled=") + (Config.bTicketReportEnabled ? TEXT("True") : TEXT("False")) + TEXT("\n")
			+ TEXT("TicketNotifyRoleId=") + Config.TicketNotifyRoleId + TEXT("\n")
			+ TEXT("TicketPanelChannelId=") + Config.TicketPanelChannelId + TEXT("\n")
			+ TEXT("TicketCategoryId=") + Config.TicketCategoryId + TEXT("\n");

		for (const FString& Reason : Config.CustomTicketReasons)
		{
			TicketBackupContent += TEXT("TicketReason=") + Reason + TEXT("\n");
		}

		if (FFileHelper::SaveStringToFile(TicketBackupContent, *TicketBackupPath,
		                                  FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
		{
			UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Updated ticket backup at '%s'."), *TicketBackupPath);
		}
		else
		{
			UE_LOG(LogDiscordBridge, Warning, TEXT("DiscordBridge: Could not write ticket backup to '%s'."), *TicketBackupPath);
		}
	}

	return Config;
}

bool FDiscordBridgeConfig::HasCredentials()
{
	const FString ModFilePath = GetModConfigFilePath();

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.FileExists(*ModFilePath))
	{
		return false;
	}

	FConfigFile ConfigFile;
	ConfigFile.Read(ModFilePath);

	FString BotToken;
	FString ChannelId;
	ConfigFile.GetString(ConfigSection, TEXT("BotToken"),  BotToken);
	ConfigFile.GetString(ConfigSection, TEXT("ChannelId"), ChannelId);
	BotToken.TrimStartAndEndInline();
	ChannelId.TrimStartAndEndInline();

	return !BotToken.IsEmpty() && !ChannelId.IsEmpty();
}
