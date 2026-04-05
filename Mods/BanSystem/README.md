# BanSystem — Satisfactory Dedicated Server Ban Mod

**Version 1.1.0** | Server-only | Requires SML `^3.11.3` | Game build `>=416835`

A server-only Alpakit C++ mod that provides a persistent, EOS-based ban system for Satisfactory dedicated servers. Bans are stored in a single JSON file and enforced at login time — banned players are kicked before they ever enter the game world.

---

## Features

| Feature | Details |
|---------|---------|
| Permanent bans | ✅ EOS |
| Timed (temporary) bans | ✅ EOS |
| Auto-expiry pruning | ✅ On startup and via REST API |
| Persistent JSON storage | ✅ Survives restarts and updates |
| Login-time enforcement | ✅ PostLogin hook + 20 s identity polling |
| UID linking | ✅ Link multiple EOS UIDs for the same player |
| REST management API | ✅ HTTP on configurable port (default 3000) |
| Player session registry | ✅ Audit log of all known UIDs and names |
| Blueprint-accessible API | ✅ Full UE Blueprint support |
| Thread-safe | ✅ Game thread + REST API thread safe |

---

## In-Game Commands (via BanChatCommands mod)

Install the optional **BanChatCommands** mod to get the full set of in-game chat commands. Admin access is controlled by player platform ID in `DefaultGame.ini`.

| Command | Description |
|---------|-------------|
| `/ban <player\|UID> [reason...]` | Permanently ban a player |
| `/tempban <player\|UID> <minutes> [reason...]` | Temporarily ban for N minutes |
| `/unban <UID>` | Remove a ban |
| `/bancheck <player\|UID>` | Query ban status |
| `/banlist [page]` | List active bans (10 per page) |
| `/linkbans <UID1> <UID2>` | Link two EOS UIDs for the same player |
| `/unlinkbans <UID1> <UID2>` | Remove a UID link |
| `/playerhistory <name\|UID>` | Look up session history |
| `/whoami` | Show your own compound UID *(no admin required)* |

→ See [BanChatCommands README](../BanChatCommands/README.md) for setup.

---

## REST API

The mod starts a local HTTP server (default port **3000**) with the same endpoints as the original Node.js Tools/BanSystem:

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/health` | Liveness probe |
| `GET` | `/bans` | Active bans |
| `GET` | `/bans/all` | All bans including expired |
| `GET` | `/bans/check/:uid` | Check if a UID is banned |
| `POST` | `/bans` | Create a ban |
| `DELETE` | `/bans/:uid` | Remove by compound UID |
| `DELETE` | `/bans/id/:id` | Remove by row ID |
| `POST` | `/bans/prune` | Delete expired bans |
| `POST` | `/bans/backup` | Create a database backup |

Set `RestApiPort=0` in `DefaultBanSystem.ini` to disable the REST API entirely.

→ See [REST API](Docs/04-RestApi.md)

---

## Storage

Bans are persisted to a single JSON file immediately after every change:

```
<ProjectSaved>/BanSystem/bans.json
```

A second file (`player_sessions.json` in the same directory) records every known player UID and display name as an audit log.

| OS | Default path |
|----|-------------|
| Windows | `C:\SatisfactoryServer\FactoryGame\Saved\BanSystem\` |
| Linux | `~/.config/Epic/FactoryGame/Saved/BanSystem/` |

---

## Configuration

Edit `<ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini`:

```ini
[/Script/BanSystem.BanSystemConfig]
DatabasePath=          ; leave empty for default Saved/BanSystem/bans.json
RestApiPort=3000       ; HTTP REST port; set 0 to disable
MaxBackups=5           ; automatic backup count limit
```

→ See [Configuration](Docs/02-Configuration.md) for the full reference.

---

## UID Format

All bans use **compound UIDs** that encode both platform and raw ID in one string:

| Platform | Format | Example |
|----------|--------|---------|
| EOS | `EOS:<32-char hex>` | `EOS:00020aed06f0a6958c3c067fb4b73d51` |

Use `/whoami` in-game to see your own UID.

---

## Dependencies

| Dependency | Version |
|------------|---------|
| SML | `^3.11.3` |
| Satisfactory (dedicated server) | `>=416835` |

---

## Documentation

→ [Full documentation index](Docs/README.md)

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
