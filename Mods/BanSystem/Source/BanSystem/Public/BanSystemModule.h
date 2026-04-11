// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/index.ts

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

class BANSYSTEM_API FBanSystemModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    /** On every server start, writes current settings to Saved/BanSystem/BanSystem.ini.
     *  That folder is never touched by mod updates so settings survive any wipe
     *  of the mod directory. */
    static void BackupConfigIfNeeded();

    /** Restores Mods/BanSystem/Config/DefaultBanSystem.ini with the full annotated
     *  template if the file is missing or has been stripped of comment lines by
     *  Alpakit's staging step. */
    static void RestoreDefaultConfigIfNeeded();

    /** Ticker callback that fires at BackupIntervalHours to trigger a scheduled backup. */
    bool OnBackupTick(float DeltaTime);

    /** Ticker callback that fires at PruneIntervalHours to prune expired bans. */
    bool OnPruneTick(float DeltaTime);

    /** Ticker callback that fires daily to prune old player session records
     *  when SessionRetentionDays > 0. */
    bool OnSessionPruneTick(float DeltaTime);

    /** Ticker callback that fires every 30 s to apply due scheduled bans. */
    bool OnScheduledBanTick(float DeltaTime);

    FTSTicker::FDelegateHandle BackupTickHandle;
    FTSTicker::FDelegateHandle PruneTickHandle;
    FTSTicker::FDelegateHandle SessionPruneTickHandle;
    FTSTicker::FDelegateHandle ScheduledBanTickHandle;
    /** Accumulated time since the last scheduled backup (seconds). */
    float BackupAccumulatedSeconds = 0.0f;
    /** Accumulated time since the last scheduled prune (seconds). */
    float PruneAccumulatedSeconds = 0.0f;
    /** Accumulated time since the last session-records prune (seconds). */
    float SessionPruneAccumulatedSeconds = 0.0f;
};
