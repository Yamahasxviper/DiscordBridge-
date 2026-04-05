// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BanSystemConfig.generated.h"

/**
 * UBanSystemConfig
 *
 * Per-server configuration for BanSystem.
 *
 * RECOMMENDED: manage settings in the persistent override file:
 *   Saved/Config/<Platform>/BanSystem.ini
 * That file is never touched by mod updates or Alpakit dev deploys and is
 * auto-created on the first startup when any setting differs from its default.
 *
 * Settings are also read from the mod's own Config/DefaultBanSystem.ini, but
 * that file may be overwritten when the mod is updated.  Use it only to check
 * the current defaults, not as the long-term home for your configuration.
 *
 * Both files use the same section header:
 *   [/Script/BanSystem.BanSystemConfig]
 *
 * Example Saved/Config/<Platform>/BanSystem.ini override:
 *
 *   [/Script/BanSystem.BanSystemConfig]
 *   DatabasePath=/home/user/bans.json
 *   RestApiPort=3001
 *   MaxBackups=10
 */
UCLASS(Config = BanSystem, DefaultConfig, meta = (DisplayName = "Ban System"))
class BANSYSTEM_API UBanSystemConfig : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Absolute path to the JSON ban file.
     * Leave empty to use the default: <ProjectSaved>/BanSystem/bans.json
     * On Linux this is typically:
     *   /home/<user>/.config/Epic/FactoryGame/Saved/BanSystem/bans.json
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    FString DatabasePath;

    /**
     * Port for the local HTTP management REST API (default: 3000).
     * The API binds to all interfaces (0.0.0.0).
     * Restrict external access with your server firewall if needed.
     * Set to 0 to disable the REST API entirely.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 RestApiPort = 3000;

    /**
     * Maximum number of automatic database backups to keep (default: 5).
     * A backup is created on demand via POST /bans/backup.
     * Older backups beyond this limit are deleted automatically.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 MaxBackups = 5;

    /** Returns the singleton config instance. */
    static const UBanSystemConfig* Get();
};
