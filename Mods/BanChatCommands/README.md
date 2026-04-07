# BanChatCommands — In-Game Ban Commands for Satisfactory

**Version 1.1.0** | Server-only | Requires SML `^3.11.3` + BanSystem `^1.0.0` | Game build `>=416835`

A server-only Satisfactory mod that adds a full set of ban and moderation commands to the in-game chat. Commands work from the Satisfactory in-game chat (for admin/moderator players) and from the server console.

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
| `/kick <player\|UID> [reason...]` | Moderator | Disconnect a player without banning them |
| `/modban <player\|UID> [reason...]` | Moderator | 30-minute temporary ban (moderator shortcut) |
| `/warn <player\|UID> <reason...>` | Admin | Issue a formal warning to a player |
| `/warnings <player\|UID>` | Admin | List all recorded warnings for a player |
| `/clearwarns <player\|UID>` | Admin | Remove all warnings for a player |
| `/announce <message...>` | Admin | Broadcast a server-wide message (also posts to Discord) |
| `/stafflist` | Admin | Show currently-online admins and moderators |
| `/reason <UID>` | Admin | Show the ban reason for a UID |
| `/history` | All | Show your own session and warning history |
| `/mute <player\|UID> [minutes] [reason...]` | Moderator | Silence a player's chat (in-memory, clears on restart) |
| `/unmute <player\|UID>` | Moderator | Remove a chat mute |
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

; Mute a player for 30 minutes
/mute SomePlayer 30 Spamming

; Unmute a player
/unmute SomePlayer

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
| BanSystem | `^1.0.0` |
| Satisfactory (dedicated server) | `>=416835` |

---

## Documentation

Full documentation for the underlying BanSystem mod:

→ [BanSystem README](../BanSystem/README.md)
→ [BanSystem Documentation](../BanSystem/Docs/README.md)
→ [BanChatCommands Documentation](Docs/README.md)

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
