// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsModule.h"
#include "BanChatCommandsConfig.h"
#include "Command/ChatCommandLibrary.h"
#include "Commands/BanChatCommands.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Interfaces/IPluginManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FBanChatCommandsModule"

void FBanChatCommandsModule::StartupModule()
{
    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module starting up."));

    // Restore documentation comments to Config/DefaultBanChatCommands.ini.
    // UE's staging pipeline strips them via FConfigFile; this re-applies them
    // on the first server start after each mod update.
    RestoreDefaultConfigDocs();

    // On every server start, write a backup to Saved/BanChatCommands/BanChatCommands.ini.
    // That folder is never touched by mod updates so the admin list survives
    // any wipe of the mod directory.
    BackupConfigIfNeeded();

    WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](const UWorld::FActorsInitializedParams& Params)
        {
            UWorld* World = Params.World;
            if (!World) return;

            const ENetMode NetMode = World->GetNetMode();
            if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer)
                return;

            AChatCommandSubsystem* CmdSys = AChatCommandSubsystem::Get(World);
            if (!CmdSys)
            {
                UE_LOG(LogBanChatCommands, Warning,
                    TEXT("BanChatCommands: AChatCommandSubsystem not available — commands not registered."));
                return;
            }

            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ATempBanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnbanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanCheckChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanListChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ALinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnlinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), APlayerHistoryChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AWhoAmIChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanNameChatCommand::StaticClass());

            UE_LOG(LogBanChatCommands, Log,
                TEXT("BanChatCommands: Registered 10 commands (ban, tempban, unban, bancheck, banlist, linkbans, unlinkbans, playerhistory, whoami, banname)."));;
        }
    );

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module started."));
}

void FBanChatCommandsModule::BackupConfigIfNeeded()
{
    const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
    if (!Cfg) return;

    // Saved/BanChatCommands/BanChatCommands.ini — dedicated per-mod folder so it's
    // easy to find, and never touched by mod updates or Alpakit dev deploys.
    const FString BackupPath = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("BanChatCommands"),
        TEXT("BanChatCommands.ini"));

    // Write on every server start so the backup always reflects the current
    // admin list.  Saved/BanChatCommands/ is never touched by mod updates or
    // Alpakit dev deploys, so the list survives any wipe of the mod directory.
    FString Content =
        FString(TEXT("; BanChatCommands admin configuration.\n"))
        + TEXT(";\n")
        + TEXT("; Edit this file to configure the admin list persistently.\n")
        + TEXT("; It is never overwritten by mod updates.\n")
        + TEXT(";\n")
        + TEXT("; EOS Product User IDs -- Each +AdminEosPUIDs= entry grants that player\n")
        + TEXT("; permission to run /ban, /tempban, /unban, /bancheck, /banlist,\n")
        + TEXT("; /linkbans, /unlinkbans, and /playerhistory from in-game chat.\n")
        + TEXT(";\n")
        + TEXT("; EOS PUIDs are 32-character hex strings.  Players can find their own\n")
        + TEXT("; PUID by running /whoami in-game.\n")
        + TEXT(";\n")
        + TEXT("; Note: On CSS Dedicated Server, all players are identified by their EOS\n")
        + TEXT("; Product User ID regardless of their launch platform (Steam, Epic, etc.).\n")
        + TEXT(";\n")
        + TEXT("; If the list is empty, those commands are only usable from the server\n")
        + TEXT("; console.  /whoami is always available to all players.\n")
        + TEXT("\n")
        + TEXT("[/Script/BanChatCommands.BanChatCommandsConfig]\n");

    for (const FString& Puid : Cfg->AdminEosPUIDs)
    {
        Content += TEXT("+AdminEosPUIDs=") + Puid + TEXT("\n");
    }

    if (Cfg->AdminEosPUIDs.IsEmpty())
    {
        Content += TEXT("; +AdminEosPUIDs=YOUR_EOS_PUID_HERE\n");
    }

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(BackupPath));

    if (FFileHelper::SaveStringToFile(Content, *BackupPath))
    {
        UE_LOG(LogBanChatCommands, Log,
            TEXT("BanChatCommands: Updated backup config at '%s'."), *BackupPath);
    }
    else
    {
        UE_LOG(LogBanChatCommands, Warning,
            TEXT("BanChatCommands: Could not write backup config to '%s'."), *BackupPath);
    }
}

void FBanChatCommandsModule::RestoreDefaultConfigDocs()
{
    // UE's staging pipeline reads Default*.ini through FConfigFile and writes
    // them back to the staged directory, stripping all comments in the process.
    // To ensure server admins always see the documented config file in the mod's
    // Config/ folder, we re-write it with full comments on every server start.

    const TSharedPtr<IPlugin> Plugin = IPluginManager::Get().FindPlugin(TEXT("BanChatCommands"));
    if (!Plugin.IsValid()) return;

    const FString DefaultConfigPath = FPaths::Combine(
        Plugin->GetBaseDir(), TEXT("Config"), TEXT("DefaultBanChatCommands.ini"));

    static const FString Content =
        FString(TEXT("; BanChatCommands admin configuration.\n"))
        + TEXT(";\n")
        + TEXT("; RECOMMENDED: place your admin list in\n")
        + TEXT(";   Saved/BanChatCommands/BanChatCommands.ini\n")
        + TEXT("; using the section header below.  That file is never overwritten by mod\n")
        + TEXT("; updates.  BanChatCommands writes your current admin list there on every\n")
        + TEXT("; server start so it survives any wipe of the mod directory.\n")
        + TEXT(";\n")
        + TEXT("; EOS Product User IDs -- Each +AdminEosPUIDs= entry grants that player permission to run\n")
        + TEXT("; /ban, /tempban, /unban, /bancheck, /banlist, /linkbans, /unlinkbans, and /playerhistory\n")
        + TEXT("; from in-game chat.\n")
        + TEXT(";\n")
        + TEXT("; EOS PUIDs are 32-character hex strings.  Players can find their own PUID by running\n")
        + TEXT("; /whoami in-game.\n")
        + TEXT(";\n")
        + TEXT("; Note: On CSS Dedicated Server, all players are identified by their EOS Product User ID\n")
        + TEXT("; regardless of their launch platform (Steam, Epic, etc.).\n")
        + TEXT(";\n")
        + TEXT("; If the list is empty, those commands are only usable from the server console.\n")
        + TEXT("; /whoami is always available to all players regardless of this setting.\n")
        + TEXT(";\n")
        + TEXT("; Example:\n")
        + TEXT(";   [/Script/BanChatCommands.BanChatCommandsConfig]\n")
        + TEXT(";   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51\n")
        + TEXT("\n")
        + TEXT("[/Script/BanChatCommands.BanChatCommandsConfig]\n")
        + TEXT("; +AdminEosPUIDs=YOUR_EOS_PUID_HERE\n");

    if (FFileHelper::SaveStringToFile(Content, *DefaultConfigPath))
    {
        UE_LOG(LogBanChatCommands, Log,
            TEXT("BanChatCommands: Restored documentation comments in '%s'."),
            *DefaultConfigPath);
    }
    else
    {
        UE_LOG(LogBanChatCommands, Warning,
            TEXT("BanChatCommands: Could not restore documentation comments in '%s' (read-only?)."),
            *DefaultConfigPath);
    }
}

void FBanChatCommandsModule::ShutdownModule()
{
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanChatCommandsModule, BanChatCommands)
