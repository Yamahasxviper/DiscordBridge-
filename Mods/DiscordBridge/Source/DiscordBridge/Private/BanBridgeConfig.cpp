// Copyright Yamahasxviper. All Rights Reserved.

#include "BanBridgeConfig.h"

#include "Misc/ConfigCacheIni.h"
#include "HAL/PlatformFileManager.h"
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
