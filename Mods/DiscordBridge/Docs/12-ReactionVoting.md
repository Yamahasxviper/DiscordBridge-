# DiscordBridge – Reaction Voting

← [Back to index](README.md)

Reaction voting lets Discord members vote to kick a newly-joined player by
reacting with 👎 to the player's join embed. If enough 👎 reactions accumulate
within the vote window, the player is automatically kicked.

---

## Requirements

- `bUseEmbedsForPlayerEvents=True` — voting uses the embed posted for each
  joining player; plain-text join messages do not support reactions.
- `PlayerEventsEnabled=True` — player join notifications must be enabled.
- `bEnableJoinReactionVoting=True` — master switch for the voting system.

---

## Settings

### `bEnableJoinReactionVoting`

Master switch for reaction voting. When `True`, DiscordBridge monitors 👎
reactions on join embeds and kicks the player if the threshold is reached.

**Default:** `False`

```ini
bEnableJoinReactionVoting=True
```

---

### `VoteKickThreshold`

The minimum number of 👎 reactions required to kick a player.

- Set to `0` to disable auto-kick (reactions are collected but no kick fires).
- Set to a positive integer to enable kicking.

**Default:** `0`

```ini
VoteKickThreshold=3
```

---

### `VoteWindowMinutes`

How long (in minutes) after the join embed is posted the vote window remains
open. Reactions added after this window are ignored.

**Default:** `5`

```ini
VoteWindowMinutes=10
```

---

## How it works

1. A player joins → DiscordBridge posts a join embed to `PlayerEventsChannelId`
   (or `ChannelId`).
2. DiscordBridge opens a vote window of `VoteWindowMinutes` minutes for that embed.
3. Discord members add 👎 reactions.
4. When the 👎 count reaches `VoteKickThreshold`, DiscordBridge kicks the player
   from the game with a standard kick message.
5. When the window expires without reaching the threshold, no action is taken.

---

## Minimal configuration

```ini
PlayerEventsEnabled=True
bUseEmbedsForPlayerEvents=True
bEnableJoinReactionVoting=True
VoteKickThreshold=3
VoteWindowMinutes=5
```

---

## Notes

- The bot needs the **Add Reactions** permission in the player events channel to
  pre-populate the 👎 reaction button on new join embeds.
- The **Message Content Intent** and **Server Members Intent** must be enabled on
  your Discord bot application.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
