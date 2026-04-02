// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EOSSystemConfig.generated.h"

/**
 * Configuration for the EOSSystem mod.
 * Trimmed to the fields required by the ban system:
 *   • EOS credentials (ProductId, SandboxId, DeploymentId, ClientId, ClientSecret)
 *   • Platform options (overlay, cache dir, log level)
 *   • Connect subsystem toggle (bEnableConnect)
 *
 * Edit in <ServerRoot>/FactoryGame/Mods/EOSSystem/Config/DefaultEOSSystem.ini
 * under the [/Script/EOSSystem.EOSSystemConfig] section.
 */
UCLASS(config=Game, defaultconfig, meta=(DisplayName="EOS System"))
class EOSSYSTEM_API UEOSSystemConfig : public UDeveloperSettings
{
    GENERATED_BODY()
public:
    UEOSSystemConfig() { SectionName = TEXT("EOSSystem"); }

    // ── Credentials ───────────────────────────────────────────────────────
    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials", meta=(Tooltip="EOS Product ID from dev portal"))
    FString ProductId;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials", meta=(Tooltip="EOS Sandbox ID (Live/Stage/Dev)"))
    FString SandboxId;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials")
    FString DeploymentId;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials", meta=(Tooltip="Server client ID"))
    FString ClientId;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials", meta=(Tooltip="Server client secret — keep private"))
    FString ClientSecret;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Credentials", meta=(Tooltip="64-char hex encryption key. Leave empty to auto-generate."))
    FString EncryptionKey;

    // ── Platform ──────────────────────────────────────────────────────────
    UPROPERTY(config, EditAnywhere, Category="EOS System|Platform")
    bool bIsServer = true;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Platform", meta=(Tooltip="Recommended true on dedicated server"))
    bool bDisableOverlay = true;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Platform", meta=(Tooltip="0 = unlimited"))
    uint32 TickBudgetMs = 0;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Platform", meta=(Tooltip="Override cache dir; empty = Saved/EOSSystemCache"))
    FString CacheDirectory;

    UPROPERTY(config, EditAnywhere, Category="EOS System|Platform", meta=(Tooltip="Off/Fatal/Error/Warning/Info/Verbose/VeryVerbose"))
    FString LogLevel = TEXT("Warning");

    // ── Connect ───────────────────────────────────────────────────────────
    // Enable the EOS Connect subsystem for Steam64↔PUID cross-platform ban mapping.
    UPROPERTY(config, EditAnywhere, Category="EOS System|Connect")
    bool bEnableConnect = true;
};
