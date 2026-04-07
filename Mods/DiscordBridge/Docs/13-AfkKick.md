# DiscordBridge – AFK Kick

← [Back to index](README.md)

DiscordBridge can automatically kick players who have been idle (AFK) for longer
than a configurable number of minutes. Idle time is tracked per-player and resets
whenever the player moves or sends a chat message.

---

## `AfkKickMinutes`

The number of minutes of inactivity before a player is kicked.

- Set to `0` (the default) to **disable** AFK kicking entirely.
- Set to a positive integer to enable it.

**Default:** `0`

```ini
; Kick players who have been AFK for 30 minutes
AfkKickMinutes=30
```

---

## `AfkKickReason`

The kick reason string displayed to the kicked player.

**Default:** `Kicked for inactivity (AFK).`

```ini
AfkKickReason=You were kicked for being AFK too long.
```

---

## How it works

When `AfkKickMinutes` is greater than zero, DiscordBridge starts a background
ticker that fires every minute. On each tick it checks every connected player's
last-activity timestamp. Players whose idle time exceeds `AfkKickMinutes` minutes
are kicked with `AfkKickReason`.

Activity is updated by:
- Player movement (position change detected by the subsystem)
- Player sending a chat message

---

## Full example

```ini
; Enable AFK kick at 45 minutes
AfkKickMinutes=45
AfkKickReason=You have been idle for too long and were kicked. Rejoin anytime!
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
