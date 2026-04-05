# BanChatCommands — In-Game Ban Commands for Satisfactory

A server-only Satisfactory mod that adds a full set of ban management commands to the in-game chat. Commands work from the Satisfactory in-game chat (for admin players) and from the server console.

Requires the **BanSystem** mod.

---

## Commands

| Command | Admin | Description |
|---------|-------|-------------|
| `/ban <player\|UID> [reason...]` | ✅ | Permanently ban a player |
| `/tempban <player\|UID> <minutes> [reason...]` | ✅ | Temporarily ban for N minutes |
| `/unban <UID>` | ✅ | Remove an existing ban |
| `/bancheck <player\|UID>` | ✅ | Check if a player is currently banned |
| `/banlist [page]` | ✅ | List active bans (10 per page) |
| `/linkbans <UID1> <UID2>` | ✅ | Link two UIDs for cross-platform ban enforcement |
| `/unlinkbans <UID1> <UID2>` | ✅ | Remove a UID link |
| `/playerhistory <name\|UID>` | ✅ | Search the session audit log |
| `/whoami` | ❌ | Show your own compound UID (open to all players) |

---

## Admin setup

Admin access is controlled by player EOS Product User IDs in the mod's own config file.  
To ensure your settings survive mod updates, add them to the server override file:

```
<ServerRoot>/FactoryGame/Saved/Config/<Platform>/BanChatCommands.ini
```

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

- Add one `+AdminEosPUIDs=` line per admin (32-character hex string, case-insensitive).
- When the list is empty, admin commands can only be run from the **server console**.
- `/whoami` is always available to all connected players.

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
; Ban a currently-connected player by name
/ban SomePlayer Griefing

; Ban by EOS PUID (player can be offline)
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban 00020aed06f0a6958c3c067fb4b73d51 Cheating

; 24-hour ban
/tempban SomePlayer 1440 Toxic behaviour

; Unban by EOS PUID
/unban EOS:00020aed06f0a6958c3c067fb4b73d51

; Check ban status
/bancheck SomePlayer
/bancheck 00020aed06f0a6958c3c067fb4b73d51

; List bans (first page)
/banlist

; Link two bans for the same person (e.g. old and new EOS PUID)
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344

; Look up a player's past UIDs (useful after they disconnect)
/playerhistory SomePlayer

; Show your own UID (no admin required)
/whoami
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

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
