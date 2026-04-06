// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/index.ts

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

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
};
