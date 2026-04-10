// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BanSystemConfig.generated.h"

/**
 * A single warning-escalation tier.
 * When a player accumulates WarnCount or more warnings, they are automatically
 * banned for DurationMinutes minutes (0 = permanent).
 */
USTRUCT(BlueprintType)
struct BANSYSTEM_API FWarnEscalationTier
{
    GENERATED_BODY()

    /** Warning count threshold that triggers this tier. */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 WarnCount = 0;

    /** Ban duration in minutes (0 = permanent). */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 DurationMinutes = 0;
};

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

    /**
     * Warning escalation tiers for automatic bans based on warning count.
     *
     * Each tier specifies a warning count threshold and the ban duration in minutes
     * (0 = permanent ban). When a player's total warning count reaches a tier's
     * threshold, they are automatically banned for the corresponding duration.
     *
     * Tiers are evaluated in ascending order; the highest matching tier wins.
     * AutoBanWarnCount / AutoBanWarnMinutes act as a simple single-tier fallback
     * when this array is empty.
     *
     * Example (DefaultBanSystem.ini):
     *   +WarnEscalationTiers=(WarnCount=2,DurationMinutes=30)
     *   +WarnEscalationTiers=(WarnCount=3,DurationMinutes=1440)
     *   +WarnEscalationTiers=(WarnCount=5,DurationMinutes=0)
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    TArray<FWarnEscalationTier> WarnEscalationTiers;

    /**
     * Number of days to retain player session records (default: 0 = keep forever).
     * Records older than this value are pruned by POST /players/prune.
     * Set to 0 to disable automatic pruning by age (records are only removed by the
     * explicit prune endpoint).
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 SessionRetentionDays = 0;

    /**
     * Interval in hours between automatic database backups (default: 0 = disabled).
     * When non-zero, BanSystem schedules a recurring timer that calls
     * UBanDatabase::Backup() every BackupIntervalHours.
     * Set to 0 to rely solely on the manual POST /bans/backup endpoint and the
     * one-time startup backup written by BanSystemModule.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    float BackupIntervalHours = 0.0f;

    /**
     * When true, BanSystem posts a Discord webhook notification whenever a
     * temporary ban expires and the player is allowed to reconnect.
     * Only has an effect when DiscordWebhookUrl is set.
     * Default: false.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    bool bNotifyBanExpired = false;

    /**
     * Interval in hours between automatic expired-ban prune runs (default: 0 = disabled).
     * When non-zero, BanSystem schedules a recurring timer that calls
     * PruneExpiredBans() every PruneIntervalHours so bans.json never grows
     * unbounded on busy servers.
     * Set to 0 to rely solely on the manual POST /bans/prune endpoint.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    float PruneIntervalHours = 0.0f;

    /**
     * When true, BanSystem pushes live JSON events (ban, unban, warn, join) to
     * a WebSocket endpoint configured in WebSocketPushUrl.
     * Requires SMLWebSocket mod to be installed.
     * Default: false.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    bool bPushEventsToWebSocket = false;

    /**
     * WebSocket endpoint to push live events to when bPushEventsToWebSocket=true.
     * Example: ws://localhost:9000/events
     * Leave empty to disable.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    FString WebSocketPushUrl;

    /**
     * Number of days after which warnings are considered "decayed" and excluded from
     * automatic escalation checks (default: 0 = warnings never decay).
     * When non-zero, warnings older than WarnDecayDays days are ignored by
     * GetWarningCount() (they are NOT deleted — they remain in the history for audit).
     * Set to 0 to use all warnings regardless of age (original behaviour).
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanSystem")
    int32 WarnDecayDays = 0;

    /** Returns the singleton config instance. */
    static const UBanSystemConfig* Get();
};
