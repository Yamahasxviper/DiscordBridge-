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
 * AdminEosPUIDs list in UBanChatCommandsConfig (DefaultBanChatCommands.ini).
 */
class BANCHATCOMMANDS_API FBanChatCommandsModule : public IModuleInterface
{
public:
    virtual void StartupModule()  override;
    virtual void ShutdownModule() override;

private:
    FDelegateHandle WorldInitHandle;

    /** On every server start, writes the current admin list to
     *  Saved/BanChatCommands/BanChatCommands.ini — that folder is never touched
     *  by mod updates so the list survives any wipe of the mod directory. */
    static void BackupConfigIfNeeded();

    /** Restores documentation comments to Config/DefaultBanChatCommands.ini
     *  inside the deployed plugin directory.  UE's staging pipeline strips
     *  comments from Default*.ini via FConfigFile; this re-applies them on the
     *  first server start after each mod update. */
    static void RestoreDefaultConfigDocs();
};
