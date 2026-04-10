// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsModule.h"
#include "BanChatCommandsConfig.h"
#include "Command/ChatCommandLibrary.h"
#include "Commands/BanChatCommands.h"
#include "MuteRegistry.h"
#include "BanWebSocketPusher.h"
#include "BanAuditLog.h"
#include "Engine/World.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#define LOCTEXT_NAMESPACE "FBanChatCommandsModule"

void FBanChatCommandsModule::StartupModule()
{
    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module starting up."));

    // Restore the annotated Default*.ini so operators can always read the
    // setting descriptions even after a mod update or fresh install.
    RestoreDefaultConfigIfNeeded();

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

    // Start the mute expiry ticker — checks timed mutes every 30 s.
    MuteExpiryHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FBanChatCommandsModule::OnMuteExpiryTick),
        30.0f);

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
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnbanNameChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanCheckChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanListChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ALinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnlinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), APlayerHistoryChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AWhoAmIChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanNameChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AReloadConfigChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AKickChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AModBanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AWarnChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AWarningsChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AClearWarnsChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AAnnounceChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AStaffListChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AReasonChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AHistoryChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AMuteChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnmuteChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ANoteChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ANotesChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ADurationChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ATempUnmuteChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AMuteCheckChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanReasonChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AStaffChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AMuteListChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AClearWarnByIdChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AExtendBanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AAppealChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AMuteReasonChatCommand::StaticClass());

            UE_LOG(LogBanChatCommands, Log,
                TEXT("BanChatCommands: Registered 36 commands (ban, tempban, unban, unbanname, bancheck, banlist, linkbans, unlinkbans, playerhistory, whoami, banname, reloadconfig, kick, modban, warn, warnings, clearwarns, announce, stafflist, reason, history, mute, unmute, note, notes, duration, tempunmute, mutecheck, banreason, staffchat, mutelist, clearwarn, extend, appeal, mutereason)."));

            // Bind MuteRegistry delegates to push WebSocket events and write audit log.
            UGameInstance* GI = World->GetGameInstance();
            if (GI)
            {
                TWeakObjectPtr<UGameInstance> WeakGI(GI);
                UMuteRegistry* MuteReg = GI->GetSubsystem<UMuteRegistry>();
                if (MuteReg)
                {
                    MuteReg->OnPlayerMuted.AddLambda(
                        [WeakGI](const FMuteEntry& Entry, bool bIsTimed)
                        {
                            UBanWebSocketPusher::PushMuteEvent(
                                TEXT("mute"), Entry.Uid, Entry.PlayerName,
                                Entry.MutedBy, Entry.Reason,
                                bIsTimed,
                                bIsTimed ? Entry.ExpireDate.ToIso8601() : FString());

                            if (UGameInstance* LiveGI = WeakGI.Get())
                            {
                                if (UBanAuditLog* AuditLog = LiveGI->GetSubsystem<UBanAuditLog>())
                                    AuditLog->LogAction(TEXT("mute"), Entry.Uid, Entry.PlayerName,
                                        Entry.MutedBy, Entry.MutedBy, Entry.Reason);
                            }
                        });

                    MuteReg->OnPlayerUnmuted.AddLambda(
                        [WeakGI](const FString& Uid)
                        {
                            UBanWebSocketPusher::PushMuteEvent(
                                TEXT("unmute"), Uid, TEXT(""), TEXT(""), TEXT(""), false, FString());

                            if (UGameInstance* LiveGI = WeakGI.Get())
                            {
                                if (UBanAuditLog* AuditLog = LiveGI->GetSubsystem<UBanAuditLog>())
                                    AuditLog->LogAction(TEXT("unmute"), Uid, TEXT(""),
                                        TEXT("system"), TEXT(""), TEXT(""));
                            }
                        });
                }
            }
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
        + TEXT("; Edit this file to configure the admin and moderator lists persistently.\n")
        + TEXT("; It is never overwritten by mod updates.\n")
        + TEXT(";\n")
        + TEXT("; EOS Product User IDs -- Each +AdminEosPUIDs= entry grants that player\n")
        + TEXT("; permission to run /ban, /tempban, /unban, /bancheck, /banlist,\n")
        + TEXT("; /linkbans, /unlinkbans, /playerhistory, /warn, /warnings, and /clearwarns\n")
        + TEXT("; from in-game chat.\n")
        + TEXT(";\n")
        + TEXT("; Each +ModeratorEosPUIDs= entry grants permission to run /kick and /modban only.\n")
        + TEXT("; Admins automatically have moderator permissions too.\n")
        + TEXT(";\n")
        + TEXT("; EOS PUIDs are 32-character hex strings.  Players can find their own\n")
        + TEXT("; PUID by running /whoami in-game.\n")
        + TEXT(";\n")
        + TEXT("; Note: On CSS Dedicated Server, all players are identified by their EOS\n")
        + TEXT("; Product User ID regardless of their launch platform (Steam, Epic, etc.).\n")
        + TEXT(";\n")
        + TEXT("; If the admin list is empty, those commands are only usable from the server\n")
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

    Content += TEXT("\n");

    for (const FString& Puid : Cfg->ModeratorEosPUIDs)
    {
        Content += TEXT("+ModeratorEosPUIDs=") + Puid + TEXT("\n");
    }

    if (Cfg->ModeratorEosPUIDs.IsEmpty())
    {
        Content += TEXT("; +ModeratorEosPUIDs=MODERATOR_EOS_PUID_HERE\n");
    }

    Content += FString::Printf(TEXT("BanListPageSize=%d\n"), Cfg->BanListPageSize);
    Content += FString::Printf(TEXT("ModBanDurationMinutes=%d\n"), Cfg->ModBanDurationMinutes);
    Content += FString::Printf(TEXT("MaxModMuteDurationMinutes=%d\n"), Cfg->MaxModMuteDurationMinutes);
    Content += FString::Printf(TEXT("bAllowModNotes=%s\n"), Cfg->bAllowModNotes ? TEXT("True") : TEXT("False"));

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

void FBanChatCommandsModule::RestoreDefaultConfigIfNeeded()
{
    const FString DefaultIniPath = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Mods"), TEXT("BanChatCommands"),
        TEXT("Config"), TEXT("DefaultBanChatCommands.ini"));

    IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

    // Only rewrite when the file is missing or has been stripped of comment
    // lines.  Alpakit's staging step runs Default*.ini through UE's config
    // cache, which drops all semicolon comment lines.  Excluding the file via
    // PluginSettings.ini prevents that, but a fresh install may still land
    // without the file.  This keeps comments visible after every update.
    if (PlatformFile.FileExists(*DefaultIniPath))
    {
        FString Existing;
        FFileHelper::LoadFileToString(Existing, *DefaultIniPath);
        if (Existing.Contains(TEXT("; ")))
            return; // comment lines present — leave as-is
    }

    const FString Content =
        FString(TEXT("; BanChatCommands admin configuration.\n"))
        + TEXT(";\n")
        + TEXT("; RECOMMENDED: place your admin and moderator lists in\n")
        + TEXT(";   Saved/BanChatCommands/BanChatCommands.ini\n")
        + TEXT("; using the section header below.  That file is never overwritten by mod\n")
        + TEXT("; updates.  BanChatCommands writes your current lists there on every\n")
        + TEXT("; server start so they survive any wipe of the mod directory.\n")
        + TEXT(";\n")
        + TEXT("; EOS Product User IDs -- Each +AdminEosPUIDs= entry grants that player permission to run\n")
        + TEXT("; /ban, /tempban, /unban, /bancheck, /banlist, /linkbans, /unlinkbans, /playerhistory,\n")
        + TEXT("; /warn, /warnings, and /clearwarns from in-game chat.\n")
        + TEXT(";\n")
        + TEXT("; Each +ModeratorEosPUIDs= entry grants permission to run /kick and /modban only.\n")
        + TEXT("; Admins automatically have moderator permissions too.\n")
        + TEXT(";\n")
        + TEXT("; BanListPageSize controls how many bans are shown per page in /banlist (default: 10, max: 50).\n")
        + TEXT(";\n")
        + TEXT("; EOS PUIDs are 32-character hex strings.  Players can find their own PUID by running\n")
        + TEXT("; /whoami in-game.\n")
        + TEXT(";\n")
        + TEXT("; Note: On CSS Dedicated Server, all players are identified by their EOS Product User ID\n")
        + TEXT("; regardless of their launch platform (Steam, Epic, etc.).\n")
        + TEXT(";\n")
        + TEXT("; If the admin list is empty, those commands are only usable from the server console.\n")
        + TEXT("; /whoami is always available to all players regardless of this setting.\n")
        + TEXT(";\n")
        + TEXT("; Example:\n")
        + TEXT(";   [/Script/BanChatCommands.BanChatCommandsConfig]\n")
        + TEXT(";   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51\n")
        + TEXT(";   +ModeratorEosPUIDs=aabbccdd11223344aabbccdd11223344\n")
        + TEXT(";   BanListPageSize=10\n")
        + TEXT("\n")
        + TEXT("[/Script/BanChatCommands.BanChatCommandsConfig]\n")
        + TEXT("; +AdminEosPUIDs=YOUR_EOS_PUID_HERE\n")
        + TEXT("; +ModeratorEosPUIDs=MODERATOR_EOS_PUID_HERE\n")
        + TEXT("BanListPageSize=10\n")
        + TEXT("ModBanDurationMinutes=30\n")
        + TEXT("; MaxModMuteDurationMinutes=0  — 0 means no ceiling for moderators.\n")
        + TEXT("MaxModMuteDurationMinutes=0\n")
        + TEXT("; bAllowModNotes=False  — set True to let moderators use /note and /notes.\n")
        + TEXT("bAllowModNotes=False\n");

    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DefaultIniPath));

    if (FFileHelper::SaveStringToFile(Content, *DefaultIniPath))
    {
        UE_LOG(LogBanChatCommands, Log,
            TEXT("BanChatCommands: Restored annotated default config at '%s'."), *DefaultIniPath);
    }
    else
    {
        UE_LOG(LogBanChatCommands, Warning,
            TEXT("BanChatCommands: Could not write default config to '%s'."), *DefaultIniPath);
    }
}

void FBanChatCommandsModule::ShutdownModule()
{
    FTSTicker::GetCoreTicker().RemoveTicker(ConfigPollHandle);
    FTSTicker::GetCoreTicker().RemoveTicker(MuteExpiryHandle);

    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module shut down."));
}

#undef LOCTEXT_NAMESPACE

// ─────────────────────────────────────────────────────────────────────────────
//  Mute expiry ticker
// ─────────────────────────────────────────────────────────────────────────────

bool FBanChatCommandsModule::OnMuteExpiryTick(float /*DeltaTime*/)
{
    // Walk all game instances and tick UMuteRegistry on each.
    // In practice there is one game instance on a dedicated server, but this
    // approach is future-proof if multiple worlds/instances ever run.
    for (const FWorldContext& WCtx : GEngine->GetWorldContexts())
    {
        UGameInstance* GI = WCtx.OwningGameInstance;
        if (!GI) continue;
        UMuteRegistry* MuteReg = GI->GetSubsystem<UMuteRegistry>();
        if (MuteReg) MuteReg->TickExpiry();
    }
    return true; // keep ticking
}

IMPLEMENT_MODULE(FBanChatCommandsModule, BanChatCommands)
