// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BanChatCommandsConfig.generated.h"

/**
 * UBanChatCommandsConfig
 *
 * Per-server configuration for BanChatCommands.
 *
 * RECOMMENDED: manage the admin list in the persistent override file:
 *   Saved/BanChatCommands/BanChatCommands.ini
 * That file is never touched by mod updates or Alpakit dev deploys.
 * BanChatCommands writes the current admin list there on every server start
 * so your configuration survives any wipe of the mod directory.
 *
 * Example Saved/BanChatCommands/BanChatCommands.ini:
 *
 *   [/Script/BanChatCommands.BanChatCommandsConfig]
 *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
 *
 * When the list is empty, admin commands (/ban, /tempban, /unban,
 * /bancheck, /banlist) can only be run from the server console.
 * /whoami is always available to all players regardless of this setting.
 *
 * Note: On CSS Dedicated Server, all players are identified by their EOS
 * Product User ID regardless of their launch platform (Steam, Epic, etc.).
 * Use /whoami in-game to find the 32-character hex EOS PUID for any player.
 */
UCLASS(Config = BanChatCommands, meta = (DisplayName = "Ban Chat Commands"))
class BANCHATCOMMANDS_API UBanChatCommandsConfig : public UObject
{
    GENERATED_BODY()

public:
    /**
     * EOS Product User IDs (32-character hex strings) of server administrators
     * who may run ban commands in chat.  Use /whoami in-game to find your EOS PUID.
     * Add one entry per line in BanChatCommands.ini using the +AdminEosPUIDs= syntax.
     * If this list is empty, only the server console can run admin commands.
     *
     * Example:
     *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> AdminEosPUIDs;

    /**
     * EOS Product User IDs of moderators who may run a subset of moderation commands
     * (/kick and /modban only). Moderators cannot run /ban, /unban, /tempban, or other
     * full admin commands. Add one entry per line: +ModeratorEosPUIDs=<hex>
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> ModeratorEosPUIDs;

    /**
     * Number of bans shown per page in /banlist (default: 10).
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    int32 BanListPageSize = 10;

    /**
     * When true, /kick automatically creates a warning entry for the kicked player
     * so that kick reasons are preserved in the warning history.
     * Default: false.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    bool bCreateWarnOnKick = false;

    /**
     * Optional URL that /reloadconfig POSTs a notification to after reloading the
     * configuration.  Useful for notifying an external dashboard that the mod list
     * has changed without polling.  Leave empty to disable.
     * Example: https://dashboard.example.com/api/bans/config-changed
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    FString ReloadConfigWebhookUrl;

    /**
     * Duration in minutes used by the /modban command.
     * Moderators can issue a temporary ban of this length. Default: 30.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    int32 ModBanDurationMinutes = 30;

    /** Returns the singleton config instance. */
    static const UBanChatCommandsConfig* Get();

    /**
     * Returns true when the compound UID ("EOS:xxx") belongs to a configured
     * server administrator.  Comparison is case-insensitive for EOS PUIDs.
     */
    bool IsAdminUid(const FString& Uid) const;

    /**
     * Returns true when the compound UID ("EOS:xxx") belongs to either a configured
     * server administrator or moderator.  Admins automatically pass this check.
     * Comparison is case-insensitive for EOS PUIDs.
     */
    bool IsModeratorUid(const FString& Uid) const;
};
