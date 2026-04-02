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
};
