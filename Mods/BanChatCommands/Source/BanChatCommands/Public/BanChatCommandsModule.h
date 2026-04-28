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
 *   /tempban      — timed ban (supports 30m/2h/1d/1w suffixes) by name or EOS PUID.
 *   /unban        — remove a ban by EOS PUID.
 *   /unbanname    — remove a ban by display-name substring.
 *   /bancheck     — query ban status for a player name or EOS PUID.
 *   /banlist      — display all active bans.
 *   /banname      — ban by display name (EOS + IP in one command).
 *   /banreason    — update the reason for an existing ban.
 *   /duration     — show remaining duration for a player's ban.
 *   /extend       — extend an existing temporary ban's duration.
 *   /linkbans     — link two UIDs to the same ban record.
 *   /unlinkbans   — remove the link between two UIDs.
 *   /modban       — issue a ban directly (moderator-only shorthand).
 *   /scheduleban  — schedule a ban to take effect after a delay.
 *   /bulkban      — ban multiple PUIDs in a single command.
 *   /qban         — quick-ban using a pre-configured template.
 *   /kick         — kick a player (no ban).
 *   /mute         — mute a player (indefinite or timed).
 *   /unmute       — remove an active mute.
 *   /tempunmute   — remove a timed mute early.
 *   /mutecheck    — show mute status for a player.
 *   /mutereason   — update the reason for an existing mute.
 *   /mutelist     — list all currently muted players.
 *   /warn         — issue a warning to a player.
 *   /warnings     — show warnings for a player.
 *   /clearwarns   — clear all warnings for a player.
 *   /clearwarn    — remove a single warning by ID.
 *   /announce     — broadcast a server-wide announcement.
 *   /clearchat    — clear the in-game chat history.
 *   /freeze       — prevent a player from moving.
 *   /staffchat    — send a message visible only to staff.
 *   /stafflist    — list all configured staff members.
 *   /reason       — look up the moderation history for a player.
 *   /history      — display a player's moderation history (alias).
 *   /reputation   — show a player's reputation/points summary.
 *   /note         — add a private admin note for a player.
 *   /notes        — list all admin notes for a player.
 *   /report       — report a player (visible to online staff).
 *   /appeal       — submit a ban appeal.
 *   /playerhistory — look up all known identities for a name or UID.
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
    virtual void StartupModule() override;
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
    /** Handle for the game-mode post-login event — re-applies freeze for reconnecting players. */
    FDelegateHandle PostLoginHandle;
    /** Handle for the AGameModeBase::Logout hook — clears FrozenPlayerUids on disconnect. */
    FDelegateHandle LogoutHookHandle;

    // ── Auto-reload ──────────────────────────────────────────────────────────

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
