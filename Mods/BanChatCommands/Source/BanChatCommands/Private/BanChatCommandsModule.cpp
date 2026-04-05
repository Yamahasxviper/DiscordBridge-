// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsModule.h"
#include "BanChatCommandsConfig.h"
#include "Command/ChatCommandLibrary.h"
#include "Commands/BanChatCommands.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FBanChatCommandsModule"

void FBanChatCommandsModule::StartupModule()
{
    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module starting up."));

    // On every server start, write a backup to Saved/BanChatCommands/BanChatCommands.ini.
    // That folder is never touched by mod updates so the admin list survives
    // any wipe of the mod directory.
    BackupConfigIfNeeded();

    // Record the initial admin-list hash so the first poll can detect changes.
    LastConfigHash = ComputeAdminHash();

    // Start the periodic config-reload ticker.  It fires every
    // ConfigPollIntervalSeconds seconds for the lifetime of the module,
    // reloading BanChatCommands.ini from disk and logging when the admin list
    // changes.  No server restart or manual /reloadconfig needed.
    ConfigPollHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FBanChatCommandsModule::OnConfigPollTick),
        ConfigPollIntervalSeconds);

    UE_LOG(LogBanChatCommands, Log,
        TEXT("BanChatCommands: config auto-reload enabled — polling every %.0f seconds."),
        ConfigPollIntervalSeconds);

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
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AReloadConfigChatCommand::StaticClass());

            UE_LOG(LogBanChatCommands, Log,
                TEXT("BanChatCommands: Registered 11 commands (ban, tempban, unban, bancheck, banlist, linkbans, unlinkbans, playerhistory, whoami, banname, reloadconfig)."));;
        }
    );

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module started."));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Automatic config-reload ticker
// ─────────────────────────────────────────────────────────────────────────────

uint32 FBanChatCommandsModule::ComputeAdminHash()
{
    const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
    if (!Cfg) return 0;

    // Sort so that order-only changes don't trigger a false positive.
    TArray<FString> Sorted = Cfg->AdminEosPUIDs;
    Sorted.Sort();
    const FString Joined = FString::Join(Sorted, TEXT(","));
    return FCrc::StrCrc32(*Joined);
}

bool FBanChatCommandsModule::OnConfigPollTick(float /*DeltaTime*/)
{
    // Force UE to re-read all UPROPERTY(Config) fields from the ini files on disk.
    GetMutableDefault<UBanChatCommandsConfig>()->ReloadConfig();

    const uint32 NewHash = ComputeAdminHash();
    if (NewHash != LastConfigHash)
    {
        LastConfigHash = NewHash;
        BackupConfigIfNeeded();

        const int32 AdminCount = UBanChatCommandsConfig::Get()->AdminEosPUIDs.Num();
        UE_LOG(LogBanChatCommands, Log,
            TEXT("BanChatCommands: config auto-reloaded — %d admin(s) now active."),
            AdminCount);
    }

    return true; // keep ticking
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistent config backup
// ─────────────────────────────────────────────────────────────────────────────

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

void FBanChatCommandsModule::ShutdownModule()
{
    FTSTicker::GetCoreTicker().RemoveTicker(ConfigPollHandle);

    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanChatCommandsModule, BanChatCommands)
