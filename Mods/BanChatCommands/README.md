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

Admin access is controlled by player platform IDs in `DefaultGame.ini`:

```
<ServerRoot>/FactoryGame/Config/DefaultGame.ini
```

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminSteam64Ids=76561198000000000
+AdminSteam64Ids=76561198111111111
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

- Add one `+AdminSteam64Ids=` line per Steam admin.
- Add one `+AdminEosPUIDs=` line per EOS admin (32-character hex string, case-insensitive).
- When both lists are empty, admin commands can only be run from the **server console**.
- `/whoami` is always available to all connected players.

**How to find your UID:** Type `/whoami` in-game. The result will be in the format `STEAM:76561198000000000` or `EOS:00020aed...`. Add only the part **after** the colon to the appropriate list.

---

## Player identification

Commands that accept `<player|UID>` resolve the target as follows:

1. **Compound UID** (`STEAM:xxx` / `EOS:xxx`) — used directly.
2. **17-digit decimal string** — treated as a raw Steam 64-bit ID.
3. **32-character hex string** — treated as a raw EOS Product User ID.
4. **Any other string** — matched case-insensitively against the display names of currently connected players (substring match).

If a display name matches more than one connected player, the command lists all ambiguous matches and asks you to be more specific. Offline players must be targeted by UID — use `/playerhistory <name>` to look up a past UID.

---

## Examples

```
; Ban a currently-connected player by name
/ban SomePlayer Griefing

; Ban by Steam64 ID (player can be offline)
/ban STEAM:76561198000000000 Cheating
/ban 76561198000000000 Cheating

; 24-hour ban
/tempban SomePlayer 1440 Toxic behaviour

; Unban by Steam64 ID
/unban STEAM:76561198000000000

; Check ban status
/bancheck SomePlayer
/bancheck STEAM:76561198000000000

; List bans (first page)
/banlist

; Link a Steam ban to an EOS ban for the same person
/linkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51

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
