# BanSystem — Satisfactory Dedicated Server Ban Mod

**Version 1.1.0** | Server-only | Requires SML `^3.11.3` | Game build `>=416835`

A server-only Alpakit C++ mod that provides a persistent, EOS-based ban system for Satisfactory dedicated servers. Bans are stored in a single JSON file and enforced at login time — banned players are kicked before they ever enter the game world. Both **EOS PUID bans** and **IP address bans** are supported.

---

## Features

| Feature | Details |
|---------|---------|
| Permanent bans | ✅ EOS PUID + IP address |
| Timed (temporary) bans | ✅ EOS PUID + IP address |
| Auto-expiry pruning | ✅ On startup and via REST API |
| Persistent JSON storage | ✅ Survives restarts and updates |
| Login-time enforcement | ✅ PostLogin hook + 20 s identity polling |
| IP ban enforcement | ✅ PreLogin hook caches remote IP; checked at every login |
| UID linking | ✅ Link multiple EOS UIDs (or EOS + IP) for the same player |
| REST management API | ✅ 42 endpoints on configurable port (default 3000) |
| Player session registry | ✅ Audit log of all known UIDs, names, and IP addresses |
| Warning system | ✅ Configurable auto-ban escalation tiers |
| Scheduled bans | ✅ Deferred ban enforcement at a future timestamp |
| Ban appeals | ✅ Self-service portal and Discord notifications |
| Discord webhook notifications | ✅ Bans, warnings, kicks, appeals, auto-escalation, geo-IP blocks |
| Multi-server ban sync | ✅ WebSocket-based peer synchronisation |
| Player reputation scoring | ✅ Composite score based on warnings, bans, and session history |
| Prometheus metrics export | ✅ `/metrics/prometheus` endpoint |
| CSV export | ✅ Bans, warnings, sessions, and audit logs |
| Admin dashboard | ✅ Unified SPA at `/dashboard` |
| Appeals portal | ✅ HTML self-service form at `/appeals/portal` |
| Automatic scheduled backups | ✅ Configurable interval |
| Blueprint-accessible API | ✅ Full UE Blueprint support |
| Thread-safe | ✅ Game thread + REST API thread safe |

---

## In-Game Commands (via BanChatCommands mod)

Install the optional **BanChatCommands** mod to get the full set of 43 in-game chat commands. Admin access is controlled by player platform ID in `BanChatCommands.ini`.

| Command | Role | Description |
|---------|------|-------------|
| `/ban <player\|UID\|IP:address> [reason...]` | Admin | Permanently ban a player or IP address |
| `/tempban <player\|UID\|IP:address> <minutes> [reason...]` | Admin | Temporarily ban for N minutes |
| `/unban <UID\|IP:address>` | Admin | Remove a ban |
| `/unbanname <name_substring>` | Admin | Remove ban for an offline player by display-name |
| `/bancheck <player\|UID\|IP:address>` | Admin | Query ban status |
| `/banlist [page]` | Admin | List active bans (10 per page) |
| `/linkbans <UID1> <UID2>` | Admin | Link two UIDs for the same player |
| `/unlinkbans <UID1> <UID2>` | Admin | Remove a UID link |
| `/playerhistory <name\|UID>` | Admin | Look up session history |
| `/banname <name> [reason...]` | Admin | Ban offline player by name + IP from session history |
| `/reloadconfig` | Admin | Hot-reload admin/moderator config without restarting |
| `/warn <player\|UID> <reason...>` | Admin | Issue a formal warning |
| `/warnings <player\|UID>` | Admin | List all warnings for a player |
| `/clearwarns <player\|UID>` | Admin | Remove all warnings for a player |
| `/clearwarn <player\|UID> <id>` | Admin | Remove a specific warning by ID |
| `/reason <UID>` | Admin | Show the ban reason for a UID |
| `/banreason <UID> <new reason>` | Admin | Edit the ban reason for a UID |
| `/announce <message...>` | Admin | Server-wide broadcast (also posts to Discord) |
| `/stafflist` | Admin | Show currently-online admins and moderators |
| `/note <player\|UID> <text>` | Admin | Add an admin note to a player |
| `/notes <player\|UID>` | Admin | List all admin notes for a player |
| `/duration <UID>` | Admin | Show remaining tempban duration |
| `/extend <UID> <minutes>` | Admin | Extend a temporary ban duration |
| `/appeal <UID>` | Admin | Manage ban appeals |
| `/scheduleban <player\|UID> <timestamp> [reason]` | Admin | Schedule a future ban |
| `/qban <template> <player\|UID>` | Admin | Apply a quick-ban template |
| `/reputation <player\|UID>` | Admin | Show player reputation score |
| `/bulkban <UID1> <UID2> ... [reason]` | Admin | Ban multiple players at once |
| `/staffchat <message...>` | Admin | Staff-only message |
| `/kick <player\|UID> [reason...]` | Moderator | Disconnect without banning |
| `/modban <player\|UID> [reason...]` | Moderator | 30-minute temporary ban |
| `/mute <player\|UID> [minutes] [reason...]` | Moderator | Silence in-game chat |
| `/unmute <player\|UID>` | Moderator | Remove a chat mute |
| `/tempmute <player\|UID> <minutes> [reason...]` | Moderator | Timed mute |
| `/tempunmute <player\|UID>` | Moderator | Remove a timed mute |
| `/mutecheck <player\|UID>` | Moderator | Check mute status |
| `/mutelist` | Moderator | List all active mutes |
| `/mutereason <player\|UID> <reason>` | Moderator | Edit mute reason |
| `/freeze <player\|UID>` | Moderator | Immobilise a player (toggle) |
| `/clearchat` | Moderator | Flush chat history |
| `/report <player\|UID> <reason>` | Moderator | Submit a player report |
| `/history` | All | Show your own session and warning history |
| `/whoami` | All | Show your own compound UID *(no admin required)* |

→ See [BanChatCommands README](../BanChatCommands/README.md) for setup.

---

## REST API

The mod starts a local HTTP server (default port **3000**) with a comprehensive REST API:

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/health` | Liveness probe |
| `GET` | `/bans` | Active bans |
| `GET` | `/bans/all` | All bans including expired |
| `GET` | `/bans/search?name=` | Search bans by name |
| `GET` | `/bans/check/:uid` | Check if a UID is banned |
| `GET` | `/bans/export-csv` | Export bans as CSV |
| `POST` | `/bans` | Create a ban |
| `POST` | `/bans/ip` | Create an IP ban |
| `POST` | `/bans/bulk` | Bulk ban operations |
| `PATCH` | `/bans/:uid` | Update an existing ban (reason, duration, make permanent) |
| `DELETE` | `/bans/:uid` | Remove by compound UID |
| `DELETE` | `/bans/id/:id` | Remove by row ID |
| `DELETE` | `/bans/ip/:ip` | Remove an IP ban |
| `POST` | `/bans/prune` | Delete expired bans |
| `POST` | `/bans/backup` | Create a database backup |
| `GET` | `/players` | List player sessions |
| `GET` | `/players/search?name=` | Search players by name |
| `GET` | `/players/export-csv` | Export players as CSV |
| `POST` | `/players/prune` | Prune old session records |
| `GET` | `/warnings` | List warnings (optional `?uid=` filter) |
| `GET` | `/warnings/export-csv` | Export warnings as CSV |
| `POST` | `/warnings` | Issue a warning |
| `DELETE` | `/warnings/:uid` | Clear all warnings for a UID |
| `DELETE` | `/warnings/id/:id` | Remove a single warning |
| `GET` | `/audit` | View audit log |
| `GET` | `/audit/export-csv` | Export audit log as CSV |
| `GET` | `/metrics` | Server statistics |
| `GET` | `/metrics/prometheus` | Prometheus text format |
| `GET` | `/reputation/:uid` | Player reputation score |
| `POST` | `/appeals` | Submit a ban appeal |
| `GET` | `/appeals` | List appeals |
| `GET` | `/appeals/:id` | Get a single appeal |
| `DELETE` | `/appeals/:id` | Dismiss an appeal |
| `GET` | `/appeals/portal` | Self-service appeals HTML form |
| `GET` | `/dashboard` | Unified admin dashboard SPA |
| `GET` | `/scheduled` | List scheduled bans |
| `POST` | `/scheduled` | Schedule a future ban |
| `DELETE` | `/scheduled/:id` | Delete a scheduled ban |
| `POST` | `/notes` | Add admin notes |

Set `RestApiPort=0` in `DefaultBanSystem.ini` to disable the REST API entirely.
Set `RestApiKey` to require an `X-Api-Key` header on all mutating requests.

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
DatabasePath=              ; leave empty for default Saved/BanSystem/bans.json
RestApiPort=3000           ; HTTP REST port; set 0 to disable
MaxBackups=5               ; max backup file count
RestApiKey=                ; optional API key for mutating REST requests
DiscordWebhookUrl=         ; optional Discord webhook for ban/warn/kick notifications
bNotifyBanExpired=False    ; notify Discord when a temp ban expires
AutoBanWarnCount=0         ; warnings before auto-ban (0 = disabled)
AutoBanWarnMinutes=0       ; auto-ban duration in minutes (0 = permanent)
WarnEscalationTiers=       ; multi-tier auto-ban thresholds
SessionRetentionDays=0     ; session record age limit (0 = keep forever)
BackupIntervalHours=0      ; recurring auto-backup interval (0 = disabled)
PruneIntervalHours=0       ; auto-prune expired bans interval (0 = disabled)
bPushEventsToWebSocket=False ; push ban events to WebSocket endpoint
BanTemplates=              ; quick-ban presets
AdminBanRateLimitCount=0   ; rate limit count (0 = disabled)
AdminBanRateLimitMinutes=0 ; rate limit window
ChatFilterAutoWarnThreshold=0 ; auto-warn on chat filter hits (0 = disabled)
```

→ See [Configuration](Docs/02-Configuration.md) for the full reference.

---

## UID Format

All bans use **compound UIDs** that encode both platform and raw ID in one string:

| Platform | Format | Example |
|----------|--------|---------|
| EOS | `EOS:<32-char hex>` | `EOS:00020aed06f0a6958c3c067fb4b73d51` |
| IP | `IP:<address>` | `IP:1.2.3.4` |

Use `/whoami` in-game to see your own EOS UID. Use `/playerhistory <name>` to find a player's IP from session records.

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
