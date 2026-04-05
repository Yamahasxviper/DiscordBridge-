# BanChatCommands – Features Overview

← [Back to index](README.md)

BanChatCommands is a server-only Satisfactory mod that adds a full set of ban management commands to the in-game chat and server console. It requires the **BanSystem** mod for storage and enforcement.

---

## Nine built-in commands

| Command | Admin required | Purpose |
|---------|:--------------:|---------|
| `/ban` | ✅ | Permanently ban a player |
| `/tempban` | ✅ | Temporarily ban for N minutes |
| `/unban` | ✅ | Remove an existing ban |
| `/bancheck` | ✅ | Check whether a player is currently banned |
| `/banlist` | ✅ | List all active bans (paginated, 10 per page) |
| `/linkbans` | ✅ | Link two UIDs so one ban covers both identities |
| `/unlinkbans` | ✅ | Remove a previously created UID link |
| `/playerhistory` | ✅ | Search the session audit log by name or UID |
| `/whoami` | ❌ | Show your own compound UID — open to all players |

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

## No permanent data storage

BanChatCommands itself stores no data. All ban records are managed by the **BanSystem** mod. BanChatCommands is purely the command interface layer.
