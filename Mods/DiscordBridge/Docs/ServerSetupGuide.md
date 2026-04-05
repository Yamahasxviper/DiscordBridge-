# DiscordBridge – Server Setup & Configuration Guide

> **This guide has been split into focused sections to make it easier to find what you need.**
> See the [Documentation Index](README.md) for the full list of guides.

---

This guide explains every setting available in `DefaultDiscordBridge.ini` so you can
get the Discord ↔ Satisfactory chat bridge running and fine-tune it to your liking.

---

## Where is the config file?

After installing the mod the **primary config** lives at:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
```

Edit that file, then restart the server. Your changes take effect on the next start.

> **Tip – surviving mod updates**
> The mod automatically writes a backup of your `BotToken` and `ChannelId` to
> `<ServerRoot>/FactoryGame/Saved/DiscordBridge/DiscordBridge.ini` every time the server
> starts. If a mod update resets the primary config, the bridge falls back to that
> backup so it keeps working until you copy your credentials back.

---

## Step 1 – Create a Discord Bot

1. Go to <https://discord.com/developers/applications> and click **New Application**.
2. Give it a name (e.g. *My Satisfactory Bot*), then open the **Bot** tab.
3. Click **Reset Token** and copy the token – paste it as `BotToken` below.
4. Under **Privileged Gateway Intents** enable:
   - **Server Members Intent**
   - **Message Content Intent**
5. Under **OAuth2 → URL Generator** tick `bot`, then tick the permissions
   **Send Messages** and **Read Message History**.
6. Open the generated URL in a browser and invite the bot to your server.
7. Enable **Developer Mode** in Discord (User Settings → Advanced), right-click
   the target text channel, and choose **Copy Channel ID**. Paste it as `ChannelId`.

---

## Setting Reference

### CONNECTION

| Setting    | Type   | Default | Description |
|------------|--------|---------|-------------|
| `BotToken` | string | *(empty)* | Your Discord bot token. Treat this like a password – never share it. The bridge will not start if this is empty. |
| `ChannelId` | string | *(empty)* | The snowflake ID of the Discord text channel to bridge. Must be a numeric ID (e.g. `123456789012345678`). |
| `ServerName` | string | *(empty)* | A display name for this server. Used as the `%ServerName%` placeholder in message formats and the player-count presence. Example: `My Satisfactory Server`. |

---

### CHAT TWO-WAY CUSTOMISATION

These settings control how messages appear on each side of the bridge.

#### `GameToDiscordFormat`

Format string applied when an in-game player chat message is forwarded to Discord.

| Placeholder   | Replaced with |
|---------------|---------------|
| `%ServerName%` | The value of `ServerName` |
| `%PlayerName%` | The in-game name of the player who sent the message |
| `%Message%`    | The raw chat message text |

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

#### `DiscordToGameFormat`

Format string that controls the **complete line** shown in the Satisfactory in-game
chat when a Discord message is relayed into the game.

| Placeholder   | Replaced with |
|---------------|---------------|
| `%Username%`   | The Discord display name of the sender |
| `%PlayerName%` | Alias for `%Username%` |
| `%Message%`    | The Discord message text |

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

### WHITELIST

Controls the built-in server whitelist that can be managed from Discord.

#### `WhitelistEnabled`

Controls whether the whitelist is active when the server starts.
This setting is applied on **every** server restart — change it and restart the server to enable or disable the whitelist.

`!whitelist on` / `!whitelist off` Discord commands update the in-memory state for the current session only; the config setting takes effect again on the next restart.

**Default:** `False` (all players can join)

---

#### `WhitelistCommandRoleId`

The snowflake ID of the Discord role whose members are allowed to run `!whitelist` management commands.

**Default:** *(empty — !whitelist commands are disabled for all Discord users)*

When set, **only members who hold this role** can run `!whitelist` commands. When left empty, `!whitelist` commands are fully disabled (deny-by-default).

**Example:**
```ini
WhitelistCommandRoleId=123456789012345678
```

---

#### `WhitelistCommandPrefix`

The prefix that triggers whitelist management commands when typed in the bridged Discord channel.
Set to an **empty string** to disable Discord-based whitelist management entirely.

**Default:** `!whitelist`

**Supported commands** (type these in the bridged Discord channel):

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist (only whitelisted players can join) |
| `!whitelist off` | Disable the whitelist (all players can join) |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players and current enabled/disabled state |
| `!whitelist status` | Show the current enabled/disabled state of **both** the whitelist and the ban system |
| `!whitelist role add <discord_id>` | Grant the `WhitelistRoleId` Discord role to a user |
| `!whitelist role remove <discord_id>` | Revoke the `WhitelistRoleId` Discord role from a user |

---

#### `WhitelistRoleId`

The snowflake ID of the Discord role used to identify whitelisted members.
Leave **empty** to disable Discord role integration.

**Default:** *(empty)*

When set:
- Discord messages sent to `WhitelistChannelId` are relayed to the game **only when the sender holds this role**.
- The `!whitelist role add/remove <discord_id>` commands assign or revoke this role via the Discord REST API (the bot must have the **Manage Roles** permission on your server).

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

---

#### `WhitelistChannelId`

The snowflake ID of a dedicated Discord channel for whitelisted members.
Leave **empty** to disable the whitelist-only channel.

**Default:** *(empty)*

When set:
- In-game messages from players on the server whitelist are **also** posted to this channel (in addition to the main `ChannelId`).
- Discord messages sent to this channel are relayed to the game **only when the sender holds `WhitelistRoleId`** (if `WhitelistRoleId` is configured).

Get the channel ID the same way as `ChannelId` (right-click the channel in Discord with Developer Mode enabled → **Copy Channel ID**).

---

#### `WhitelistKickDiscordMessage`

The message posted to the **main** Discord channel whenever a non-whitelisted player attempts to join and is kicked.
Leave **empty** to disable this notification.

**Default:** `:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.`

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | The in-game name of the player who was kicked |

**Example:**

```ini
WhitelistKickDiscordMessage=:no_entry: **%PlayerName%** is not whitelisted and was removed from the server.
```

---

#### `WhitelistKickReason`

The reason shown **in-game** to the player when they are kicked for not being on the whitelist.
This is the text the player sees in the disconnected / kicked screen.

**Default:** `You are not on this server's whitelist. Contact the server admin to be added.`

**Example:**

```ini
WhitelistKickReason=You are not whitelisted. DM an admin on Discord to request access.
```

---

### IN-GAME COMMANDS

These settings control commands that server admins can type directly in the **Satisfactory in-game chat** to manage the whitelist and ban list without using Discord.

#### `InGameWhitelistCommandPrefix`

The prefix that triggers whitelist management commands in the in-game chat.
Set to an **empty string** to disable in-game whitelist commands.

**Default:** `!whitelist`

**Supported commands** (type these in the Satisfactory in-game chat):

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist |
| `!whitelist off` | Disable the whitelist (all players can join) |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players and current enabled/disabled state |
| `!whitelist status` | Show the current enabled/disabled state of **both** the whitelist and the ban system |

> **Note:** In-game whitelist commands do not support `!whitelist role add/remove` (that is Discord-only).

---

#### `InGameBanCommandPrefix`

The prefix that triggers ban management commands in the in-game chat.
Set to an **empty string** to disable in-game ban commands.

**Default:** `!ban`

**Supported commands** (type these in the Satisfactory in-game chat):

| Command | Effect |
|---------|--------|
| `!ban on` | Enable the ban system; currently-connected banned players are kicked immediately |
| `!ban off` | Disable the ban system |
| `!ban add <name>` | Ban a player by in-game name; kicks immediately if the player is connected and ban system is enabled |
| `!ban remove <name>` | Unban a player by in-game name |
| `!ban list` | List all banned players and current enabled/disabled state |
| `!ban status` | Show the current enabled/disabled state of **both** the ban system and the whitelist |

---

### BEHAVIOUR

| Setting             | Type | Default | Description |
|---------------------|------|---------|-------------|
| `IgnoreBotMessages` | bool | `True` | When `True`, messages from Discord bot accounts are silently dropped. This prevents echo loops when other bots are active in the same channel. Set to `False` only if you intentionally want bot messages relayed into the game. |

---

### SERVER STATUS MESSAGES

These settings control whether and where online/offline notifications are posted
to Discord when the server starts or stops.

> **Scope note:** `ServerStatusMessagesEnabled` and `StatusChannelId` apply
> **only** to the online/offline notifications listed here.  Kick notifications
> (`BanKickDiscordMessage`, `WhitelistKickDiscordMessage`) always go to the
> main `ChannelId`, and command responses always return to the channel where
> the command was typed.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `ServerStatusMessagesEnabled` | bool | `True` | Master on/off switch for all online/offline notifications. Set to `False` to suppress both messages globally without clearing their text. |
| `StatusChannelId` | string | *(empty)* | Snowflake ID of a dedicated channel for status messages. When set, online/offline messages are posted here instead of the main `ChannelId`. Leave empty to use the main channel. |
| `ServerOnlineMessage` | string | `:green_circle: Server is now **online**!` | Posted when the server finishes loading and the bridge connects. Leave empty to disable. |
| `ServerOfflineMessage` | string | `:red_circle: Server is now **offline**.` | Posted when the server shuts down gracefully. Leave empty to disable. |

**Examples:**

```ini
; Custom emoji + text (note: %ServerName% is not substituted in status messages)
ServerOnlineMessage=🟢 Server is back online!
ServerOfflineMessage=🔴 Server has gone offline.

; Route status messages to a dedicated channel
StatusChannelId=111222333444555666777

; Disable offline notification only
ServerOfflineMessage=

; Disable all status notifications
ServerStatusMessagesEnabled=False
```

---

### PLAYER COUNT PRESENCE

Controls the **Discord bot status** (the "Now Playing" activity line shown on the
bot's profile).

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `ShowPlayerCountInPresence` | bool | `True` | When `True`, the bot's Discord status is refreshed periodically to show the current player count. Set to `False` to leave the bot status blank. |
| `PlayerCountPresenceFormat` | string | `Satisfactory with %PlayerCount% players` | Text shown in the bot's Discord presence. Use `%PlayerCount%` for the live player count and `%ServerName%` for the server name. Leave empty to show nothing. |
| `PlayerCountUpdateIntervalSeconds` | float | `60.0` | How often (in seconds) the presence is refreshed. **Minimum is 15 seconds.** Values below 15 are clamped to 15 to respect Discord's rate limits. |
| `PlayerCountActivityType` | int | `0` | Controls the activity verb shown before the presence text in Discord. See table below. |

**Activity type values:**

| Value | Discord verb |
|-------|-------------|
| `0`   | Playing |
| `2`   | Listening to |
| `3`   | Watching |
| `5`   | Competing in |

**Examples:**

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

## Troubleshooting

### The bridge does not start / no messages are relayed

1. Check that `BotToken` and `ChannelId` are both set in the config file.
2. Confirm the bot is **in your Discord server** and has **Send Messages** and
   **Read Message History** permissions in the target channel.
3. Make sure both **Privileged Gateway Intents** are enabled in the Discord
   Developer Portal (Server Members, Message Content).
4. Look in the server log (`FactoryGame.log`) for lines starting with `LogDiscordBridge`
   – they will contain details about any connection errors.

### Messages go one way only (game → Discord works, Discord → game doesn't)

- Verify **Message Content Intent** is enabled. Without it Discord does not send
  message content to bots, so the bridge cannot read Discord messages.

### The bot shows "offline" in Discord even while the server is running

- Discord caches presence state. Wait up to a minute or try restarting your Discord client.
- Make sure `ShowPlayerCountInPresence=True` and `PlayerCountPresenceFormat` is
  not empty.

### Messages from other bots are relayed into the game (echo loop)

- Set `IgnoreBotMessages=True` (this is the default). This drops messages from
  any Discord account that has the `bot` flag set.

### The config gets reset after a mod update

- This is expected behaviour for the primary config. Your credentials are saved
  automatically to `<ServerRoot>/FactoryGame/Saved/DiscordBridge/DiscordBridge.ini` each
  session and will be restored automatically. If you want to be safe, keep a
  separate copy of your `BotToken` and `ChannelId` somewhere secure.

### Whitelist commands are not recognised / players are not being kicked

1. Make sure `WhitelistCommandPrefix` is set (default is `!whitelist`) and not empty.
2. Confirm the whitelist is **enabled** (`!whitelist status` in the Discord channel).
3. If using `WhitelistRoleId`, verify the bot has the **Manage Roles** permission on your Discord server.
4. Players are only kicked on **join** – the whitelist is checked when a player connects, not while they are already in the game.

### Log verbosity

Add the following to your server's `DefaultEngine.ini` to increase log detail:

```ini
[Core.Log]
LogDiscordBridge=Verbose
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
