// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/index.ts

#include "BanSystemModule.h"
#include "BanDatabase.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanSystem, Log, All);

#define LOCTEXT_NAMESPACE "FBanSystemModule"

void FBanSystemModule::StartupModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module starting."));
    // UBanDatabase, UBanRestApi, and UBanEnforcer are UGameInstanceSubsystem
    // subclasses and initialise automatically when the game instance starts.
    // No manual setup is required here.
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module started."));
}

void FBanSystemModule::ShutdownModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
