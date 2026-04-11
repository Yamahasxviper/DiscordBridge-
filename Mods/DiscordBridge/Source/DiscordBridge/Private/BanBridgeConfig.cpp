// Copyright Yamahasxviper. All Rights Reserved.

#include "BanBridgeConfig.h"

#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY(LogBanDiscord);

// ─────────────────────────────────────────────────────────────────────────────
// Internal helpers
// ─────────────────────────────────────────────────────────────────────────────

static FString GetIniBridgeString(const FConfigFile& Cfg, const FString& Key,
                                   const FString& Default = TEXT(""))
{
	FString Value;
	if (Cfg.GetString(TEXT("BanBridge"), *Key, Value))
	{
		Value.TrimStartAndEndInline();
		return Value;
	}
	return Default;
}

// ─────────────────────────────────────────────────────────────────────────────
// FBanBridgeConfig
// ─────────────────────────────────────────────────────────────────────────────

FString FBanBridgeConfig::GetConfigFilePath()
{
	// Mirror the pattern used by DiscordBridgeConfig and TicketConfig in this
	// repository: resolve relative to FPaths::ProjectDir() so the path works
	// on both the Windows editor host and Linux dedicated server.
	return FPaths::ProjectDir() /
	       TEXT("Mods/DiscordBridge/Config/DefaultBanBridge.ini");
}

FString FBanBridgeConfig::GetBackupFilePath()
{
	return FPaths::Combine(
		FPaths::ProjectSavedDir(),
		TEXT("DiscordBridge"),
		TEXT("BanBridge.ini"));
}

FBanBridgeConfig FBanBridgeConfig::Load()
{
	FBanBridgeConfig Out;

	const FString PrimaryPath = GetConfigFilePath();
	const FString BackupPath  = GetBackupFilePath();

	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	FConfigFile Cfg;
	bool bLoaded = false;

	// Try primary config file first.
	if (PF.FileExists(*PrimaryPath))
	{
		Cfg.Read(PrimaryPath);
		bLoaded = Cfg.Contains(TEXT("BanBridge"));
	}

	// Fall back to the backup copy if primary is missing or empty.
	if (!bLoaded && PF.FileExists(*BackupPath))
	{
		Cfg.Read(BackupPath);
		bLoaded = Cfg.Contains(TEXT("BanBridge"));
	}

	if (!bLoaded)
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: No [BanBridge] section found at '%s'. "
		            "Discord ban commands are disabled until AdminRoleId is set."),
		       *PrimaryPath);
		return Out;
	}

	Out.AdminRoleId            = GetIniBridgeString(Cfg, TEXT("AdminRoleId"));
	Out.ModeratorRoleId        = GetIniBridgeString(Cfg, TEXT("ModeratorRoleId"));
	Out.BanCommandChannelId    = GetIniBridgeString(Cfg, TEXT("BanCommandChannelId"));
	Out.ModerationLogChannelId = GetIniBridgeString(Cfg, TEXT("ModerationLogChannelId"));

	if (Out.AdminRoleId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: AdminRoleId is not set. Discord ban commands are disabled."));
	}
	else
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: AdminRoleId            = \"%s\""), *Out.AdminRoleId);
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: ModeratorRoleId        = \"%s\""),
		       Out.ModeratorRoleId.IsEmpty() ? TEXT("(not set)") : *Out.ModeratorRoleId);
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: BanCommandChannelId    = \"%s\""),
		       Out.BanCommandChannelId.IsEmpty() ? TEXT("(any channel)") : *Out.BanCommandChannelId);
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: ModerationLogChannelId = \"%s\""),
		       Out.ModerationLogChannelId.IsEmpty() ? TEXT("(disabled)") : *Out.ModerationLogChannelId);
	}

	// ── Write backup ─────────────────────────────────────────────────────────
	// Mirror the pattern used by DiscordBridgeConfig and TicketConfig: on every
	// server start write the current settings to Saved/DiscordBridge/BanBridge.ini
	// so they survive any future deletion or reset of the primary config file.
	{
		const FString BackupContent =
			FString(TEXT("[BanBridge]\n"))
			+ TEXT("# Auto-generated backup of DefaultBanBridge.ini.\n")
			+ TEXT("# This file is read automatically when the primary config has no [BanBridge] section.\n")
			+ TEXT("AdminRoleId=")            + Out.AdminRoleId            + TEXT("\n")
			+ TEXT("ModeratorRoleId=")        + Out.ModeratorRoleId        + TEXT("\n")
			+ TEXT("BanCommandChannelId=")    + Out.BanCommandChannelId    + TEXT("\n")
			+ TEXT("ModerationLogChannelId=") + Out.ModerationLogChannelId + TEXT("\n");

		PF.CreateDirectoryTree(*FPaths::GetPath(BackupPath));
		if (FFileHelper::SaveStringToFile(BackupContent, *BackupPath))
		{
			UE_LOG(LogBanDiscord, Log,
			       TEXT("BanBridge: Updated backup config at '%s'."), *BackupPath);
		}
		else
		{
			UE_LOG(LogBanDiscord, Warning,
			       TEXT("BanBridge: Could not write backup config to '%s'."), *BackupPath);
		}
	}

	return Out;
}

// ─────────────────────────────────────────────────────────────────────────────
// Authorisation helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FBanBridgeConfig::IsAdminRole(const TArray<FString>& Roles) const
{
	return !AdminRoleId.IsEmpty() && Roles.Contains(AdminRoleId);
}

bool FBanBridgeConfig::IsModeratorRole(const TArray<FString>& Roles) const
{
	if (IsAdminRole(Roles))
		return true;
	return !ModeratorRoleId.IsEmpty() && Roles.Contains(ModeratorRoleId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Template restoration
// ─────────────────────────────────────────────────────────────────────────────

void FBanBridgeConfig::RestoreDefaultConfigIfNeeded()
{
	const FString PrimaryPath = GetConfigFilePath();
	IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

	// Leave the file as-is if it already contains comment lines — that means
	// it was either hand-edited or previously written by this function.
	if (PF.FileExists(*PrimaryPath))
	{
		FString Existing;
		FFileHelper::LoadFileToString(Existing, *PrimaryPath);
		if (Existing.Contains(TEXT(";")))
			return;
	}

	// File is missing or was stripped of comments by Alpakit — write the
	// annotated template so operators can see setting descriptions.
	// This content mirrors DefaultBanBridge.ini shipped in the repository.
	const FString Template =
		TEXT("; DiscordBridge – BanSystem integration settings\n")
		TEXT("; ─────────────────────────────────────────────────────────────────────────────\n")
		TEXT("; Configure the Discord role and channel IDs for ban/moderation commands.\n")
		TEXT(";\n")
		TEXT("; NOTE: this file is NOT overwritten by mod updates. Your settings persist\n")
		TEXT("; across version upgrades. A backup is also written automatically to\n")
		TEXT(";   <ServerRoot>/FactoryGame/Saved/DiscordBridge/BanBridge.ini\n")
		TEXT("; on every server start so settings survive primary file deletion.\n")
		TEXT(";\n")
		TEXT("[BanBridge]\n")
		TEXT("\n")
		TEXT("; Discord role ID whose members may run ALL ban/moderation commands.\n")
		TEXT("; Leave empty to disable all Discord ban commands.\n")
		TEXT("; How to get: Discord Settings → Advanced → Developer Mode,\n")
		TEXT("; then right-click the role in Server Settings → Roles → Copy Role ID.\n")
		TEXT("AdminRoleId=\n")
		TEXT("\n")
		TEXT("; Optional: Discord role ID for moderators with limited command access.\n")
		TEXT("; Moderators may use: !kick, !modban, !mute, !unmute, !tempmute,\n")
		TEXT("; !mutecheck, !mutelist, !announce, !stafflist, !staffchat.\n")
		TEXT("; Leave empty to restrict those commands to admins only.\n")
		TEXT("ModeratorRoleId=\n")
		TEXT("\n")
		TEXT("; Optional: restrict ban commands to a single Discord channel.\n")
		TEXT("; Leave empty to allow ban commands from any channel the bot can see.\n")
		TEXT("BanCommandChannelId=\n")
		TEXT("\n")
		TEXT("; Optional: Discord channel ID to post a moderation log after every\n")
		TEXT("; ban/unban/kick/mute/warn action. Leave empty to disable logging.\n")
		TEXT("ModerationLogChannelId=\n");

	PF.CreateDirectoryTree(*FPaths::GetPath(PrimaryPath));
	if (FFileHelper::SaveStringToFile(Template, *PrimaryPath))
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanBridge: Created annotated config template at '%s'. "
		            "Set AdminRoleId to enable Discord ban commands."), *PrimaryPath);
	}
	else
	{
		UE_LOG(LogBanDiscord, Warning,
		       TEXT("BanBridge: Could not write config template to '%s'."), *PrimaryPath);
	}
}
