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

    /** Restores documentation comments to Config/DefaultBanSystem.ini inside
     *  the deployed plugin directory.  UE's staging pipeline strips comments
     *  from Default*.ini via FConfigFile; this re-applies them on the first
     *  server start after each mod update. */
    static void RestoreDefaultConfigDocs();
};
