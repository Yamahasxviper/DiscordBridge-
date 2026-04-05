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

void FBanSystemModule::ShutdownModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
