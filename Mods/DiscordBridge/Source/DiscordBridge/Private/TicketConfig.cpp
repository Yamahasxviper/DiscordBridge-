// Copyright Yamahasxviper. All Rights Reserved.

#include "TicketConfig.h"

#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

static FString GetIniString(const FConfigFile& Cfg, const FString& Key,
                            const FString& Default = TEXT(""))
{
	FString Value;
	if (Cfg.GetString(TEXT("TicketSystem"), *Key, Value))
	{
		Value.TrimStartAndEndInline();
		return Value;
	}
	return Default;
}

static bool GetIniBool(const FConfigFile& Cfg, const FString& Key, bool Default)
{
	bool Value;
	if (Cfg.GetBool(TEXT("TicketSystem"), *Key, Value))
	{
		return Value;
	}
	return Default;
}

static float GetIniFloat(const FConfigFile& Cfg, const FString& Key, float Default)
{
	FString StrVal;
	if (Cfg.GetString(TEXT("TicketSystem"), *Key, StrVal))
	{
		StrVal.TrimStartAndEndInline();
		if (!StrVal.IsEmpty())
			return FCString::Atof(*StrVal);
	}
	return Default;
}

/** Parse every occurrence of "Key=Value" in a raw INI file string. */
static TArray<FString> ParseRawIniArray(const FString& RawContent, const FString& Key)
{
	TArray<FString> Result;
	TArray<FString> Lines;
	RawContent.ParseIntoArrayLines(Lines);

	const FString Prefix = Key + TEXT("=");
	for (const FString& Line : Lines)
	{
		const FString Trimmed = Line.TrimStartAndEnd();
		if (Trimmed.StartsWith(Prefix))
		{
			Result.Add(Trimmed.Mid(Prefix.Len()).TrimStartAndEnd());
		}
	}
	return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// FTicketConfig
// ─────────────────────────────────────────────────────────────────────────────

FString FTicketConfig::GetConfigFilePath()
{
	// Use FPaths::ProjectDir() + "Mods/..." — the same pattern used by
	// BanSystem and DiscordBridge throughout this repo.
	// FPaths::ProjectModsDir() is not part of Satisfactory's CSS custom
	// Unreal Engine build and would cause a compile/link failure.
	return FPaths::ProjectDir() /
	       TEXT("Mods/DiscordBridge/Config/DefaultTickets.ini");
}

FString FTicketConfig::GetBackupFilePath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("DiscordBridge"),
		TEXT("TicketSystem.ini"));
}

TArray<FString> FTicketConfig::ParseChannelIds(const FString& CommaList)
{
	TArray<FString> Parts;
	CommaList.ParseIntoArray(Parts, TEXT(","), /*bCullEmpty=*/true);
	for (FString& Part : Parts)
	{
		Part.TrimStartAndEndInline();
	}
	return Parts;
}

FTicketConfig FTicketConfig::Load()
{
	FTicketConfig Config;

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	const FString PrimaryPath = GetConfigFilePath();
	const FString BackupPath  = GetBackupFilePath();

	// ── Try to load the primary config file ───────────────────────────────────
	const bool bPrimaryExists = PlatformFile.FileExists(*PrimaryPath);

	if (bPrimaryExists)
	{
		FString RawContent;
		FFileHelper::LoadFileToString(RawContent, *PrimaryPath);

		FConfigFile Cfg;
		Cfg.Read(PrimaryPath);

		Config.BotToken                 = GetIniString(Cfg, TEXT("BotToken"));
		Config.GuildId                  = GetIniString(Cfg, TEXT("GuildId"));
		Config.TicketChannelId          = GetIniString(Cfg, TEXT("TicketChannelId"));
		Config.TicketLogChannelId       = GetIniString(Cfg, TEXT("TicketLogChannelId"));
		Config.bTicketWhitelistEnabled  = GetIniBool  (Cfg, TEXT("TicketWhitelistEnabled"),  true);
		Config.bTicketHelpEnabled       = GetIniBool  (Cfg, TEXT("TicketHelpEnabled"),       true);
		Config.bTicketReportEnabled     = GetIniBool  (Cfg, TEXT("TicketReportEnabled"),     true);
		Config.bTicketBanAppealEnabled  = GetIniBool  (Cfg, TEXT("BanAppealEnabled"),        true);
		Config.TicketNotifyRoleId       = GetIniString(Cfg, TEXT("TicketNotifyRoleId"));
		Config.TicketPanelChannelId     = GetIniString(Cfg, TEXT("TicketPanelChannelId"));
		Config.TicketCategoryId         = GetIniString(Cfg, TEXT("TicketCategoryId"));
		Config.CustomTicketReasons      = ParseRawIniArray(RawContent, TEXT("TicketReason"));
		Config.InactiveTicketTimeoutHours = GetIniFloat(Cfg, TEXT("InactiveTicketTimeoutHours"), 0.0f);

		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Loaded config from %s"), *PrimaryPath);
	}
	else
	{
		// ── Fall back to backup ────────────────────────────────────────────────
		if (PlatformFile.FileExists(*BackupPath))
		{
			FString BackupRaw;
			FFileHelper::LoadFileToString(BackupRaw, *BackupPath);

			FConfigFile BackupCfg;
			BackupCfg.Read(BackupPath);

			Config.BotToken                = GetIniString(BackupCfg, TEXT("BotToken"));
			Config.GuildId                 = GetIniString(BackupCfg, TEXT("GuildId"));
			Config.TicketChannelId         = GetIniString(BackupCfg, TEXT("TicketChannelId"));
			Config.TicketLogChannelId      = GetIniString(BackupCfg, TEXT("TicketLogChannelId"));
			Config.bTicketWhitelistEnabled = GetIniBool  (BackupCfg, TEXT("TicketWhitelistEnabled"), true);
			Config.bTicketHelpEnabled      = GetIniBool  (BackupCfg, TEXT("TicketHelpEnabled"),      true);
			Config.bTicketReportEnabled    = GetIniBool  (BackupCfg, TEXT("TicketReportEnabled"),    true);
			Config.bTicketBanAppealEnabled = GetIniBool  (BackupCfg, TEXT("BanAppealEnabled"),       true);
			Config.TicketNotifyRoleId      = GetIniString(BackupCfg, TEXT("TicketNotifyRoleId"));
			Config.TicketPanelChannelId    = GetIniString(BackupCfg, TEXT("TicketPanelChannelId"));
			Config.TicketCategoryId        = GetIniString(BackupCfg, TEXT("TicketCategoryId"));
			Config.CustomTicketReasons     = ParseRawIniArray(BackupRaw, TEXT("TicketReason"));
			Config.InactiveTicketTimeoutHours = GetIniFloat(BackupCfg, TEXT("InactiveTicketTimeoutHours"), 0.0f);

			UE_LOG(LogTicketSystem, Log,
			       TEXT("TicketSystem: Primary config not found at '%s' – restored from backup."),
			       *PrimaryPath);
		}
		else
		{
			UE_LOG(LogTicketSystem, Warning,
			       TEXT("TicketSystem: No config found at '%s' or '%s'. Using defaults."),
			       *PrimaryPath, *BackupPath);
		}
	}

	// ── Write the backup ───────────────────────────────────────────────────────
	// Build a raw key=value map to write to the backup file so settings survive
	// a mod update that resets DefaultTickets.ini.
	if (bPrimaryExists || PlatformFile.FileExists(*BackupPath))
	{
		FString BackupContent;
		BackupContent += TEXT("[TicketSystem]\n");
		BackupContent += FString::Printf(TEXT("BotToken=%s\n"),               *Config.BotToken);
		BackupContent += FString::Printf(TEXT("GuildId=%s\n"),                *Config.GuildId);
		BackupContent += FString::Printf(TEXT("TicketChannelId=%s\n"),        *Config.TicketChannelId);
		BackupContent += FString::Printf(TEXT("TicketLogChannelId=%s\n"),     *Config.TicketLogChannelId);
		BackupContent += FString::Printf(TEXT("TicketWhitelistEnabled=%s\n"), Config.bTicketWhitelistEnabled ? TEXT("True") : TEXT("False"));
		BackupContent += FString::Printf(TEXT("TicketHelpEnabled=%s\n"),      Config.bTicketHelpEnabled      ? TEXT("True") : TEXT("False"));
		BackupContent += FString::Printf(TEXT("TicketReportEnabled=%s\n"),    Config.bTicketReportEnabled    ? TEXT("True") : TEXT("False"));
		BackupContent += FString::Printf(TEXT("BanAppealEnabled=%s\n"),       Config.bTicketBanAppealEnabled ? TEXT("True") : TEXT("False"));
		BackupContent += FString::Printf(TEXT("TicketNotifyRoleId=%s\n"),     *Config.TicketNotifyRoleId);
		BackupContent += FString::Printf(TEXT("TicketPanelChannelId=%s\n"),   *Config.TicketPanelChannelId);
		BackupContent += FString::Printf(TEXT("TicketCategoryId=%s\n"),       *Config.TicketCategoryId);
		for (const FString& Reason : Config.CustomTicketReasons)
		{
			BackupContent += FString::Printf(TEXT("TicketReason=%s\n"), *Reason);
		}
		BackupContent += FString::Printf(TEXT("InactiveTicketTimeoutHours=%.2f\n"),
			Config.InactiveTicketTimeoutHours);

		PlatformFile.CreateDirectoryTree(*FPaths::GetPath(BackupPath));
		FFileHelper::SaveStringToFile(BackupContent, *BackupPath);
	}

	return Config;
}
