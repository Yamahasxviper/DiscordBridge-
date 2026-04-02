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
 *
 * When AdminSteam64Ids is empty, admin commands (/ban, /tempban, /unban,
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
     * If this list is empty, only the server console can run admin commands.
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> AdminSteam64Ids;

    /** Returns the singleton config instance. */
    static const UBanChatCommandsConfig* Get();

    /** Returns true when Steam64Id is in the configured admin list. */
    bool IsAdmin(const FString& Steam64Id) const;
};
