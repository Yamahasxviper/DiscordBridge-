# ServerWhitelist

Standalone server-only whitelist mod for Satisfactory dedicated servers.

Restricts who can join the server to an approved player list.  
Managed entirely via **in-game chat commands** — no Discord bot required.

---

## Features

- Kick non-whitelisted players on join (with customisable disconnect message)
- In-game chat commands for admins: `!whitelist on/off/add/remove/list/status`
- Persistent whitelist stored in `Saved/ServerWhitelist.json`
- Pre-populate the whitelist via `+WhitelistedPlayers=` lines in the config
- Compatible with **DiscordBridge**: when both mods are installed, ServerWhitelist defers to DiscordBridge (no double-kick, shared `ServerWhitelist.json`)

---

## Quick Setup

1. Install the mod on your dedicated server.
2. Edit `Mods/ServerWhitelist/Config/DefaultServerWhitelist.ini`:
   ```ini
   [ServerWhitelist]
   WhitelistEnabled=True

   ; Admin(s) who can run !whitelist commands in-game:
   +AdminPlayerNames=YourIngameName

   ; Optional: pre-populate on first start:
   +WhitelistedPlayers=FriendOne
   +WhitelistedPlayers=FriendTwo
   ```
3. Restart the server.

---

## In-Game Commands

Type these in the Satisfactory in-game chat.  
Only players listed in `AdminPlayerNames` can run them.

| Command | Description |
|---------|-------------|
| `!whitelist on` | Enable the whitelist |
| `!whitelist off` | Disable the whitelist |
| `!whitelist add <Name>` | Add a player to the whitelist |
| `!whitelist remove <Name>` | Remove a player from the whitelist |
| `!whitelist list` | Show all whitelisted players |
| `!whitelist status` | Show whether the whitelist is on or off |

---

## Config Reference

All settings live in `Mods/ServerWhitelist/Config/DefaultServerWhitelist.ini`.  
This file is **excluded from the mod package** so updates will never overwrite it.

| Key | Default | Description |
|-----|---------|-------------|
| `WhitelistEnabled` | `False` | Master on/off switch |
| `WhitelistKickReason` | *(see file)* | Message shown in the disconnected screen |
| `InGameCommandPrefix` | `!whitelist` | Trigger prefix for in-game commands |
| `+AdminPlayerNames=` | *(none)* | In-game names allowed to run commands |
| `+WhitelistedPlayers=` | *(none)* | Pre-populate on first start only |

---

## Player List Persistence

The whitelist is stored in:
```
<ServerRoot>/FactoryGame/Saved/ServerWhitelist.json
```

Changes made via in-game commands are saved immediately.  
You can edit the JSON file directly while the server is stopped.  
Delete the file to reset to the `+WhitelistedPlayers=` entries in the config.

---

## Compatibility with DiscordBridge

When **DiscordBridge** is installed alongside ServerWhitelist:

- ServerWhitelist detects DiscordBridge at startup and **defers all enforcement** to it.
- Both mods share the same `ServerWhitelist.json` file so the player list stays in sync.
- Manage the whitelist via DiscordBridge's `!whitelist` commands in Discord as usual.

Use ServerWhitelist **without** DiscordBridge for a fully offline whitelist experience.

---

## Notes

- `AdminPlayerNames` uses in-game display names (case-insensitive). A player who changes their in-game name to match an admin's name could run commands. For higher security, use DiscordBridge's role-based whitelist management instead.
- Player names in `ServerWhitelist.json` are stored lower-case and compared case-insensitively.
