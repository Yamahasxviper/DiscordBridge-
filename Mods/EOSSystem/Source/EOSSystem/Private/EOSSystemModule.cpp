// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSSystemModule.h"
#include "EOSSDKLoader.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSystem, Log, All);

void FEOSSystemModule::StartupModule()
{
    UE_LOG(LogEOSSystem, Log, TEXT("EOSSystem module starting up — loading EOS SDK DLL..."));
    if (FEOSSDKLoader::Get().Load())
    {
        UE_LOG(LogEOSSystem, Log, TEXT("EOSSystem: EOS SDK DLL loaded successfully."));
    }
    else
    {
        UE_LOG(LogEOSSystem, Warning, TEXT("EOSSystem: EOS SDK DLL not found — EOS features will be disabled."));
    }
}

void FEOSSystemModule::ShutdownModule()
{
    FEOSSDKLoader::Get().Unload();
    UE_LOG(LogEOSSystem, Log, TEXT("EOSSystem module shut down."));
}

IMPLEMENT_MODULE(FEOSSystemModule, EOSSystem)
