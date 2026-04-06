// Copyright Yamahasxviper. All Rights Reserved.

#include "ServerWhitelistConfig.h"

#include "HAL/PlatformFileManager.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

namespace
{
	static const TCHAR* ConfigSection = TEXT("ServerWhitelist");

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

	/** Default content written when the config file does not exist yet. */
	const FString DefaultConfigContent =
		TEXT("[ServerWhitelist]\n")
		TEXT("; ==============================================================================\n")
		TEXT("; ServerWhitelist – Configuration\n")
		TEXT("; ==============================================================================\n")
		TEXT(";\n")
		TEXT("; Set WhitelistEnabled=True and add player names below to activate.\n")
		TEXT("; Use in-game commands to manage at runtime (admin only):\n")
		TEXT(";   !whitelist on/off\n")
		TEXT(";   !whitelist add <Name>\n")
		TEXT(";   !whitelist remove <Name>\n")
		TEXT(";   !whitelist list\n")
		TEXT(";   !whitelist status\n")
		TEXT(";\n")
		TEXT("; ==============================================================================\n")
		TEXT("\n")
		TEXT("; Enable or disable the whitelist.  Default: False\n")
		TEXT("WhitelistEnabled=False\n")
		TEXT("\n")
		TEXT("; Message shown to a kicked player.  Default: (see below)\n")
		TEXT("WhitelistKickReason=You are not on this server's whitelist. Contact the server admin to be added.\n")
		TEXT("\n")
		TEXT("; In-game command prefix.  Default: !whitelist\n")
		TEXT("InGameCommandPrefix=!whitelist\n")
		TEXT("\n")
		TEXT("; Admin player names that can run !whitelist commands in-game.\n")
		TEXT("; Add one line per admin.\n")
		TEXT("; Example: +AdminPlayerNames=Alice\n")
		TEXT("\n")
		TEXT("; Initial player whitelist (applied only on the very first start).\n")
		TEXT("; After that, edit ServerWhitelist.json or use in-game commands.\n")
		TEXT("; Example: +WhitelistedPlayers=Alice\n");
}

// ---------------------------------------------------------------------------

FString FServerWhitelistConfig::GetConfigFilePath()
{
	return FPaths::ProjectModsDir() / TEXT("ServerWhitelist/Config/DefaultServerWhitelist.ini");
}

FServerWhitelistConfig FServerWhitelistConfig::LoadOrCreate()
{
	FServerWhitelistConfig Out;
	const FString FilePath = GetConfigFilePath();

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		UE_LOG(LogTemp, Display,
			TEXT("ServerWhitelist: Config not found – creating default at %s"), *FilePath);
		FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(
			*FPaths::GetPath(FilePath));
		FFileHelper::SaveStringToFile(DefaultConfigContent, *FilePath);
		return Out; // all defaults
	}

	FConfigFile Cfg;
	Cfg.Read(FilePath);

	Out.bWhitelistEnabled = GetIniBoolOrDefault(Cfg, TEXT("WhitelistEnabled"), false);
	Out.WhitelistKickReason = GetIniStringOrFallback(
		Cfg,
		TEXT("WhitelistKickReason"),
		TEXT("You are not on this server's whitelist. Contact the server admin to be added."));
	Out.InGameCommandPrefix = GetIniStringOrDefault(
		Cfg, TEXT("InGameCommandPrefix"), TEXT("!whitelist"));

	// Read TArray<FString> from +AdminPlayerNames= lines.
	if (const FConfigSection* Section = Cfg.FindSection(ConfigSection))
	{
		for (auto It = Section->CreateConstIterator(); It; ++It)
		{
			const FName Key = It.Key();
			if (Key == TEXT("AdminPlayerNames"))
			{
				const FString Val = It.Value().GetValue().TrimStartAndEnd();
				if (!Val.IsEmpty())
				{
					Out.AdminPlayerNames.AddUnique(Val.ToLower());
				}
			}
			else if (Key == TEXT("WhitelistedPlayers"))
			{
				const FString Val = It.Value().GetValue().TrimStartAndEnd();
				if (!Val.IsEmpty())
				{
					Out.InitialWhitelistedPlayers.AddUnique(Val);
				}
			}
		}
	}

	UE_LOG(LogTemp, Log,
		TEXT("ServerWhitelist: Config loaded – enabled=%s, admins=%d, initialPlayers=%d"),
		Out.bWhitelistEnabled ? TEXT("true") : TEXT("false"),
		Out.AdminPlayerNames.Num(),
		Out.InitialWhitelistedPlayers.Num());

	return Out;
}
