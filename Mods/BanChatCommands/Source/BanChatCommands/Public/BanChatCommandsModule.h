// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FBanChatCommandsModule
 *
 * Registers the six unified ban chat commands on every server world init.
 *
 * Commands registered:
 *   /ban       — permanent ban by name or EOS PUID.
 *   /tempban   — timed ban (minutes) by name or EOS PUID.
 *   /unban     — remove a ban by EOS PUID.
 *   /bancheck  — query ban status for a player name or EOS PUID.
 *   /banlist   — display all active bans.
 *   /whoami    — show the caller's own EOS PUID.
 *
 * All commands delegate storage and enforcement to BanSystem subsystems.
 * Admin access to ban/tempban/unban/bancheck/banlist is controlled by the
 * AdminEosPUIDs list in UBanChatCommandsConfig (DefaultGame.ini).
 */
class BANCHATCOMMANDS_API FBanChatCommandsModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    FDelegateHandle WorldInitHandle;
};
