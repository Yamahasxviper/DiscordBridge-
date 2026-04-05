// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/index.ts

#include "BanSystemModule.h"
#include "BanDatabase.h"
#include "BanSystemConfig.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanSystem, Log, All);

#define LOCTEXT_NAMESPACE "FBanSystemModule"

void FBanSystemModule::StartupModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module starting."));
    // UBanDatabase, UBanRestApi, and UBanEnforcer are UGameInstanceSubsystem
    // subclasses and initialise automatically when the game instance starts.
    // No manual setup is required here.

    // Re-apply documentation comments stripped by UE's staging pipeline.
    // Must run before BackupConfigIfNeeded so live values are available.
    RestoreDefaultConfigDocs();

    // On every server start, write a backup to Saved/BanSystem/BanSystem.ini.
    // That folder is never touched by mod updates so settings survive any wipe
    // of the mod directory.
    BackupConfigIfNeeded();

    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module started."));
}

void FBanSystemModule::BackupConfigIfNeeded()
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg) return;

    // Saved/BanSystem/BanSystem.ini — dedicated per-mod folder so it's easy to
    // find, and never touched by mod updates or Alpakit dev deploys.
    const FString BackupPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("BanSystem"),
        TEXT("BanSystem.ini"));

    // Write on every server start so the backup always reflects the current
    // settings.  Saved/BanSystem/ is never touched by mod updates or Alpakit
    // dev deploys, so the settings survive any wipe of the mod directory.
    // NOTE: String concatenation (not Printf) to avoid misinterpreting any
    // '%' character that might appear in a custom DatabasePath.
    const FString Content =
        FString(TEXT("; BanSystem configuration.\n"))
        + TEXT(";\n")
        + TEXT("; Edit this file to configure BanSystem persistently.\n")
        + TEXT("; It is never overwritten by mod updates.\n")
        + TEXT(";\n")
        + TEXT("; Settings are also read from the mod's own Config/DefaultBanSystem.ini,\n")
        + TEXT("; but that file may be overwritten when the mod is updated.\n")
        + TEXT("\n")
        + TEXT("[/Script/BanSystem.BanSystemConfig]\n")
        + TEXT("\n")
        + TEXT("; -- Database ------------------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Absolute path to the JSON ban file.\n")
        + TEXT("; Leave empty to use the default: <ProjectSaved>/BanSystem/bans.json\n")
        + TEXT("; On Linux this is typically: /home/<user>/.config/Epic/FactoryGame/Saved/BanSystem/bans.json\n")
        + TEXT("DatabasePath=") + Cfg->DatabasePath + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- REST Management API -------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Port for the local HTTP management API (default: 3000).\n")
        + TEXT("; Mirrors the Tools/BanSystem API_PORT setting.\n")
        + TEXT(";\n")
        + TEXT("; The API binds to all interfaces (0.0.0.0).\n")
        + TEXT("; Restrict external access with your server firewall if needed.\n")
        + TEXT(";\n")
        + TEXT("; Set to 0 to disable the REST API entirely.\n")
        + TEXT("RestApiPort=")  + FString::FromInt(Cfg->RestApiPort) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Backup --------------------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Number of automatic database backups to keep (default: 5).\n")
        + TEXT("; A backup is created on demand via POST /bans/backup.\n")
        + TEXT("; Older backups beyond this limit are deleted automatically.\n")
        + TEXT("MaxBackups=")   + FString::FromInt(Cfg->MaxBackups)  + TEXT("\n");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(BackupPath));

    if (FFileHelper::SaveStringToFile(Content, *BackupPath))
    {
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: Updated backup config at '%s'."), *BackupPath);
    }
    else
    {
        UE_LOG(LogBanSystem, Warning,
            TEXT("BanSystem: Could not write backup config to '%s'."), *BackupPath);
    }
}

void FBanSystemModule::RestoreDefaultConfigDocs()
{
    // UE's staging pipeline reads Default*.ini through FConfigFile and writes
    // them back to the staged directory, stripping all comments in the process.
    // We detect this by checking whether the deployed file contains any ';'
    // comment lines (mirroring the DiscordBridge branch approach that checks for
    // '#').  If the file already has comments it was either hand-edited or
    // previously restored — leave it alone.  Only rewrite when comments are
    // absent so that user-customised values (e.g. a non-default RestApiPort) set
    // directly in this file are preserved.
    const FString DefaultConfigPath = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Mods"), TEXT("BanSystem"),
        TEXT("Config"), TEXT("DefaultBanSystem.ini"));

    // Load raw content; if file doesn't exist yet or already has comments, skip.
    FString RawContent;
    if (FFileHelper::LoadFileToString(RawContent, *DefaultConfigPath) &&
        RawContent.Contains(TEXT(";")))
    {
        return; // Already documented — nothing to do.
    }

    // File is either missing or Alpakit-stripped (no comments).  Embed the
    // current live values so existing operator customisations are not lost.
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    const FString DatabasePath = Cfg ? Cfg->DatabasePath : TEXT("");
    const FString RestApiPort  = Cfg ? FString::FromInt(Cfg->RestApiPort) : TEXT("3000");
    const FString MaxBackups   = Cfg ? FString::FromInt(Cfg->MaxBackups)  : TEXT("5");

    // NOTE: String concatenation (not Printf) to avoid misinterpreting any
    // '%' character that might appear in a custom DatabasePath.
    const FString Content =
        FString(TEXT("; BanSystem configuration.\n"))
        + TEXT(";\n")
        + TEXT("; Settings in this file are read automatically by UBanSystemConfig\n")
        + TEXT("; (UCLASS Config=BanSystem).\n")
        + TEXT(";\n")
        + TEXT("; RECOMMENDED: place your server-specific settings in\n")
        + TEXT(";   Saved/BanSystem/BanSystem.ini\n")
        + TEXT("; using the same section header below.  That file is never overwritten by mod\n")
        + TEXT("; updates.  BanSystem writes your current settings there on every server start\n")
        + TEXT("; so they survive any wipe of the mod directory.\n")
        + TEXT(";\n")
        + TEXT("; Example:\n")
        + TEXT(";\n")
        + TEXT("; [/Script/BanSystem.BanSystemConfig]\n")
        + TEXT("; DatabasePath=/home/user/bans.json\n")
        + TEXT("; RestApiPort=3001\n")
        + TEXT("; MaxBackups=10\n")
        + TEXT("\n")
        + TEXT("[/Script/BanSystem.BanSystemConfig]\n")
        + TEXT("\n")
        + TEXT("; -- Database ------------------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Absolute path to the JSON ban file.\n")
        + TEXT("; Leave empty to use the default: <ProjectSaved>/BanSystem/bans.json\n")
        + TEXT("; On Linux this is typically: /home/<user>/.config/Epic/FactoryGame/Saved/BanSystem/bans.json\n")
        + TEXT("DatabasePath=") + DatabasePath + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- REST Management API -------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Port for the local HTTP management API (default: 3000).\n")
        + TEXT("; Mirrors the Tools/BanSystem API_PORT setting.\n")
        + TEXT(";\n")
        + TEXT("; The API binds to all interfaces (0.0.0.0).\n")
        + TEXT("; Restrict external access with your server firewall if needed.\n")
        + TEXT(";\n")
        + TEXT("; Set to 0 to disable the REST API entirely.\n")
        + TEXT("RestApiPort=") + RestApiPort + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Backup --------------------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Number of automatic database backups to keep (default: 5).\n")
        + TEXT("; A backup is created on demand via POST /bans/backup.\n")
        + TEXT("; Older backups beyond this limit are deleted automatically.\n")
        + TEXT("MaxBackups=") + MaxBackups + TEXT("\n");

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DefaultConfigPath));

    if (FFileHelper::SaveStringToFile(Content, *DefaultConfigPath))
    {
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: Restored documentation comments in '%s'."),
            *DefaultConfigPath);
    }
    else
    {
        UE_LOG(LogBanSystem, Warning,
            TEXT("BanSystem: Could not restore documentation comments in '%s' (read-only?)."),
            *DefaultConfigPath);
    }
}

void FBanSystemModule::ShutdownModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
