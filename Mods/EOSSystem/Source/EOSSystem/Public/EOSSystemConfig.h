// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "EOSSystemConfig.generated.h"

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

    // ── Features ──────────────────────────────────────────────────────────
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableConnect      = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableAuth         = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableSessions     = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableAntiCheat    = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableSanctions    = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableMetrics      = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableReports      = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableStats        = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableAchievements = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableLeaderboards = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableStorage      = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableFriends      = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnablePresence     = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableEcom         = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableLobby        = false;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Features") bool bEnableRTC          = false;

    // ── Sessions ──────────────────────────────────────────────────────────
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions") FString SessionName            = TEXT("ServerSession");
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions") FString BucketId               = TEXT("Game:Live");
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions", meta=(Tooltip="Public IP:Port. Empty = skip HostAddress")) FString ServerIpAddress;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions") uint32  MaxPlayers             = 64;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions") bool    bSanctionsEnabled      = true;
    UPROPERTY(config, EditAnywhere, Category="EOS System|Sessions") bool    bJoinInProgressAllowed = true;

    // ── AntiCheat ─────────────────────────────────────────────────────────
    UPROPERTY(config, EditAnywhere, Category="EOS System|AntiCheat") FString AntiCheatServerName = TEXT("EOSServer");
};
