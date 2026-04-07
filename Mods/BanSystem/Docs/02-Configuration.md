# BanSystem – Configuration

← [Back to index](README.md)

All BanSystem settings are read from the mod's default INI file:

```
<ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
```

This file ships with the mod and may be replaced when you update BanSystem.
**To keep your settings across updates**, put your overrides in the server's
saved config instead — UE reads it last and it is never touched by mod updates:

```
<ServerRoot>/FactoryGame/Saved/Config/<Platform>/BanSystem.ini
```

Both files use the same `[/Script/BanSystem.BanSystemConfig]` section header.
Restart the server for changes to take effect.

---

## Settings

### `DatabasePath`

Absolute path to the JSON ban file.
Leave **empty** to use the default location.

**Default:** *(empty — resolves to `<ProjectSaved>/BanSystem/bans.json`)*

| OS | Default resolved path |
|----|-----------------------|
| Windows | `C:\SatisfactoryServer\FactoryGame\Saved\BanSystem\bans.json` |
| Linux | `~/.config/Epic/FactoryGame/Saved/BanSystem/bans.json` |

**Example:**

```ini
DatabasePath=/data/satisfactory/bans.json
```

---

### `RestApiPort`

Port for the local HTTP management REST API.
The API binds to all network interfaces (`0.0.0.0`).

> **Security:** Restrict external access with your server firewall. The REST API has no authentication — anyone who can reach the port can manage bans.

**Default:** `3000`

Set to `0` to disable the REST API entirely.

**Example:**

```ini
RestApiPort=3001
```

---

### `MaxBackups`

Maximum number of database backup files to keep.
A backup is created on demand by calling `POST /bans/backup` on the REST API.
When the limit is reached, the oldest backup is deleted automatically.

**Default:** `5`

Backups are stored alongside `bans.json` in the same directory, named:

```
bans_YYYY-MM-DD_HH-MM-SS.json
```

**Example:**

```ini
MaxBackups=10
```

---

## Full example `DefaultBanSystem.ini`

```ini
[/Script/BanSystem.BanSystemConfig]

; Custom ban file location (leave empty for default)
DatabasePath=

; REST API port; set 0 to disable
RestApiPort=3000

; How many backup files to keep
MaxBackups=5

; API key for REST mutating requests (leave empty to disable auth)
RestApiKey=

; Discord webhook for ban/warn/kick notifications (leave empty to disable)
DiscordWebhookUrl=

; Auto-ban after N warnings (0 = disabled)
AutoBanWarnCount=0
AutoBanWarnMinutes=0
```

---

## `RestApiKey`

Optional API key that gates all mutating REST endpoints (`POST`, `DELETE`, `PATCH`).
When non-empty, requests to those endpoints must include the header:

```
X-Api-Key: <value>
```

Read-only `GET` endpoints are never gated.

Leave **empty** to disable API key authentication (safe only on a firewalled server).

**Default:** *(empty — no authentication)*

```ini
RestApiKey=mysecretkey
```

---

## `DiscordWebhookUrl`

Optional Discord webhook URL. When set, `BanDiscordNotifier` posts an embed to
this URL whenever a ban is created or removed, a warning is issued, or a player
is kicked.

Leave **empty** to disable Discord notifications (default).

```ini
DiscordWebhookUrl=https://discord.com/api/webhooks/...
```

---

## `AutoBanWarnCount`

The number of warnings that trigger an **automatic ban** (default: `0` = disabled).
When a player reaches this many warnings via `/warn`, BanSystem bans them
automatically with reason `"Auto-banned: reached warning threshold"`.

```ini
AutoBanWarnCount=3
```

---

## `AutoBanWarnMinutes`

Duration in minutes for the automatic ban issued when `AutoBanWarnCount` is
reached. Set to `0` for a permanent auto-ban.

```ini
AutoBanWarnMinutes=1440   ; 24-hour auto-ban
```

---

## `WarnEscalationTiers`

Warning-escalation tiers for fine-grained automatic bans based on warning count.
Each tier specifies a `WarnCount` threshold and a `DurationMinutes` (0 = permanent).
Tiers are evaluated in ascending order; the highest matching tier wins.

`AutoBanWarnCount` / `AutoBanWarnMinutes` act as a simple single-tier fallback when
this array is empty.

```ini
+WarnEscalationTiers=(WarnCount=2,DurationMinutes=30)
+WarnEscalationTiers=(WarnCount=3,DurationMinutes=1440)
+WarnEscalationTiers=(WarnCount=5,DurationMinutes=0)
```

---

## `SessionRetentionDays`

Number of days to keep player session records in `player_sessions.json`.
Records older than this value are pruned when `POST /players/prune` is called.
Set to `0` to keep records forever (default).

```ini
SessionRetentionDays=90
```

---

## `BackupIntervalHours`

Interval in hours between automatic database backups. When non-zero, BanSystem
schedules a recurring timer that calls `UBanDatabase::Backup()` on this interval.
Set to `0` to rely solely on the manual `POST /bans/backup` endpoint (default).

```ini
BackupIntervalHours=24
```

---

## `bNotifyBanExpired`

When `True` and `DiscordWebhookUrl` is set, BanSystem posts a Discord notification
whenever a temporary ban expires and the player is allowed to reconnect.

**Default:** `False`

```ini
bNotifyBanExpired=True
```

---

## BanChatCommands admin config

If you are using the **BanChatCommands** mod, add admin EOS PUIDs to the BanChatCommands override config file (not `DefaultBanSystem.ini` and not `DefaultGame.ini`):

```
<ServerRoot>/FactoryGame/Saved/Config/<Platform>/BanChatCommands.ini
```

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d52
```

This file is in the server's `Saved/Config/` directory and is **never overwritten** by mod updates. Use one `+AdminEosPUIDs=` line per admin; the value is the player's raw 32-character hex EOS PUID (without the `EOS:` prefix).

Use `/whoami` in-game to find your own EOS PUID.

→ See the [BanChatCommands Configuration guide](../../BanChatCommands/Docs/02-Configuration.md) for the full admin setup reference.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
