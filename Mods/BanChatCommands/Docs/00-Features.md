# BanChatCommands – Features Overview

← [Back to index](README.md)

BanChatCommands is a server-only Satisfactory mod that adds a full set of ban management commands to the in-game chat and server console. It requires the **BanSystem** mod for storage and enforcement.

---

## Forty-three built-in commands

| Command | Role | Purpose |
|---------|:----:|---------|
| `/ban` | Admin | Permanently ban a player or IP address |
| `/tempban` | Admin | Temporarily ban for N minutes |
| `/unban` | Admin | Remove an existing ban by UID or IP address |
| `/unbanname` | Admin | Remove ban for an offline player by display-name substring |
| `/bancheck` | Admin | Check whether a player is currently banned |
| `/banlist` | Admin | List all active bans (paginated, 10 per page) |
| `/linkbans` | Admin | Link two UIDs so one ban covers both identities |
| `/unlinkbans` | Admin | Remove a previously created UID link |
| `/playerhistory` | Admin | Search the session audit log by name or UID |
| `/banname` | Admin | Ban offline player by name + IP from session history |
| `/reloadconfig` | Admin | Hot-reload admin/moderator config without restarting |
| `/warn` | Admin | Issue a formal warning to a player |
| `/warnings` | Admin | List all recorded warnings for a player |
| `/clearwarns` | Admin | Remove all warnings for a player |
| `/clearwarn` | Admin | Remove a specific warning by ID |
| `/reason` | Admin | Show the ban reason for a UID |
| `/banreason` | Admin | Edit the ban reason for a UID |
| `/announce` | Admin | Broadcast a server-wide message (also posts to Discord) |
| `/stafflist` | Admin | Show currently-online admins and moderators |
| `/note` | Admin | Add an admin note to a player |
| `/notes` | Admin | List all admin notes for a player |
| `/duration` | Admin | Show remaining tempban duration |
| `/extend` | Admin | Extend a temporary ban duration |
| `/appeal` | Admin | Manage ban appeals |
| `/staffchat` | Admin | Staff-only message |
| `/scheduleban` | Admin | Schedule a future ban |
| `/qban` | Admin | Apply a quick-ban template |
| `/reputation` | Admin | Show player reputation score |
| `/bulkban` | Admin | Ban multiple players at once |
| `/kick` | Moderator | Disconnect a player without banning them |
| `/modban` | Moderator | 30-minute temporary ban (moderator shortcut) |
| `/mute` | Moderator | Silence a player's chat |
| `/unmute` | Moderator | Remove a chat mute |
| `/tempmute` | Moderator | Timed mute |
| `/tempunmute` | Moderator | Remove a timed mute |
| `/mutecheck` | Moderator | Check mute status |
| `/mutelist` | Moderator | List all active mutes |
| `/mutereason` | Moderator | Edit mute reason |
| `/freeze` | Moderator | Immobilise a player (toggle on/off) |
| `/clearchat` | Moderator | Flush chat history (posts Discord embed) |
| `/report` | Moderator | Submit a player report |
| `/history` | All | Show your own session and warning history |
| `/whoami` | All | Show your own compound UID — open to all players |

---

## Flexible player targeting

Commands that accept `<player|UID>` resolve the target in order:

1. **Compound UID** (`EOS:xxx`) — used directly.
2. **32-character hex string** — treated as a raw EOS Product User ID.
3. **Anything else** — matched case-insensitively against the display names of currently connected players (substring match).

---

## Ban linking across identities

Using `/linkbans`, a ban issued under one EOS PUID also blocks the player if they reconnect under a different PUID. See the [Commands Reference](03-Commands.md) for usage.

---

## Works from the server console

All commands — including admin-only ones — can always be run from the **server console**, regardless of the admin list. This means you can use the commands even before adding any admins to the config.

---

## Session audit trail

The `/playerhistory` command queries the **player session registry** provided by BanSystem. Every time a player connects, their compound UID and display name are recorded. This lets you look up a player's past UIDs even after they have disconnected.

---

## Offline player unbanning with `/unbanname`

`/unbanname <name>` searches the session registry by display-name substring and removes the ban — even if the player is not currently online. If an IP address was recorded at their last login, that IP ban is also removed. This is the counterpart to `/banname`.

---

## Offline player banning with `/banname`

`/banname <name> [reason]` searches the session registry by display-name substring and permanently bans the player — even if they are not currently online. If an IP address was recorded at their last login, it is banned too and linked to the EOS PUID ban for combined enforcement.

---

## Live config reload with `/reloadconfig`

`/reloadconfig` forces BanChatCommands to re-read the admin list from disk immediately. No server restart is needed after adding or removing admins — just edit the config file and run `/reloadconfig`.

---

## Moderator role

In addition to full admins (`AdminEosPUIDs`), you can define a **moderator** tier
(`ModeratorEosPUIDs`). Moderators can run `/kick`, `/modban`, `/mute`, `/unmute`,
`/tempmute`, `/tempunmute`, `/mutecheck`, `/mutelist`, `/mutereason`, `/freeze`,
`/clearchat`, and `/report`, but cannot run full admin commands like `/ban`, `/unban`,
or `/warn`.

---

## Warning system

`/warn <player> <reason>` issues a formal warning that is stored persistently in
BanSystem's `PlayerWarningRegistry`. Warnings trigger automatic escalation bans when
the player accumulates enough warnings — configurable via `AutoBanWarnCount` /
`AutoBanWarnMinutes` or the `WarnEscalationTiers` array in BanSystem's config.

| Command | Purpose |
|---------|---------|
| `/warn <player> <reason>` | Issue a warning |
| `/warnings <player>` | List all warnings |
| `/clearwarns <player>` | Remove all warnings |

---

## Server-wide announcements with `/announce`

`/announce <message>` broadcasts a message to all online players in the game chat
and simultaneously posts it to the bridged Discord channel (via DiscordBridge, if
configured). Requires admin.

---

## In-memory muting with `/mute` / `/unmute`

`/mute <player> [minutes] [reason]` blocks a player's chat messages from being
relayed or displayed to other players. Mutes are held in `UMuteRegistry` (a
`GameInstance` subsystem) and do **not** persist across server restarts.

`/unmute <player>` removes a mute immediately.

---

## Self-service `/history`

Any player (no admin required) can type `/history` to see their own past session
records and any warnings they have received. Useful for players who want to understand
why they were warned.

---

## `/stafflist`

Lists all currently-online admins and moderators so players know who to contact.
Requires admin.

---

## `/reason`

`/reason <UID>` shows the ban reason stored for a given compound UID. Useful when
reviewing a ban list entry to recall why a player was banned. Requires admin.

---

## Player freeze with `/freeze`

`/freeze <player>` toggles movement lock on a player. When frozen, the player
cannot move but can still chat. Moderators can use this to temporarily hold a
player while investigating an issue.

---

## Chat management with `/clearchat`

`/clearchat` flushes the in-game chat history for all players and posts a
notification embed to the bridged Discord channel. Requires moderator.

---

## Player reports with `/report`

`/report <player> <reason>` submits a player report to the configured
`ReportWebhookUrl` Discord webhook. Requires moderator.

---

## Scheduled bans with `/scheduleban`

`/scheduleban <player> <timestamp> [reason]` schedules a ban to take effect
at a future UTC timestamp. Uses BanSystem's `ScheduledBanRegistry`.

---

## Quick-ban templates with `/qban`

`/qban <template> <player>` applies a pre-configured ban template.
Templates are defined in BanSystem's config via `BanTemplates=`.

---

## Admin notes with `/note` / `/notes`

`/note <player> <text>` adds a persistent admin note to a player's record.
`/notes <player>` lists all notes. Useful for tracking previous interactions
and warnings without issuing formal warnings.

---

## Ban duration management

| Command | Purpose |
|---------|---------|
| `/duration <UID>` | Show remaining tempban time |
| `/extend <UID> <minutes>` | Add time to a tempban |
| `/banreason <UID> <reason>` | Edit ban reason |

---

## Mute management

| Command | Purpose |
|---------|---------|
| `/mute <player> [minutes] [reason]` | Silence chat |
| `/unmute <player>` | Remove mute |
| `/tempmute <player> <minutes> [reason]` | Timed mute |
| `/tempunmute <player>` | Remove timed mute |
| `/mutecheck <player>` | Check mute status |
| `/mutelist` | List all mutes |
| `/mutereason <player> <reason>` | Edit mute reason |

---

## Player reputation with `/reputation`

`/reputation <player>` shows a composite reputation score based on warnings,
bans, and session behaviour.

---

## Bulk banning with `/bulkban`

`/bulkban <UID1> <UID2> ... [reason]` bans multiple players in a single
command. Useful for coordinated enforcement actions.

---

## No permanent data storage

BanChatCommands itself stores no persistent data. All ban and warning records are
managed by the **BanSystem** mod. BanChatCommands is purely the command interface
layer.
