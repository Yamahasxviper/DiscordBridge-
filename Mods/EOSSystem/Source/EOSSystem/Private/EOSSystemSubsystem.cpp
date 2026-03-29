// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSSystemSubsystem.h"
#include "EOSSDKLoader.h"
#include "EOSSystemConfig.h"
#include "Engine/GameInstance.h"
#include "Misc/Paths.h"
#include "Math/UnrealMathUtility.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSystemSub, Log, All);

void UEOSSystemSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (!SDK.IsLoaded()) { UE_LOG(LogEOSSystemSub, Warning, TEXT("EOS SDK not loaded — skipping platform init.")); return; }

    const UEOSSystemConfig* Cfg = GetDefault<UEOSSystemConfig>();
    if (!Cfg || Cfg->ProductId.IsEmpty() || Cfg->SandboxId.IsEmpty() || Cfg->DeploymentId.IsEmpty()
             || Cfg->ClientId.IsEmpty()  || Cfg->ClientSecret.IsEmpty())
    {
        UE_LOG(LogEOSSystemSub, Error, TEXT("EOSSystem: missing credentials in DefaultGame.ini [EOSSystem] — platform will NOT be created."));
        return;
    }

    // Set up logging
    if (SDK.fp_EOS_Logging_SetCallback) SDK.fp_EOS_Logging_SetCallback(&UEOSSystemSubsystem::OnEOSLog);
    if (SDK.fp_EOS_Platform_SetLogLevel) SDK.fp_EOS_Platform_SetLogLevel(EOS_LC_ALL_CATEGORIES, MapLogLevel(Cfg->LogLevel));

    // EOS_Initialize
    EOS_InitializeOptions InitOpts = {};
    InitOpts.ApiVersion     = EOS_INITIALIZE_API_LATEST;
    InitOpts.ProductName    = "EOSSystem";
    InitOpts.ProductVersion = "1.0";
    if (SDK.fp_EOS_Initialize)
    {
        EOS_EResult InitRes = SDK.fp_EOS_Initialize(&InitOpts);
        if (InitRes == EOS_Success)           bInitialisedByUs = true;
        else if (InitRes != (EOS_EResult)22 /*EOS_AlreadyConfigured*/)
        {
            UE_LOG(LogEOSSystemSub, Error, TEXT("EOS_Initialize failed: %s"), *FEOSSDKLoader::ResultToString(InitRes));
            return;
        }
    }

    // Build EOS_Platform_Options
    EOS_Platform_ClientCredentials Creds = {};
    Creds.ClientId     = TCHAR_TO_UTF8(*Cfg->ClientId);
    Creds.ClientSecret = TCHAR_TO_UTF8(*Cfg->ClientSecret);

    // Cache dir
    CacheDirStorage = Cfg->CacheDirectory.IsEmpty()
        ? (FPaths::ProjectSavedDir() / TEXT("EOSSystemCache"))
        : Cfg->CacheDirectory;
    FPaths::NormalizeDirectoryName(CacheDirStorage);

    // Encryption key (64 hex chars required)
    EncKeyStorage = Cfg->EncryptionKey;
    if (EncKeyStorage.Len() != 64)
    {
        FString Gen;
        Gen.Reserve(64);
        const TCHAR* Hex = TEXT("0123456789abcdef");
        for (int32 i = 0; i < 64; ++i) Gen.AppendChar(Hex[FMath::RandRange(0,15)]);
        EncKeyStorage = Gen;
        UE_LOG(LogEOSSystemSub, Log, TEXT("EOSSystem: auto-generated encryption key."));
    }

    uint64_t Flags = 0;
    if (Cfg->bDisableOverlay) Flags |= EOS_PF_DISABLE_OVERLAY;

    EOS_Platform_Options PlatOpts = {};
    PlatOpts.ApiVersion               = EOS_PLATFORM_OPTIONS_API_LATEST;
    PlatOpts.Reserved                 = nullptr;
    PlatOpts.ProductId                = TCHAR_TO_UTF8(*Cfg->ProductId);
    PlatOpts.SandboxId                = TCHAR_TO_UTF8(*Cfg->SandboxId);
    PlatOpts.ClientCredentials        = Creds;
    PlatOpts.bIsServer                = Cfg->bIsServer ? EOS_TRUE : EOS_FALSE;
    PlatOpts.EncryptionKey            = TCHAR_TO_UTF8(*EncKeyStorage);
    PlatOpts.OverrideCountryCode      = nullptr;
    PlatOpts.OverrideLocaleCode       = nullptr;
    PlatOpts.DeploymentId             = TCHAR_TO_UTF8(*Cfg->DeploymentId);
    PlatOpts.Flags                    = Flags;
    PlatOpts.CacheDirectory           = TCHAR_TO_UTF8(*CacheDirStorage);
    PlatOpts.TickBudgetInMilliseconds = Cfg->TickBudgetMs;
    PlatOpts.RTCOptions               = nullptr;

    if (!SDK.fp_EOS_Platform_Create) { UE_LOG(LogEOSSystemSub, Error, TEXT("fp_EOS_Platform_Create is null")); return; }
    PlatformHandle = SDK.fp_EOS_Platform_Create(&PlatOpts);
    if (!PlatformHandle) { UE_LOG(LogEOSSystemSub, Error, TEXT("EOS_Platform_Create returned null.")); return; }

    bPlatformReady = true;
    UE_LOG(LogEOSSystemSub, Log, TEXT("EOS Platform created successfully."));
    OnEOSInitialized.Broadcast(true);
}

void UEOSSystemSubsystem::Deinitialize()
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (PlatformHandle && SDK.fp_EOS_Platform_Release)
        SDK.fp_EOS_Platform_Release(PlatformHandle);
    PlatformHandle = nullptr;

    if (bInitialisedByUs && SDK.IsLoaded() && SDK.fp_EOS_Shutdown)
        SDK.fp_EOS_Shutdown();

    bPlatformReady   = false;
    bInitialisedByUs = false;
    Super::Deinitialize();
}

void UEOSSystemSubsystem::Tick(float DeltaTime)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (PlatformHandle && SDK.fp_EOS_Platform_Tick)
        SDK.fp_EOS_Platform_Tick(PlatformHandle);
}

TStatId UEOSSystemSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UEOSSystemSubsystem, STATGROUP_Tickables);
}

FString UEOSSystemSubsystem::GetProductId() const    { return GetDefault<UEOSSystemConfig>()->ProductId; }
FString UEOSSystemSubsystem::GetDeploymentId() const { return GetDefault<UEOSSystemConfig>()->DeploymentId; }

void UEOSSystemSubsystem::SetLogLevel(const FString& Level)
{
    FEOSSDKLoader& SDK = FEOSSDKLoader::Get();
    if (SDK.fp_EOS_Platform_SetLogLevel)
        SDK.fp_EOS_Platform_SetLogLevel(EOS_LC_ALL_CATEGORIES, MapLogLevel(Level));
}

void EOS_CALL UEOSSystemSubsystem::OnEOSLog(const EOS_LogMessage* Msg)
{
    if (!Msg || !Msg->Message) return;
    const FString Text = UTF8_TO_TCHAR(Msg->Message);
    switch (Msg->Level)
    {
        case EOS_LOG_Fatal:   UE_LOG(LogEOSSystemSub, Fatal,   TEXT("[EOS] %s"), *Text); break;
        case EOS_LOG_Error:   UE_LOG(LogEOSSystemSub, Error,   TEXT("[EOS] %s"), *Text); break;
        case EOS_LOG_Warning: UE_LOG(LogEOSSystemSub, Warning, TEXT("[EOS] %s"), *Text); break;
        default:              UE_LOG(LogEOSSystemSub, Log,     TEXT("[EOS] %s"), *Text); break;
    }
}

EOS_ELogLevel UEOSSystemSubsystem::MapLogLevel(const FString& Level)
{
    if (Level.Equals(TEXT("Off"),          ESearchCase::IgnoreCase)) return EOS_LOG_Off;
    if (Level.Equals(TEXT("Fatal"),        ESearchCase::IgnoreCase)) return EOS_LOG_Fatal;
    if (Level.Equals(TEXT("Error"),        ESearchCase::IgnoreCase)) return EOS_LOG_Error;
    if (Level.Equals(TEXT("Info"),         ESearchCase::IgnoreCase)) return EOS_LOG_Info;
    if (Level.Equals(TEXT("Verbose"),      ESearchCase::IgnoreCase)) return EOS_LOG_Verbose;
    if (Level.Equals(TEXT("VeryVerbose"),  ESearchCase::IgnoreCase)) return EOS_LOG_VeryVerbose;
    return EOS_LOG_Warning; // default
}
