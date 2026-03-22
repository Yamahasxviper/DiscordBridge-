# DiscordBridge – Discord Commands Reference

← [Back to index](README.md)

This document is a single-page reference for **every command** available in the
DiscordBridge mod — Discord commands and in-game chat commands alike.  Each
section explains what the commands do, what they are used for, and any
important notes you should know before using them.

---

## Table of contents

1. [Ban system commands (`!ban`)](#1-ban-system-commands-ban)
2. [Kick command (`!kick`)](#2-kick-command-kick)
3. [Whitelist commands (`!whitelist`)](#3-whitelist-commands-whitelist)
4. [Server info commands (`!server`)](#4-server-info-commands-server)
5. [Server control commands (`!admin`)](#5-server-control-commands-admin)
6. [Ticket panel command (`!admin ticket-panel`)](#6-ticket-panel-command-admin-ticket-panel)
7. [In-game server info command (`!server` in-game)](#7-in-game-server-info-command-server-in-game)
8. [Quick-reference table](#8-quick-reference-table)

---

## 1 · Ban system commands (`!ban`)

**Purpose:** Manage a persistent list of banned players.  Banned players are
kicked immediately on login and cannot reconnect until unbanned.  Bans survive
server restarts.

**Where:** Bridged Discord channel **and** in-game chat (see note below).

**Who can use it (Discord):** Members who hold the Discord role set in
`BanCommandRoleId` inside `DefaultBan.ini`.  All commands are
**deny-by-default** — nobody can run them until the role is configured.

**Who can use it (in-game):** Any player in the game chat.  The in-game prefix
is controlled by `InGameBanCommandPrefix` (default `!ban`).

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultBan.ini`

---

### 1a · Player status / overview

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban players` | List every player currently on the server together with their platform ID (Steam64 or EOS PUID). | Players who are already banned are flagged with `:hammer: BANNED` so you can spot them at a glance. |
| `!ban status` | Show whether the ban system is enabled, whether the whitelist is enabled, and what online platform the server is using (Steam or EOS). | Use this first when troubleshooting to confirm the system is active. |
| `!ban check <name>` | Check whether a specific player (by in-game name) is on the ban list, and display their platform ID if they are currently online. | Name matching is case-insensitive. |

---

### 1b · Enable / disable the system

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban on` | Enable the ban system. | Any players who are already connected and are on the ban list are kicked immediately. |
| `!ban off` | Disable the ban system. | The ban list is preserved — bans are not deleted, they are just not enforced until you run `!ban on` again. |

---

### 1c · Ban / unban by in-game name

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban add <name>` | Add a player to the ban list by in-game name and kick them. | **Also auto-bans by platform ID** if the player is currently online — you do not need a separate `!ban id add` step.  If the player is offline the name-only ban is still saved. |
| `!ban remove <name>` | Remove a player from the ban list by in-game name. | Does not automatically remove any platform ID bans that may have been added by `!ban add`. |
| `!ban list` | Show every name currently on the ban list together with the enabled/disabled state. | — |

---

### 1d · Ban / unban by platform ID (recommended for persistent bans)

Platform ID bans are **more reliable** than name bans because a Steam or EOS
account ID never changes even if the player changes their display name.

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban id add <platform_id>` | Ban a player by their Steam64 ID or EOS PUID. | Auto-detects the format and labels it `(Steam)` or `(EOS PUID)` in confirmations. |
| `!ban id remove <platform_id>` | Unban a player by their Steam64 ID or EOS PUID. | — |
| `!ban id lookup <name>` | Find the platform ID of a currently-connected player by in-game name and return a ready-to-paste `!ban id add` command. | Only works if the player is online at the time. |
| `!ban id list` | List every platform ID on the ban list (both Steam and EOS), with type labels. | — |

---

### 1e · Steam64-specific commands

Use these when your server runs in **Steam mode** (`DefaultPlatformService=Steam`).
Steam64 IDs are 17-digit numbers starting with `765`.

> **CSS dedicated servers:** Satisfactory's CSS engine build uses EOS as the
> primary platform and **disables the Steam v1 OSS** on dedicated servers.  On a
> CSS server you should use EOS PUIDs (section 1f), not Steam64 IDs, even for
> Steam players.  Run `!ban status` to confirm your platform type.

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban steam add <steam64_id>` | Ban a player by their Steam64 ID. | The ID is validated — non-numeric or wrong-length input is rejected. |
| `!ban steam remove <steam64_id>` | Unban a player by their Steam64 ID. | — |
| `!ban steam list` | List every Steam64 ID on the ban list. | — |

---

### 1f · EOS PUID-specific commands

Use these when your server runs in **EOS mode** (`DefaultPlatformService=EOS`),
which is the default for CSS Satisfactory dedicated servers.

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban epic add <eos_puid>` | Ban a player by their EOS Product User ID (PUID). | PUIDs are 32-character hexadecimal strings. |
| `!ban epic remove <eos_puid>` | Unban a player by their EOS PUID. | — |
| `!ban epic list` | List every EOS PUID on the ban list. | — |

---

### 1g · Discord role management (Discord only)

These commands grant or revoke the Discord role that allows a member to run ban
commands.  They require the bot to have the **Manage Roles** permission in your
Discord server.

| Command | What it does | Notes |
|---------|-------------|-------|
| `!ban role add <discord_user_id>` | Grant the ban admin role to a Discord member. | Pass the member's Discord user ID (not their username). |
| `!ban role remove <discord_user_id>` | Revoke the ban admin role from a Discord member. | — |

> These two commands are **Discord-only** — they are not available in in-game chat.

---

### 1h · Cross-platform auto-linking

When a player connects and both a **Steam64 ID** and an **EOS PUID** are
observed for the same session, DiscordBridge automatically links the two
identities.  If either ID is banned, **both** are checked and the player is
kicked.  The partner ID is also added to the ban list automatically so future
bans cover both platforms.

---

## 2 · Kick command (`!kick`)

**Purpose:** Kick a player from the server immediately, **without** adding them
to the permanent ban list.  The player can reconnect right away.  Use this for
temporary disciplinary actions — disruptive behaviour that does not warrant a
full ban.

**Where:** Bridged Discord channel **and** in-game chat.

**Who can use it (Discord):** Members who hold the Discord role set in
`KickCommandRoleId` inside `DefaultBan.ini`.  All commands are
**deny-by-default** — nobody can run them until the role is configured.
You can set `KickCommandRoleId` to the same role as `BanCommandRoleId` to
give your existing ban admins kick access as well.

**Who can use it (in-game):** Any player in the game chat when
`InGameKickCommandPrefix` is configured.

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultBan.ini`
(the kick settings are in the `-- KICK COMMAND` section of that file).

---

| Command | What it does | Notes |
|---------|-------------|-------|
| `!kick <name>` | Kick the named player. Uses the `KickReason` from config as the disconnect message. | Name matching is case-insensitive. The player sees the reason in the Disconnected screen and can reconnect immediately. |
| `!kick <name> <reason>` | Kick the named player with a custom reason. | The reason text is both shown to the player in-game and included in the `%Reason%` placeholder in `KickDiscordMessage`. |

**Example Discord output:**
```
!kick PlayerName Behaviour was disruptive
→ :boot: PlayerName was kicked by an admin.
```

**Key difference from `!ban add`:**

| | `!kick` | `!ban add` |
|-|---------|-----------|
| Player can reconnect | ✅ immediately | ❌ blocked until unbanned |
| Permanent record | ❌ none | ✅ added to `ServerBanlist.json` |
| Platform ID also banned | ❌ no | ✅ auto-banned if online |

---

## 3 · Whitelist commands (`!whitelist`)

**Purpose:** Restrict the server to an approved list of players.  When the
whitelist is enabled, any player whose name is not on the list (and who does
not hold the configured Discord whitelist role) is kicked on login.

**Where:** Bridged Discord channel **and** in-game chat.

**Who can use it (Discord):** Members who hold the role set in
`WhitelistCommandRoleId` inside `DefaultWhitelist.ini`.
**Deny-by-default** — must be configured before use.

**Who can use it (in-game):** Any player in the game chat (prefix controlled
by `InGameWhitelistCommandPrefix`, default `!whitelist`).

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultWhitelist.ini`

> The whitelist and the ban system are **completely independent**.  You can use
> one, both, or neither at the same time.

---

### 2a · Enable / disable

| Command | What it does | Notes |
|---------|-------------|-------|
| `!whitelist on` | Enable the whitelist. | Players not on the list are kicked immediately. |
| `!whitelist off` | Disable the whitelist. | All players may join; the saved whitelist is preserved. |

---

### 2b · Manage whitelist entries

| Command | What it does | Notes |
|---------|-------------|-------|
| `!whitelist add <name>` | Add a player to the whitelist by in-game name. | — |
| `!whitelist remove <name>` | Remove a player from the whitelist by in-game name. | — |
| `!whitelist list` | Show every name on the whitelist and whether the system is enabled or disabled. | — |
| `!whitelist status` | Show whitelist state and ban system state side-by-side. | Useful for a quick sanity check. |

---

### 2c · Discord role management (Discord only)

| Command | What it does | Notes |
|---------|-------------|-------|
| `!whitelist role add <discord_user_id>` | Grant the whitelist admin role to a Discord member. | Requires the bot to have **Manage Roles** permission. |
| `!whitelist role remove <discord_user_id>` | Revoke the whitelist admin role from a Discord member. | — |

> These two commands are **Discord-only** — not available in in-game chat.

---

### 2d · Discord role auto-whitelist

Set `WhitelistRoleId` in `DefaultWhitelist.ini` to a Discord role ID.  Any
Discord member holding that role will be **automatically treated as whitelisted
by their Discord display name** when they try to join — no `!whitelist add`
step needed.  This is the easiest way to manage access for an active community
server.

---

## 4 · Server info commands (`!server`)

**Purpose:** Let any Discord member query live server information without
needing an admin role.  These are **read-only** — they cannot change anything
on the server.

**Where:** Bridged Discord channel only (a separate in-game variant is covered
in [section 6](#6-in-game-server-info-command-server-in-game)).

**Who can use it:** All members who can see and post in the bridged Discord
channel.  No role is required.

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini`  
**Setting:** `ServerInfoCommandPrefix` (default `!server`)

---

| Command | What it does | Notes |
|---------|-------------|-------|
| `!server players` | List the in-game names of all players currently online. | — |
| `!server online` | Alias for `!server players`. | Same output, alternative wording. |
| `!server status` | Show the server name, online/offline state, and current player count.  Includes a warning if the EOS platform is unavailable. | Good first command to check if something looks wrong. |
| `!server eos` | Run a full EOS platform diagnostic: credential presence, session-interface init status, number of platform IDs in the ban list, and step-by-step fix instructions when EOS is unavailable. | Use this when `!server status` shows an EOS warning, or when platform-ID ban enforcement appears to have stopped. |
| `!server help` | Display a summary of all available bot commands. | Useful to share with new moderators so they can discover the command groups. |

---

## 5 · Server control commands (`!admin`)

**Purpose:** Allow authorised Discord admins to stop or restart the dedicated
server remotely.

**Where:** Bridged Discord channel only.

**Who can use it:** Members who hold the role set in `ServerControlCommandRoleId`
inside `DefaultDiscordBridge.ini`.  **Deny-by-default** — all commands are
disabled for everyone until this setting is filled in.

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini`  
**Settings:** `ServerControlCommandPrefix` (default `!admin`), `ServerControlCommandRoleId`

> ⚠️ **Only grant the control role to fully trusted administrators.**
> `!admin stop` and `!admin restart` immediately affect the live server.

---

| Command | Exit code | What it does | Notes |
|---------|-----------|-------------|-------|
| `!admin start` | *(no exit)* | Replies that the server is already running. | Harmless no-op when the server is online. |
| `!admin stop` | `0` | Gracefully shuts down the server process. | The process supervisor must be configured to **not** restart on exit code 0.  See below. |
| `!admin restart` | `1` | Gracefully shuts down the server so the process supervisor restarts it. | The supervisor must be configured to restart on non-zero exit codes (`on-failure`). |

---

### Process supervisor configuration

DiscordBridge signals its intent through the server exit code:

| Intent | Exit code |
|--------|-----------|
| `!admin stop` (permanent stop) | `0` |
| `!admin restart` (restart requested) | `1` |

Configure your supervisor to restart **only** on a non-zero exit code:

**systemd** (`/etc/systemd/system/satisfactory.service`):
```ini
[Service]
Restart=on-failure
```

**Docker:**
```
--restart on-failure
```

**Shell wrapper:**
```bash
while true; do
    ./FactoryServer.sh
    [ $? -eq 0 ] && break   # exit code 0 = !admin stop; do not restart
done
```

---

## 6 · Ticket panel command (`!admin ticket-panel`)

**Purpose:** Post the button-based ticket panel to a designated Discord channel
so players can open support tickets with a single click — no typing required.

**Where:** Bridged Discord channel only.

**Who can use it:** Members who hold `ServerControlCommandRoleId` (same role as
`!admin stop`/`!admin restart`).

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultTickets.ini`

---

| Command | What it does | Notes |
|---------|-------------|-------|
| `!admin ticket-panel` | Post the ticket button panel to the channel configured by `TicketPanelChannelId`, or to the main bridged channel if that setting is empty. | Re-run this command any time you need to refresh the panel (e.g. after adding custom ticket reasons). |

---

### Built-in ticket buttons

| Button | Purpose |
|--------|---------|
| **Whitelist Request** | Player asks to be added to the server whitelist. |
| **Help / Support** | Player asks a question or reports a problem. |
| **Report a Player** | Player reports another player to the admins. |

Each built-in button can be hidden by setting the corresponding config key to
`False` (`TicketWhitelistEnabled`, `TicketHelpEnabled`, `TicketReportEnabled`).

### Custom ticket reasons

Add your own buttons with `TicketReason=Label|Description` lines in
`DefaultTickets.ini`:

```ini
TicketReason=Bug Report|Report a bug or technical issue
TicketReason=Suggestion|Submit a feature request or idea
TicketReason=Ban Appeal|Appeal a ban or moderation decision
```

Discord allows a maximum of 25 buttons per message.  The three built-in
buttons count toward this limit, so you can have up to **22 custom reasons**.

### How tickets work

1. A player clicks a button on the panel.
2. The bot creates a **private Discord channel** visible only to the player and
   the role set in `TicketNotifyRoleId`.
3. Admins are @mentioned in the notification channel (`TicketChannelId`) so they
   know a ticket has been opened.
4. When resolved, the admin closes the channel.  The bot deletes it automatically.

---

## 7 · In-game server info command (`!server` in-game)

**Purpose:** Let players query server information directly from the Satisfactory
in-game chat — no Discord client needed.

**Where:** Satisfactory in-game chat only.

**Who can use it:** Any player on the server.

**Config file:** `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini`  
**Setting:** `InGameServerInfoCommandPrefix` (default `!server`)

---

| Command | What it does | Notes |
|---------|-------------|-------|
| `!server players` | List the names of all players currently online (shown in game chat). | — |
| `!server status` | Show the server name, player count, and EOS platform state. | — |
| `!server eos` | Show the full EOS platform diagnostic in plain text (no Discord markdown). | Useful for on-the-spot troubleshooting without access to Discord. |
| `!server help` | List all available in-game server info sub-commands. | — |

---

## 8 · Quick-reference table

| Command | Where | Role required | What it does |
|---------|-------|--------------|-------------|
| `!ban on` | Discord + in-game | Ban admin | Enable the ban system |
| `!ban off` | Discord + in-game | Ban admin | Disable the ban system (preserves the list) |
| `!ban players` | Discord + in-game | Ban admin | List connected players with platform IDs |
| `!ban status` | Discord + in-game | Ban admin | Show ban + whitelist state and platform type |
| `!ban check <name>` | Discord + in-game | Ban admin | Check if a player is banned |
| `!ban add <name>` | Discord + in-game | Ban admin | Ban a player by name (auto-adds platform ID if online) |
| `!ban remove <name>` | Discord + in-game | Ban admin | Unban a player by name |
| `!ban list` | Discord + in-game | Ban admin | List all banned names |
| `!ban id add <id>` | Discord + in-game | Ban admin | Ban by Steam64 ID or EOS PUID |
| `!ban id remove <id>` | Discord + in-game | Ban admin | Unban by platform ID |
| `!ban id lookup <name>` | Discord + in-game | Ban admin | Find a connected player's platform ID |
| `!ban id list` | Discord + in-game | Ban admin | List all banned platform IDs |
| `!ban steam add <id>` | Discord + in-game | Ban admin | Ban by Steam64 ID |
| `!ban steam remove <id>` | Discord + in-game | Ban admin | Unban by Steam64 ID |
| `!ban steam list` | Discord + in-game | Ban admin | List all Steam64 bans |
| `!ban epic add <id>` | Discord + in-game | Ban admin | Ban by EOS PUID |
| `!ban epic remove <id>` | Discord + in-game | Ban admin | Unban by EOS PUID |
| `!ban epic list` | Discord + in-game | Ban admin | List all EOS PUID bans |
| `!ban role add <user_id>` | Discord only | Ban admin | Grant ban admin role to a Discord member |
| `!ban role remove <user_id>` | Discord only | Ban admin | Revoke ban admin role from a Discord member |
| `!kick <name>` | Discord + in-game | Kick admin | Kick a player without banning (can reconnect) |
| `!kick <name> <reason>` | Discord + in-game | Kick admin | Kick with a custom reason |
| `!whitelist on` | Discord + in-game | Whitelist admin | Enable the whitelist |
| `!whitelist off` | Discord + in-game | Whitelist admin | Disable the whitelist (preserves the list) |
| `!whitelist add <name>` | Discord + in-game | Whitelist admin | Add a player to the whitelist |
| `!whitelist remove <name>` | Discord + in-game | Whitelist admin | Remove a player from the whitelist |
| `!whitelist list` | Discord + in-game | Whitelist admin | List all whitelisted names |
| `!whitelist status` | Discord + in-game | Whitelist admin | Show whitelist + ban state side-by-side |
| `!whitelist role add <user_id>` | Discord only | Whitelist admin | Grant whitelist admin role to a Discord member |
| `!whitelist role remove <user_id>` | Discord only | Whitelist admin | Revoke whitelist admin role from a Discord member |
| `!server players` | Discord + in-game | None | List online players |
| `!server online` | Discord | None | Alias for `!server players` |
| `!server status` | Discord + in-game | None | Server name, player count, EOS state |
| `!server eos` | Discord + in-game | None | Full EOS platform diagnostic |
| `!server help` | Discord + in-game | None | List all available commands |
| `!admin start` | Discord only | Server control admin | Confirm the server is running |
| `!admin stop` | Discord only | Server control admin | Gracefully stop the server (exit 0) |
| `!admin restart` | Discord only | Server control admin | Gracefully restart the server (exit 1) |
| `!admin ticket-panel` | Discord only | Server control admin | Post the ticket button panel |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
