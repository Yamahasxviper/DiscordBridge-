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
