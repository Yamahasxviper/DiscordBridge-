# DiscordBridge – Stats Commands

← [Back to index](README.md)

DiscordBridge exposes two Discord slash commands that let server members query live
statistics without needing in-game access.

> **Note:** These are now Discord slash commands (`/`). The old `!stats` and
> `!playerstats` prefix commands and their `StatsCommandPrefix` /
> `PlayerStatsCommandPrefix` config fields have been removed.

---

## `/stats` — server summary

Typing `/stats` in any channel where the bot is present returns a live server summary:

- Current online player count and player names
- Server uptime
- Aggregate session counters (total messages relayed, etc.)

---

## `/playerstats` — per-player session counters

Typing `/playerstats <PlayerName>` returns session statistics for the named
player (last seen, total time online, messages sent, etc.).

The player name is matched case-insensitively against the server's known
player history — the player does not need to be currently online.

**Usage:**
```
/playerstats SomePlayer
/playerstats some
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
