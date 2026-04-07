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
 *   Saved/BanSystem/BanSystem.ini
 * That file is never touched by mod updates or Alpakit dev deploys.
 * BanSystem writes the current settings there on every server start so your
 * configuration survives any wipe of the mod directory.
 *
 * Settings are also read from the mod's own Config/DefaultBanSystem.ini, but
 * that file may be overwritten when the mod is updated.  Use it only to check
 * the current defaults, not as the long-term home for your configuration.
 *
 * Both files use the same section header:
 *   [/Script/BanSystem.BanSystemConfig]
 *
 * Example Saved/BanSystem/BanSystem.ini override:
 *
 *   [/Script/BanSystem.BanSystemConfig]
 *   DatabasePath=/home/user/bans.json
 *   RestApiPort=3001
 *   MaxBackups=10
 */
UCLASS(Config = BanSystem, meta = (DisplayName = "Ban System"))
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

    /**
     * Optional API key for authenticating mutating REST API requests (POST, DELETE, PATCH).
     * When non-empty, all mutating endpoints require the header: X-Api-Key: <value>
     * Read-only endpoints (GET) are never gated.
     * Leave empty to disable API key authentication (default; only safe on a firewalled server).
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    FString RestApiKey;

    /**
     * Optional Discord webhook URL for ban/unban/warn/kick notifications.
     * When set, BanDiscordNotifier posts an embed to this URL whenever a ban is
     * created or removed, a warning is issued, or a player is kicked.
     * Leave empty to disable Discord notifications (default).
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    FString DiscordWebhookUrl;

    /**
     * Number of warnings before an automatic permanent ban is issued (default: 0 = disabled).
     * When a player reaches this many warnings via /warn, they are automatically permanently
     * banned with reason "Auto-banned: reached warning threshold".
     * Set to 0 to disable auto-banning on warnings.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 AutoBanWarnCount = 0;

    /**
     * Duration in minutes for the auto-ban issued when AutoBanWarnCount is reached (default: 0 = permanent).
     * Set to 0 for a permanent auto-ban.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 AutoBanWarnMinutes = 0;

    /** Returns the singleton config instance. */
    static const UBanSystemConfig* Get();
};
