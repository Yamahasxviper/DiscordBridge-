# BanChatCommands — In-Game Ban Commands for Satisfactory

**Version 1.1.0** | Server-only | Requires SML `^3.11.3` + BanSystem `^1.1.0` | Game build `>=416835`

A server-only Satisfactory mod that adds 42 in-game chat commands for ban management, moderation, warnings, mutes, notes, freeze, reporting, scheduled bans, and more. Commands work from the Satisfactory in-game chat (for admin/moderator players) and from the server console.

Requires the **BanSystem** mod.

---

## Commands

| Command | Role | Description |
|---------|------|-------------|
| `/ban <player\|UID\|IP:address> [reason...]` | Admin | Permanently ban a player or IP address |
| `/tempban <player\|UID\|IP:address> <minutes> [reason...]` | Admin | Temporarily ban for N minutes |
| `/unban <UID\|IP:address>` | Admin | Remove an existing ban |
| `/unbanname <name_substring>` | Admin | Remove ban for an offline player by display-name |
| `/bancheck <player\|UID\|IP:address>` | Admin | Check if a player or IP is currently banned |
| `/banlist [page]` | Admin | List active bans (10 per page) |
| `/linkbans <UID1> <UID2>` | Admin | Link two UIDs for cross-identity ban enforcement |
| `/unlinkbans <UID1> <UID2>` | Admin | Remove a UID link |
| `/playerhistory <name\|UID>` | Admin | Search the session audit log |
| `/banname <name> [reason...]` | Admin | Ban offline player by name + IP from session history |
| `/reloadconfig` | Admin | Hot-reload admin/moderator config without restarting |
| `/warn <player\|UID> <reason...>` | Admin | Issue a formal warning to a player |
| `/warnings <player\|UID>` | Admin | List all recorded warnings for a player |
| `/clearwarns <player\|UID>` | Admin | Remove all warnings for a player |
| `/clearwarn <player\|UID> <id>` | Admin | Remove a specific warning by ID |
| `/announce <message...>` | Admin | Broadcast a server-wide message (also posts to Discord) |
| `/stafflist` | Moderator | Show currently-online admins and moderators |
| `/reason <UID>` | All | Show the ban reason for a UID |
| `/banreason <UID> <new reason>` | Admin | Edit the ban reason for a UID |
| `/note <player\|UID> <text>` | Admin | Add an admin note to a player |
| `/notes <player\|UID>` | Admin | List all admin notes for a player |
| `/duration <UID>` | Admin | Show remaining tempban duration |
| `/extend <UID> <minutes>` | Admin | Extend a temporary ban duration |
| `/appeal <reason...>` | All | Submit your own ban appeal |
| `/staffchat <message...>` | Moderator | Staff-only message |
| `/scheduleban <player\|UID> <timestamp> [reason]` | Admin | Schedule a future ban |
| `/qban <template> <player\|UID>` | Admin | Apply a quick-ban template |
| `/reputation <player\|UID>` | Admin | Show player reputation score |
| `/bulkban <UID1> <UID2> ... [reason]` | Admin | Ban multiple players at once |
| `/kick <player\|UID> [reason...]` | Moderator | Disconnect a player without banning them |
| `/modban <player\|UID> [reason...]` | Moderator | 30-minute temporary ban (moderator shortcut) |
| `/mute <player\|UID> [duration] [reason...]` | Admin | Silence a player's chat (duration: minutes or 30m/1h/1d) |
| `/unmute <player\|UID>` | Admin | Remove a chat mute |
| `/tempunmute <player\|UID>` | Moderator | Remove a timed mute |
| `/mutecheck <player\|UID>` | Moderator | Check mute status |
| `/mutelist` | Moderator | List all active mutes |
| `/mutereason <player\|UID> <reason>` | Admin | Edit mute reason |
| `/freeze <player\|UID>` | Admin | Immobilise a player (toggle) |
| `/clearchat` | Admin | Flush chat history (posts Discord embed) |
| `/report <player> [reason...]` | All | Submit a player report to staff |
| `/history` | All | Show your own session and warning history |
| `/whoami` | All | Show your own compound UID |

---

## Role levels

| Role | Who has it |
|------|-----------|
| **Admin** | EOS PUIDs listed in `AdminEosPUIDs` |
| **Moderator** | EOS PUIDs listed in `ModeratorEosPUIDs` (admins also qualify) |
| **All** | Any connected player (no config required) |

Commands issued from the **server console** always bypass role checks.

---

## Admin setup

Admin and moderator access is controlled by player EOS Product User IDs in the mod's own config file.
To ensure your settings survive mod updates, add them to the server override file:

```
<ServerRoot>/FactoryGame/Saved/Config/<Platform>/BanChatCommands.ini
```

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
+ModeratorEosPUIDs=aabbccdd11223344aabbccdd11223344
```

- Add one `+AdminEosPUIDs=` line per admin (32-character hex string, case-insensitive).
- Add one `+ModeratorEosPUIDs=` line per moderator.
- When both lists are empty, admin commands can only be run from the **server console**.
- `/whoami` and `/history` are always available to all connected players.

> **Note:** On CSS Dedicated Server, all players are identified by their EOS Product User ID regardless of launch platform (Steam, Epic, etc.).

**How to find your EOS PUID:** Type `/whoami` in-game. The result will be your 32-character hex EOS PUID.

---

## Player identification

Commands that accept `<player|UID>` resolve the target as follows:

1. **Compound UID** (`EOS:xxx`) — used directly.
2. **32-character hex string** — treated as a raw EOS Product User ID.
3. **Any other string** — matched case-insensitively against the display names of currently connected players (substring match).

If a display name matches more than one connected player, the command lists all ambiguous matches and asks you to be more specific. Offline players must be targeted by UID — use `/playerhistory <name>` to look up a past UID.

---

## Examples

```
; Permanently ban a connected player by name
/ban SomePlayer Griefing

; Ban by EOS PUID (player can be offline)
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban 00020aed06f0a6958c3c067fb4b73d51 Cheating

; Ban by IP address (IPv4 or IPv6)
/ban IP:1.2.3.4 VPN evader

; 24-hour ban
/tempban SomePlayer 1440 Toxic behaviour

; Kick without banning (moderator)
/kick SomePlayer Please stop that

; 30-minute moderator ban
/modban SomePlayer Spamming

; Unban by EOS PUID
/unban EOS:00020aed06f0a6958c3c067fb4b73d51

; Issue a warning
/warn SomePlayer Please follow the server rules

; List warnings
/warnings SomePlayer

; Mute a player indefinitely
/mute SomePlayer Spamming

; Mute a player for 30 minutes with shorthand duration
/mute SomePlayer 30m Spamming

; Mute a player for 2 hours
/mute SomePlayer 2h Toxic behaviour

; Unmute a player
/unmute SomePlayer

; Staff-only message
/staffchat Important mod meeting at 8pm

; Freeze a player in place
/freeze SomePlayer

; Clear chat
/clearchat

; Report a player
/report SomePlayer Suspected hacking

; Schedule a ban for later
/scheduleban SomePlayer 2025-06-01T12:00:00Z Repeated offender

; Quick-ban using a template
/qban griefing SomePlayer

; Check player reputation
/reputation SomePlayer

; Bulk ban
/bulkban EOS:aaa... EOS:bbb... Coordinated griefing

; Submit your own ban appeal (available to all players)
/appeal I understand the rules now and would like another chance

; Show ban duration remaining
/duration EOS:00020aed06f0a6958c3c067fb4b73d51

; Extend a ban
/extend EOS:00020aed06f0a6958c3c067fb4b73d51 1440

; Add a note
/note SomePlayer Previous verbal warning about language

; List notes
/notes SomePlayer

; Clear a specific warning
/clearwarn SomePlayer 3

; Edit ban reason
/banreason EOS:00020aed06f0a6958c3c067fb4b73d51 Updated: Griefing and harassment

; Server broadcast (also appears in Discord)
/announce Server restarting in 5 minutes!

; Show ban reason for a UID
/reason EOS:00020aed06f0a6958c3c067fb4b73d51

; Show your own session history and warnings
/history

; Show online staff
/stafflist

; Reload admin/moderator list without restarting
/reloadconfig
```

---

## Dependencies

| Dependency | Version |
|------------|---------|
| SML | `^3.11.3` |
| BanSystem | `^1.1.0` |
| Satisfactory (dedicated server) | `>=416835` |

---

## Documentation

Full documentation for the underlying BanSystem mod:

→ [BanSystem README](../BanSystem/README.md)
→ [BanSystem Documentation](../BanSystem/Docs/README.md)
→ [BanChatCommands Documentation](Docs/README.md)

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
