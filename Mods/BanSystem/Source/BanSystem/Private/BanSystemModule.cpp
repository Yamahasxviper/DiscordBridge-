// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/index.ts

#include "BanSystemModule.h"
#include "BanDatabase.h"
#include "BanSystemConfig.h"
#include "PlayerWarningRegistry.h"
#include "PlayerSessionRegistry.h"
#include "ScheduledBanRegistry.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanSystem, Log, All);

#define LOCTEXT_NAMESPACE "FBanSystemModule"

void FBanSystemModule::StartupModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module starting."));
    // UBanDatabase, UBanRestApi, and UBanEnforcer are UGameInstanceSubsystem
    // subclasses and initialise automatically when the game instance starts.
    // No manual setup is required here.

    // Restore the annotated Default*.ini so operators can always read the
    // setting descriptions even after a mod update or fresh install.
    RestoreDefaultConfigIfNeeded();

    // On every server start, write a backup to Saved/BanSystem/BanSystem.ini.
    // That folder is never touched by mod updates so settings survive any wipe
    // of the mod directory.
    BackupConfigIfNeeded();

    // Start the scheduled backup ticker if BackupIntervalHours > 0.
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (Cfg && Cfg->BackupIntervalHours > 0.0f)
    {
        BackupAccumulatedSeconds = 0.0f;
        BackupTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FBanSystemModule::OnBackupTick),
            1.0f);
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: scheduled backup enabled — every %.1f hour(s)."),
            Cfg->BackupIntervalHours);
    }

    // Start the scheduled prune ticker if PruneIntervalHours > 0.
    if (Cfg && Cfg->PruneIntervalHours > 0.0f)
    {
        PruneAccumulatedSeconds = 0.0f;
        PruneTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FBanSystemModule::OnPruneTick),
            1.0f);
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: scheduled prune enabled — every %.1f hour(s)."),
            Cfg->PruneIntervalHours);
    }

    // Start the session retention ticker if SessionRetentionDays > 0.
    // Runs independently from the ban-prune ticker: once every 24 hours.
    if (Cfg && Cfg->SessionRetentionDays > 0)
    {
        SessionPruneAccumulatedSeconds = 0.0f;
        SessionPruneTickHandle = FTSTicker::GetCoreTicker().AddTicker(
            FTickerDelegate::CreateRaw(this, &FBanSystemModule::OnSessionPruneTick),
            1.0f);
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: session retention enabled — keeping records for %d day(s)."),
            Cfg->SessionRetentionDays);
    }

    // Always start the scheduled-ban ticker (every 30 s).
    // UScheduledBanRegistry is a GameInstanceSubsystem and initialises automatically,
    // but we route the tick through the module ticker so the registry stays active
    // even when no game-instance tick is running at module startup.
    ScheduledBanTickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateRaw(this, &FBanSystemModule::OnScheduledBanTick),
        1.0f);
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem: scheduled-ban ticker started."));

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
        + TEXT("MaxBackups=")   + FString::FromInt(Cfg->MaxBackups)  + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- REST API Authentication ---------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Optional API key for authenticating mutating REST API requests (POST, DELETE, PATCH).\n")
        + TEXT("; When non-empty, all mutating endpoints require the header: X-Api-Key: <value>\n")
        + TEXT("; Read-only endpoints (GET) are never gated.\n")
        + TEXT("; Leave empty to disable API key authentication (default; only safe on a firewalled server).\n")
        + TEXT("RestApiKey=")   + Cfg->RestApiKey + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Discord Notifications -----------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Optional Discord webhook URL for ban/unban/warn/kick notifications.\n")
        + TEXT("; When set, BanDiscordNotifier posts an embed to this URL whenever a ban is\n")
        + TEXT("; created or removed, a warning is issued, or a player is kicked.\n")
        + TEXT("; Leave empty to disable Discord notifications (default).\n")
        + TEXT("DiscordWebhookUrl=") + Cfg->DiscordWebhookUrl + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Auto-ban on Warnings ------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Number of warnings before an automatic permanent ban is issued (default: 0 = disabled).\n")
        + TEXT("; When a player reaches this many warnings via /warn, they are automatically\n")
        + TEXT("; permanently banned with reason \"Auto-banned: reached warning threshold\".\n")
        + TEXT("; Set to 0 to disable auto-banning on warnings.\n")
        + TEXT("AutoBanWarnCount=")   + FString::FromInt(Cfg->AutoBanWarnCount)   + TEXT("\n")
        + TEXT(";\n")
        + TEXT("; Duration in minutes for the auto-ban issued when AutoBanWarnCount is reached (default: 0 = permanent).\n")
        + TEXT("; Set to 0 for a permanent auto-ban.\n")
        + TEXT("AutoBanWarnMinutes=") + FString::FromInt(Cfg->AutoBanWarnMinutes) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Warning Escalation Tiers -------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Multi-tier automatic bans based on warning count.\n")
        + TEXT("; Each tier: WarnCount=<N>,DurationMinutes=<M>  (0 = permanent ban)\n")
        + TEXT("; Example:\n")
        + TEXT(";   +WarnEscalationTiers=(WarnCount=2,DurationMinutes=30)\n")
        + TEXT(";   +WarnEscalationTiers=(WarnCount=3,DurationMinutes=1440)\n")
        + TEXT(";   +WarnEscalationTiers=(WarnCount=5,DurationMinutes=0)\n")
        + TEXT("\n")
        + TEXT("; -- Session Retention -------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Number of days to retain player session records (default: 0 = keep forever).\n")
        + TEXT("SessionRetentionDays=") + FString::FromInt(Cfg->SessionRetentionDays) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Scheduled Backup --------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Interval in hours between automatic database backups (default: 0 = disabled).\n")
        + TEXT("BackupIntervalHours=") + FString::SanitizeFloat(Cfg->BackupIntervalHours) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Ban Expiry Notifications ------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; When true, post a Discord notification when a temporary ban expires (default: false).\n")
        + TEXT("bNotifyBanExpired=") + (Cfg->bNotifyBanExpired ? TEXT("true") : TEXT("false")) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- Auto-Prune --------------------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; Interval in hours between automatic expired-ban prune runs (0 = disabled).\n")
        + TEXT("PruneIntervalHours=") + FString::SanitizeFloat(Cfg->PruneIntervalHours) + TEXT("\n")
        + TEXT("\n")
        + TEXT("; -- WebSocket Push Events ---------------------------------------------------\n")
        + TEXT(";\n")
        + TEXT("; When true, BanSystem pushes live JSON events to WebSocketPushUrl via SMLWebSocket.\n")
        + TEXT("bPushEventsToWebSocket=") + (Cfg->bPushEventsToWebSocket ? TEXT("true") : TEXT("false")) + TEXT("\n")
        + TEXT("WebSocketPushUrl=") + Cfg->WebSocketPushUrl + TEXT("\n");

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

void FBanSystemModule::RestoreDefaultConfigIfNeeded()
{
    const FString DefaultIniPath = FPaths::Combine(
        FPaths::ProjectDir(),
        TEXT("Mods"), TEXT("BanSystem"),
        TEXT("Config"), TEXT("DefaultBanSystem.ini"));

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
        + TEXT("; ── Database ──────────────────────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Absolute path to the JSON ban file.\n")
        + TEXT("; Leave empty to use the default: <ProjectSaved>/BanSystem/bans.json\n")
        + TEXT("; On Linux this is typically: /home/<user>/.config/Epic/FactoryGame/Saved/BanSystem/bans.json\n")
        + TEXT("DatabasePath=\n")
        + TEXT("\n")
        + TEXT("; ── REST Management API ───────────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Port for the local HTTP management API (default: 3000).\n")
        + TEXT("; Mirrors the Tools/BanSystem API_PORT setting.\n")
        + TEXT(";\n")
        + TEXT("; The API binds to all interfaces (0.0.0.0).\n")
        + TEXT("; Restrict external access with your server firewall if needed.\n")
        + TEXT(";\n")
        + TEXT("; Set to 0 to disable the REST API entirely.\n")
        + TEXT("RestApiPort=3000\n")
        + TEXT("\n")
        + TEXT("; ── Backup ────────────────────────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Number of automatic database backups to keep (default: 5).\n")
        + TEXT("; A backup is created on demand via POST /bans/backup.\n")
        + TEXT("; Older backups beyond this limit are deleted automatically.\n")
        + TEXT("MaxBackups=5\n")
        + TEXT("\n")
        + TEXT("; ── REST API Authentication ───────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Optional API key for authenticating mutating REST API requests (POST, DELETE, PATCH).\n")
        + TEXT("; When non-empty, all mutating endpoints require the header: X-Api-Key: <value>\n")
        + TEXT("; Read-only endpoints (GET) are never gated.\n")
        + TEXT("; Leave empty to disable API key authentication (default; only safe on a firewalled server).\n")
        + TEXT("RestApiKey=\n")
        + TEXT("\n")
        + TEXT("; ── Discord Notifications ─────────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Optional Discord webhook URL for ban/unban/warn/kick notifications.\n")
        + TEXT("; When set, BanDiscordNotifier posts an embed to this URL whenever a ban is\n")
        + TEXT("; created or removed, a warning is issued, or a player is kicked.\n")
        + TEXT("; Leave empty to disable Discord notifications (default).\n")
        + TEXT("DiscordWebhookUrl=\n")
        + TEXT("\n")
        + TEXT("; ── Auto-ban on Warnings ──────────────────────────────────────────────────────\n")
        + TEXT(";\n")
        + TEXT("; Number of warnings before an automatic permanent ban is issued (default: 0 = disabled).\n")
        + TEXT("; When a player reaches this many warnings via /warn, they are automatically\n")
        + TEXT("; permanently banned with reason \"Auto-banned: reached warning threshold\".\n")
        + TEXT("; Set to 0 to disable auto-banning on warnings.\n")
        + TEXT("AutoBanWarnCount=0\n")
        + TEXT(";\n")
        + TEXT("; Duration in minutes for the auto-ban issued when AutoBanWarnCount is reached (default: 0 = permanent).\n")
        + TEXT("; Set to 0 for a permanent auto-ban.\n")
        + TEXT("AutoBanWarnMinutes=0\n");

    PlatformFile.CreateDirectoryTree(*FPaths::GetPath(DefaultIniPath));

    if (FFileHelper::SaveStringToFile(Content, *DefaultIniPath))
    {
        UE_LOG(LogBanSystem, Log,
            TEXT("BanSystem: Restored annotated default config at '%s'."), *DefaultIniPath);
    }
    else
    {
        UE_LOG(LogBanSystem, Warning,
            TEXT("BanSystem: Could not write default config to '%s'."), *DefaultIniPath);
    }
}

void FBanSystemModule::ShutdownModule()
{
    if (BackupTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(BackupTickHandle);
        BackupTickHandle.Reset();
    }
    if (PruneTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PruneTickHandle);
        PruneTickHandle.Reset();
    }
    if (SessionPruneTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(SessionPruneTickHandle);
        SessionPruneTickHandle.Reset();
    }
    if (ScheduledBanTickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(ScheduledBanTickHandle);
        ScheduledBanTickHandle.Reset();
    }
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

bool FBanSystemModule::OnBackupTick(float DeltaTime)
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || Cfg->BackupIntervalHours <= 0.0f) return true;

    BackupAccumulatedSeconds += DeltaTime;
    const float IntervalSeconds = Cfg->BackupIntervalHours * 3600.0f;
    if (BackupAccumulatedSeconds < IntervalSeconds) return true;

    BackupAccumulatedSeconds = 0.0f;

    // Find the game instance to get the UBanDatabase subsystem.
    // We walk the GEngine world list to find the first server world.
    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            UWorld* World = Ctx.World();
            if (!World) continue;
            if (World->GetNetMode() != NM_DedicatedServer && World->GetNetMode() != NM_ListenServer) continue;
            UGameInstance* GI = World->GetGameInstance();
            if (!GI) continue;
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) continue;

            const FString BackupDir = FPaths::GetPath(DB->GetDatabasePath()) / TEXT("backups");
            const FString Dest = DB->Backup(BackupDir, Cfg ? Cfg->MaxBackups : 5);
            if (!Dest.IsEmpty())
            {
                UE_LOG(LogBanSystem, Log, TEXT("BanSystem: scheduled backup written to '%s'"), *Dest);
            }
            else
            {
                UE_LOG(LogBanSystem, Warning, TEXT("BanSystem: scheduled backup failed"));
            }
            break;
        }
    }

    return true; // keep ticking
}

bool FBanSystemModule::OnPruneTick(float DeltaTime)
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || Cfg->PruneIntervalHours <= 0.0f) return true;

    PruneAccumulatedSeconds += DeltaTime;
    const float IntervalSeconds = Cfg->PruneIntervalHours * 3600.0f;
    if (PruneAccumulatedSeconds < IntervalSeconds) return true;

    PruneAccumulatedSeconds = 0.0f;

    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            UWorld* World = Ctx.World();
            if (!World) continue;
            if (World->GetNetMode() != NM_DedicatedServer && World->GetNetMode() != NM_ListenServer) continue;
            UGameInstance* GI = World->GetGameInstance();
            if (!GI) continue;
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) continue;

            const int32 Pruned = DB->PruneExpiredBans();
            if (Pruned > 0)
            {
                UE_LOG(LogBanSystem, Log,
                    TEXT("BanSystem: auto-prune removed %d expired ban(s)."), Pruned);
            }

            // Also prune expired timed warnings.
            if (UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>())
                WarnReg->PruneExpiredWarnings();

            break;
        }
    }

    return true; // keep ticking
}

bool FBanSystemModule::OnSessionPruneTick(float DeltaTime)
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || Cfg->SessionRetentionDays <= 0) return true;

    // Run once per 24 hours.
    static constexpr float SessionPruneIntervalSeconds = 24.0f * 3600.0f;
    SessionPruneAccumulatedSeconds += DeltaTime;
    if (SessionPruneAccumulatedSeconds < SessionPruneIntervalSeconds) return true;

    SessionPruneAccumulatedSeconds = 0.0f;

    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            UWorld* World = Ctx.World();
            if (!World) continue;
            if (World->GetNetMode() != NM_DedicatedServer && World->GetNetMode() != NM_ListenServer) continue;
            UGameInstance* GI = World->GetGameInstance();
            if (!GI) continue;
            UPlayerSessionRegistry* Reg = GI->GetSubsystem<UPlayerSessionRegistry>();
            if (!Reg) continue;

            const int32 Pruned = Reg->PruneOldRecords(Cfg->SessionRetentionDays);
            if (Pruned > 0)
            {
                UE_LOG(LogBanSystem, Log,
                    TEXT("BanSystem: session-retention pruned %d record(s) older than %d day(s)."),
                    Pruned, Cfg->SessionRetentionDays);
            }
            break;
        }
    }

    return true; // keep ticking
}

bool FBanSystemModule::OnScheduledBanTick(float DeltaTime)
{
    if (GEngine)
    {
        for (const FWorldContext& Ctx : GEngine->GetWorldContexts())
        {
            UWorld* World = Ctx.World();
            if (!World) continue;
            if (World->GetNetMode() != NM_DedicatedServer && World->GetNetMode() != NM_ListenServer) continue;
            UGameInstance* GI = World->GetGameInstance();
            if (!GI) continue;
            UScheduledBanRegistry* Reg = GI->GetSubsystem<UScheduledBanRegistry>();
            if (Reg)
                Reg->Tick(DeltaTime);
            break;
        }
    }
    return true; // keep ticking
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
