# DiscordBridge – Stats Commands

← [Back to index](README.md)

DiscordBridge exposes two Discord commands that let server members query live
statistics without needing in-game access.

---

## `!stats` — server summary

Typing `!stats` in the bridged Discord channel returns a live server summary:

- Current online player count and player names
- Server uptime
- Aggregate session counters (total messages relayed, etc.)

The command respects `PlayersCommandChannelId` / `PlayersCommandPrefix` routing
but has its own dedicated prefix (`StatsCommandPrefix`).

### `StatsCommandPrefix`

The trigger word for the global stats command.

**Default:** `!stats`

Set to an **empty string** to disable the command entirely.

```ini
StatsCommandPrefix=!stats
```

---

## `!playerstats` — per-player session counters

Typing `!playerstats <PlayerName>` returns session statistics for the named
player (last seen, total time online, messages sent, etc.).

The player name is matched case-insensitively against the server's known
player history — the player does not need to be currently online.

### `PlayerStatsCommandPrefix`

The trigger word for the per-player stats command.

**Default:** `!playerstats`

Set to an **empty string** to disable the command entirely.

```ini
PlayerStatsCommandPrefix=!playerstats
```

**Usage:**
```
!playerstats SomePlayer
!playerstats some
```

---

## Full example

```ini
; Both commands enabled with defaults
StatsCommandPrefix=!stats
PlayerStatsCommandPrefix=!playerstats

; Rename the commands
StatsCommandPrefix=!serverstats
PlayerStatsCommandPrefix=!ps

; Disable both
StatsCommandPrefix=
PlayerStatsCommandPrefix=
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
