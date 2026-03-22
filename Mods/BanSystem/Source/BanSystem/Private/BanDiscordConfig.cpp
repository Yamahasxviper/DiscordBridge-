// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordConfig.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
	static const TCHAR* ConfigSection = TEXT("BanSystem");

	FString GetIniStringOrDefault(const FConfigFile& Cfg,
	                              const FString& Key,
	                              const FString& Default)
	{
		FString Value;
		return Cfg.GetString(ConfigSection, *Key, Value) ? Value : Default;
	}

	FString GetIniStringOrFallback(const FConfigFile& Cfg,
	                               const FString& Key,
	                               const FString& Default)
	{
		FString Value;
		return (Cfg.GetString(ConfigSection, *Key, Value) && !Value.IsEmpty()) ? Value : Default;
	}

	// Default config file content written when no config exists.
	static const TCHAR* DefaultContent = TEXT(
		"[BanSystem]\r\n"
		"# BanSystem - Discord Integration Configuration\r\n"
		"# =============================================\r\n"
		"# These settings control BanSystem's Discord command interface.\r\n"
		"# Requires DiscordBridge (or another IBanDiscordCommandProvider mod) to be installed.\r\n"
		"# This file is NOT overwritten by mod updates, so your settings persist.\r\n"
		"#\r\n"
		"# -- CHANNEL & AUTHORISATION ------------------------------------------\r\n"
		"#\r\n"
		"# Snowflake ID of the Discord channel where BanSystem commands are accepted.\r\n"
		"# Leave empty to disable all BanSystem Discord commands.\r\n"
		"# How to get it: enable Developer Mode in Discord, right-click the channel,\r\n"
		"# 'Copy Channel ID'.\r\n"
		"DiscordChannelId=\r\n"
		"#\r\n"
		"# Snowflake ID of the Discord role allowed to run BanSystem Discord commands.\r\n"
		"# The guild owner can always run these commands regardless of this setting.\r\n"
		"# Leave empty to restrict to guild owner only.\r\n"
		"DiscordCommandRoleId=\r\n"
		"#\r\n"
		"# -- COMMAND PREFIXES -------------------------------------------------\r\n"
		"# Set any prefix to an empty string to disable that command.\r\n"
		"#\r\n"
		"# Ban/unban a player by Steam64 ID or in-game name.\r\n"
		"# Syntax: !steamban <Steam64Id|PlayerName> [duration_minutes] [reason]\r\n"
		"SteamBanCommandPrefix=!steamban\r\n"
		"# Remove a Steam ban.  Syntax: !steamunban <Steam64Id>\r\n"
		"SteamUnbanCommandPrefix=!steamunban\r\n"
		"# List all active Steam bans.  Syntax: !steambanlist\r\n"
		"SteamBanListCommandPrefix=!steambanlist\r\n"
		"#\r\n"
		"# Ban/unban a player by EOS Product User ID or in-game name.\r\n"
		"# Syntax: !eosban <EOSProductUserId|PlayerName> [duration_minutes] [reason]\r\n"
		"EOSBanCommandPrefix=!eosban\r\n"
		"# Remove an EOS ban.  Syntax: !eosunban <EOSProductUserId>\r\n"
		"EOSUnbanCommandPrefix=!eosunban\r\n"
		"# List all active EOS bans.  Syntax: !eosbanlist\r\n"
		"EOSBanListCommandPrefix=!eosbanlist\r\n"
		"#\r\n"
		"# Ban a connected player by name on all platforms at once.\r\n"
		"# Syntax: !banbyname <PlayerName> [duration_minutes] [reason]\r\n"
		"BanByNameCommandPrefix=!banbyname\r\n"
		"#\r\n"
		"# List Steam64 and EOS IDs of connected players.\r\n"
		"# Syntax: !playerids [PlayerName]\r\n"
		"PlayerIdsCommandPrefix=!playerids\r\n"
		"#\r\n"
		"# -- RESPONSE MESSAGE FORMATS -----------------------------------------\r\n"
		"# Leave empty to use the built-in defaults.\r\n"
		"# Placeholders: %PlayerId%  %Reason%  %BannedBy%  %Duration%\r\n"
		"SteamBanResponseMessage=\r\n"
		"SteamUnbanResponseMessage=\r\n"
		"EOSBanResponseMessage=\r\n"
		"EOSUnbanResponseMessage=\r\n"
	);
}

// ─────────────────────────────────────────────────────────────────────────────
// Static helpers
// ─────────────────────────────────────────────────────────────────────────────

FString FBanDiscordConfig::GetConfigFilePath()
{
	// Stored alongside the other mod config files so server operators find it
	// in the same place as DefaultBanSystem.ini.
	return FPaths::ProjectDir() /
	       TEXT("Mods/BanSystem/Config/DefaultBanSystem.ini");
}

// ─────────────────────────────────────────────────────────────────────────────
// LoadOrCreate
// ─────────────────────────────────────────────────────────────────────────────

FBanDiscordConfig FBanDiscordConfig::LoadOrCreate()
{
	const FString ConfigPath = GetConfigFilePath();

	// Create the config file with defaults if it does not exist yet.
	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*ConfigPath))
	{
		FPlatformFileManager::Get().GetPlatformFile()
		    .CreateDirectoryTree(*FPaths::GetPath(ConfigPath));

		if (FFileHelper::SaveStringToFile(DefaultContent, *ConfigPath))
		{
			UE_LOG(LogTemp, Log,
			       TEXT("BanSystem: Created default config at %s"), *ConfigPath);
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("BanSystem: Failed to create default config at %s"), *ConfigPath);
		}
	}

	// Load the INI file.
	FConfigFile Cfg;
	Cfg.Read(ConfigPath);

	FBanDiscordConfig Out;

	// Channel & authorisation
	Out.DiscordChannelId    = GetIniStringOrDefault(Cfg, TEXT("DiscordChannelId"),    TEXT(""));
	Out.DiscordCommandRoleId = GetIniStringOrDefault(Cfg, TEXT("DiscordCommandRoleId"), TEXT(""));

	// Command prefixes
	Out.SteamBanCommandPrefix      = GetIniStringOrDefault(Cfg, TEXT("SteamBanCommandPrefix"),      TEXT("!steamban"));
	Out.SteamUnbanCommandPrefix    = GetIniStringOrDefault(Cfg, TEXT("SteamUnbanCommandPrefix"),    TEXT("!steamunban"));
	Out.SteamBanListCommandPrefix  = GetIniStringOrDefault(Cfg, TEXT("SteamBanListCommandPrefix"),  TEXT("!steambanlist"));
	Out.EOSBanCommandPrefix        = GetIniStringOrDefault(Cfg, TEXT("EOSBanCommandPrefix"),        TEXT("!eosban"));
	Out.EOSUnbanCommandPrefix      = GetIniStringOrDefault(Cfg, TEXT("EOSUnbanCommandPrefix"),      TEXT("!eosunban"));
	Out.EOSBanListCommandPrefix    = GetIniStringOrDefault(Cfg, TEXT("EOSBanListCommandPrefix"),    TEXT("!eosbanlist"));
	Out.BanByNameCommandPrefix     = GetIniStringOrDefault(Cfg, TEXT("BanByNameCommandPrefix"),     TEXT("!banbyname"));
	Out.PlayerIdsCommandPrefix     = GetIniStringOrDefault(Cfg, TEXT("PlayerIdsCommandPrefix"),     TEXT("!playerids"));

	// Response message formats (fall back to defaults when left empty in the INI)
	Out.SteamBanResponseMessage = GetIniStringOrFallback(
		Cfg, TEXT("SteamBanResponseMessage"),
		TEXT(":hammer: **BanSystem** - Steam ID `%PlayerId%` banned by **%BannedBy%** (%Duration%) - Reason: %Reason%"));

	Out.SteamUnbanResponseMessage = GetIniStringOrFallback(
		Cfg, TEXT("SteamUnbanResponseMessage"),
		TEXT(":white_check_mark: **BanSystem** - Steam ID `%PlayerId%` has been unbanned."));

	Out.EOSBanResponseMessage = GetIniStringOrFallback(
		Cfg, TEXT("EOSBanResponseMessage"),
		TEXT(":hammer: **BanSystem** - EOS ID `%PlayerId%` banned by **%BannedBy%** (%Duration%) - Reason: %Reason%"));

	Out.EOSUnbanResponseMessage = GetIniStringOrFallback(
		Cfg, TEXT("EOSUnbanResponseMessage"),
		TEXT(":white_check_mark: **BanSystem** - EOS ID `%PlayerId%` has been unbanned."));

	UE_LOG(LogTemp, Log,
	       TEXT("BanSystem: Discord config loaded (DiscordChannelId=%s)."),
	       Out.DiscordChannelId.IsEmpty() ? TEXT("<empty – commands disabled>") : *Out.DiscordChannelId);

	return Out;
}
