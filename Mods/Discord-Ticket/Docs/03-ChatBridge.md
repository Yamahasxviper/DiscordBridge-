# DiscordBridge – Chat Bridge (Message Formatting)

← [Back to index](README.md)

These settings control how messages are formatted when they cross the bridge in
either direction.

---

## Config file

All chat bridge settings live in the primary config file:

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

## `GameToDiscordFormat`

Format string applied when an in-game player chat message is forwarded to Discord.

| Placeholder | Replaced with |
|-------------|---------------|
| `%ServerName%` | The value of `ServerName` |
| `%PlayerName%` | The in-game name of the player who sent the message |
| `%Message%` | The raw chat message text |

**Default:** `**%PlayerName%**: %Message%`

**Examples:**

```ini
; Bold name, plain message
GameToDiscordFormat=**%PlayerName%**: %Message%

; Include server label
GameToDiscordFormat=**[%ServerName%] %PlayerName%**: %Message%

; Code-formatted name
GameToDiscordFormat=`[%PlayerName%]` %Message%
```

---

## `DiscordToGameFormat`

Format string that controls the **complete line** shown in the Satisfactory in-game
chat when a Discord message is relayed into the game.

| Placeholder | Replaced with |
|-------------|---------------|
| `%Username%` | The Discord display name of the sender |
| `%PlayerName%` | Alias for `%Username%` |
| `%Message%` | The Discord message text |

**Default:** `[Discord] %Username%: %Message%`

**Examples:**

```ini
; Default – prefixed with [Discord]
DiscordToGameFormat=[Discord] %Username%: %Message%

; Name only, no prefix
DiscordToGameFormat=%Username%: %Message%

; Branded prefix
DiscordToGameFormat=[Satisfactory] %PlayerName%: %Message%

; Message only, no username shown
DiscordToGameFormat=%Message%
```

---

## `PlayerJoinMessage`

Format string posted to the bridged Discord channel when a player joins the server.
Clear this value to disable join notifications.

| Placeholder | Replaced with |
|-------------|---------------|
| `%ServerName%` | The value of `ServerName` |
| `%PlayerName%` | The in-game name of the player who joined |
| `%Platform%` | The platform display name — e.g. `Steam` or `Epic Games Store` |

**Default:** `:arrow_right: **%PlayerName%** joined the server.`

**Examples:**

```ini
; Default join message
PlayerJoinMessage=:arrow_right: **%PlayerName%** joined the server.

; Include the platform the player joined from
PlayerJoinMessage=:arrow_right: **%PlayerName%** joined via %Platform%.

; Disable join notifications
PlayerJoinMessage=
```

---

## `PlayerLeaveMessage`

Format string posted to the bridged Discord channel when a player leaves the server.
Clear this value to disable leave notifications.

| Placeholder | Replaced with |
|-------------|---------------|
| `%ServerName%` | The value of `ServerName` |
| `%PlayerName%` | The in-game name of the player who left |
| `%Platform%` | The platform display name — e.g. `Steam` or `Epic Games Store` |

**Default:** `:arrow_left: **%PlayerName%** left the server.`

**Examples:**

```ini
; Default leave message
PlayerLeaveMessage=:arrow_left: **%PlayerName%** left the server.

; Include the platform
PlayerLeaveMessage=:arrow_left: **%PlayerName%** (%Platform%) left the server.

; Disable leave notifications
PlayerLeaveMessage=
```

---



[SML chat commands](https://docs.ficsit.app/satisfactory-modding/latest/SMLChatCommands.html)
use a `/` prefix (e.g. `/save`, `/tp`).  They are handled entirely inside the
Satisfactory game engine and **cannot** be sent via the Discord bridge:

- A Discord message that starts with `/` is **not** forwarded to the game.
  The bot replies with an informational message explaining this.
- An in-game chat message that starts with `/` is **not** forwarded to Discord.
  It is treated as a private command, not a public chat message.
- SML chat commands also do **not** work when typed in the Unreal Engine server
  console (the terminal / RCON interface). They must be entered in the
  in-game chat UI by a connected player.

> **Bot commands are not affected.**
> The filter only applies to regular chat messages that are being relayed
> between Discord and the game.  Bot commands (`!whitelist`, `!ban`,
> `!server`, `!admin`) are matched and handled *before* the `/` filter
> runs and are completely unaffected — including any custom command prefix
> you have configured, even if it starts with `/`.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
