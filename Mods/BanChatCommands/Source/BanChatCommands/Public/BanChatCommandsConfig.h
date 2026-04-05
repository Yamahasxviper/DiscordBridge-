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
 * Settings are read from the mod's Config/DefaultBanChatCommands.ini.
 * To add admins, create (or edit) the override file on your server:
 *   Saved/Config/<Platform>/BanChatCommands.ini
 * and add a section:
 *
 *   [/Script/BanChatCommands.BanChatCommandsConfig]
 *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
 *
 * When the list is empty, admin commands (/ban, /tempban, /unban,
 * /bancheck, /banlist) can only be run from the server console.
 * Using Saved/Config ensures your admin list is never overwritten by a mod update.
 * /whoami is always available to all players regardless of this setting.
 *
 * Note: On CSS Dedicated Server, all players are identified by their EOS
 * Product User ID regardless of their launch platform (Steam, Epic, etc.).
 * Use /whoami in-game to find the 32-character hex EOS PUID for any player.
 */
UCLASS(Config = BanChatCommands, DefaultConfig, meta = (DisplayName = "Ban Chat Commands"))
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

    /** Returns the singleton config instance. */
    static const UBanChatCommandsConfig* Get();

    /**
     * Returns true when the compound UID ("EOS:xxx") belongs to a configured
     * server administrator.  Comparison is case-insensitive for EOS PUIDs.
     */
    bool IsAdminUid(const FString& Uid) const;
};
