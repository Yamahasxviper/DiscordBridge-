// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"
#include "Containers/Ticker.h"

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
 * Config changes are also picked up automatically every
 * ConfigPollIntervalSeconds seconds without any manual intervention.
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

    /** Writes the current admin list to Saved/BanChatCommands/BanChatCommands.ini.
     *  Called on startup, after /reloadconfig, and after auto-reload detects changes.
     *  That folder is never touched by mod updates so the list survives any wipe
     *  of the mod directory. */
    static void BackupConfigIfNeeded();

    /** Restores Mods/BanChatCommands/Config/DefaultBanChatCommands.ini with the full
     *  annotated template if the file is missing or has been stripped of comment lines
     *  by Alpakit's staging step. */
    static void RestoreDefaultConfigIfNeeded();

private:
    FDelegateHandle WorldInitHandle;
    /** Handle for the game-mode logout event — removes disconnected players from FrozenPlayerUids. */
    FDelegateHandle LogoutHandle;

    // ── Auto-reload ───────────────────────────────────────────────────────────

    /** How often (seconds) to poll for config-file changes. */
    static constexpr float ConfigPollIntervalSeconds = 60.0f;

    /** Handle to the recurring FTSTicker that drives config polling. */
    FTSTicker::FDelegateHandle ConfigPollHandle;

    /** Handle to the recurring ticker that expires timed mutes every 30 s. */
    FTSTicker::FDelegateHandle MuteExpiryHandle;

    /** Per-world mute-event delegate handles — stored so they can be removed before
     *  re-binding on subsequent world loads, preventing duplicate firings. */
    FDelegateHandle MutedDelegateHandle;
    FDelegateHandle UnmutedDelegateHandle;

    /** Ticker callback — calls UMuteRegistry::TickExpiry() on all instances.
     *  Returns true to keep ticking. */
    bool OnMuteExpiryTick(float DeltaTime);

    /** Hash of AdminEosPUIDs at the last (re)load — used to detect changes. */
    uint32 LastConfigHash = 0;

    /** CRC32 over the sorted AdminEosPUIDs list. */
    static uint32 ComputeAdminHash();

    /** Ticker callback — reloads config from disk and logs when the admin list
     *  changes.  Returns true to keep ticking. */
    bool OnConfigPollTick(float DeltaTime);
};
