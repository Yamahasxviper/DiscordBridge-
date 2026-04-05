// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

/**
 * FBanChatCommandsModule
 *
 * Registers the ban chat commands on every server world init.
 *
 * Commands registered:
 *   /ban          — permanent ban by name or EOS PUID.
 *   /tempban      — timed ban (minutes) by name or EOS PUID.
 *   /unban        — remove a ban by EOS PUID.
 *   /bancheck     — query ban status for a player name or EOS PUID.
 *   /banlist      — display all active bans.
 *   /linkbans     — link two UIDs to the same ban record.
 *   /unlinkbans   — remove the link between two UIDs.
 *   /playerhistory — look up all known identities for a name or UID.
 *   /banname      — ban by display name (EOS + IP in one command).
 *   /whoami       — show the caller's own EOS PUID.
 *   /reloadconfig — hot-reload BanChatCommands.ini without restarting.
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
};
