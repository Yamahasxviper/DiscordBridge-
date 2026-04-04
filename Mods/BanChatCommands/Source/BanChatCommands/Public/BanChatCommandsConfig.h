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
 * Edit <ServerRoot>/FactoryGame/Config/DefaultGame.ini and add a section:
 *
 *   [/Script/BanChatCommands.BanChatCommandsConfig]
 *   +AdminSteam64Ids=76561198000000000
 *   +AdminSteam64Ids=76561198111111111
 *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
 *
 * When both lists are empty, admin commands (/ban, /tempban, /unban,
 * /bancheck, /banlist) can only be run from the server console.
 * /whoami is always available to all players regardless of this setting.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Ban Chat Commands"))
class BANCHATCOMMANDS_API UBanChatCommandsConfig : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Steam64 IDs of server administrators who may run ban commands in chat.
     * Add one entry per line in DefaultGame.ini using the +AdminSteam64Ids= syntax.
     * If this list and AdminEosPUIDs are both empty, only the server console can
     * run admin commands.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> AdminSteam64Ids;

    /**
     * EOS Product User IDs (32-character hex strings) of server administrators
     * who may run ban commands in chat.  Use /whoami in-game to find your EOS PUID.
     * Add one entry per line in DefaultGame.ini using the +AdminEosPUIDs= syntax.
     *
     * Example:
     *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> AdminEosPUIDs;

    /** Returns the singleton config instance. */
    static const UBanChatCommandsConfig* Get();

    /**
     * Returns true when the compound UID ("STEAM:xxx" or "EOS:xxx") belongs to a
     * configured server administrator.  Checks AdminSteam64Ids for Steam players
     * and AdminEosPUIDs for EOS players (case-insensitive for EOS PUIDs).
     */
    bool IsAdminUid(const FString& Uid) const;

    /**
     * Legacy helper — checks whether a raw Steam64 ID string is in AdminSteam64Ids.
     * Prefer IsAdminUid() for new code; this remains for backwards compatibility.
     */
    bool IsAdmin(const FString& Steam64Id) const;
};
