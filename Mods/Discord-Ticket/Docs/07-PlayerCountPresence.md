# DiscordBridge – Player Count Presence

← [Back to index](README.md)

Controls the **Discord bot status** (the "Now Playing" activity line shown on the
bot's profile), which can display the live player count.

---

## Config file

All player count presence settings live in the primary config file:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
```

This file ships with the mod and is created automatically on the first server start
if it is missing. Edit the file and restart the server to apply changes.

> **Backup:** the mod writes `<ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini` on every
> server start.  If a mod update resets `DefaultDiscordBridge.ini`, the backup is used to
> restore your settings automatically.

> **Other config files:** whitelist, ban, and ticket settings each have their own dedicated
> file in the same `Config/` folder.  See [Getting Started](01-GettingStarted.md) for the
> full list.

---

## Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `ShowPlayerCountInPresence` | bool | `True` | When `True`, the bot's Discord status is refreshed periodically to show the current player count. Set to `False` to leave the bot status blank. |
| `PlayerCountPresenceFormat` | string | `Satisfactory with %PlayerCount% players` | Text shown in the bot's Discord presence. Use `%PlayerCount%` for the live player count and `%ServerName%` for the server name. Leave empty to show just the raw player count number. |
| `PlayerCountUpdateIntervalSeconds` | float | `60.0` | How often (in seconds) the presence is refreshed. **Minimum is 15 seconds.** Values below 15 are clamped to 15 to respect Discord's rate limits. |
| `PlayerCountActivityType` | int | `0` | Controls the activity verb shown before the presence text in Discord. See table below. |

**Activity type values:**

| Value | Discord verb |
|-------|-------------|
| `0` | Playing |
| `2` | Listening to |
| `3` | Watching |
| `5` | Competing in |

---

## Examples

```ini
; "Playing Satisfactory with 4 players"
PlayerCountPresenceFormat=Satisfactory with %PlayerCount% players
PlayerCountActivityType=0

; "Watching My Server – 4 online"
PlayerCountPresenceFormat=%ServerName% – %PlayerCount% online
PlayerCountActivityType=3

; Disable presence entirely
ShowPlayerCountInPresence=False
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
