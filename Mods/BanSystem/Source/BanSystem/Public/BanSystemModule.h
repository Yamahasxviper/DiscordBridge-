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
    /** On first startup, writes non-default settings to Saved/Config/<Platform>/BanSystem.ini.
     *  UE config layering reads that file with higher priority than the mod's own
     *  Config/DefaultBanSystem.ini, so settings survive an Alpakit dev deploy that
     *  deletes and recreates the mod directory. */
    static void BackupConfigIfNeeded();
};
