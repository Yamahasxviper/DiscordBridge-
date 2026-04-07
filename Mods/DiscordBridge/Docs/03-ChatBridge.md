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

## `ChatRelayBlocklist`

A list of words or phrases that, when found in a player's in-game chat message
(case-insensitive substring match), cause the entire message to be **silently
dropped** and not relayed to Discord.

```ini
+ChatRelayBlocklist=http://
+ChatRelayBlocklist=badword
```

Add one `+ChatRelayBlocklist=` line per entry.

---

## `ChatRelayBlocklistReplacements`

A list of **find-and-replace rules** applied to in-game chat messages before they
are relayed to Discord. Unlike `ChatRelayBlocklist` (which drops the whole
message), replacements substitute only the matched text — the rest of the message
is still forwarded.

Each entry is a `(Pattern, Replacement)` pair. `Pattern` is a case-insensitive
substring match. `Replacement` defaults to `***` when omitted.

Rules are applied in the order they are listed.

```ini
; Censor a specific word (default *** replacement)
+ChatRelayBlocklistReplacements=(Pattern="badword")

; Censor with a custom replacement
+ChatRelayBlocklistReplacements=(Pattern="slur",Replacement="[removed]")

; Replace a URL pattern
+ChatRelayBlocklistReplacements=(Pattern="http://evil.example.com",Replacement="[link removed]")
```

> **Tip:** Use `ChatRelayBlocklist` to drop messages that are entirely spam.
> Use `ChatRelayBlocklistReplacements` to sanitise specific words while still
> relaying the rest of the message.

---

## `DiscordInviteUrl`

When set, the Discord server invite URL is periodically announced in the game
chat so players can find the Discord without being told manually.

```ini
DiscordInviteUrl=https://discord.gg/yourInvite
```

Leave empty to disable this announcement (default).

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
