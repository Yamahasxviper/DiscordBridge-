# DiscordBridge – Chat Bridge (Message Formatting)

← [Back to index](README.md)

These settings control how messages are formatted when they cross the bridge in
either direction.

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

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
