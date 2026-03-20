# DiscordBridge – Server Status Messages & Behaviour

← [Back to index](README.md)

---

## Config file

All server status and behaviour settings live in the primary config file:

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

## Server Status Messages

These messages are posted to Discord when the server starts or stops.

> **Scope of these settings**
> `ServerStatusMessagesEnabled` and `StatusChannelId` apply **only** to the
> online/offline notifications below.  Kick notifications (`BanKickDiscordMessage`,
> `WhitelistKickDiscordMessage`) always go to the main `ChannelId`, and command
> responses always go back to the channel where the command was typed.

### `ServerStatusMessagesEnabled`

Master on/off switch for online/offline notifications.
Set to `False` to suppress both `ServerOnlineMessage` and `ServerOfflineMessage`
globally without having to clear their text.

**Default:** `True`

| Value | Effect |
|-------|--------|
| `True` | Online/offline messages are posted normally |
| `False` | No online/offline messages are ever posted, regardless of the message text |

**Example — disable all status notifications:**
```ini
ServerStatusMessagesEnabled=False
```

---

### `StatusChannelId`

The Discord channel ID of the dedicated channel for online/offline status
messages.  Leave **empty** to post status messages to the main bridged channel
(`ChannelId`).

**Default:** *(empty — status messages go to `ChannelId`)*

When set, `ServerOnlineMessage` and `ServerOfflineMessage` are posted to this
channel instead of the main bridged channel, keeping server-status updates
separate from regular chat.

**How to get the channel ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode),
then right-click the channel and choose **Copy Channel ID**.

**Example:**
```ini
StatusChannelId=111222333444555666777
```

> **Tip:** Pair `StatusChannelId` with a read-only announcement channel so
> players can see server-up/down pings without cluttering the main chat.

---

### `ServerStatusNotifyRoleId`

The Discord role ID to @mention in every server online/offline
status message.  When set, the bot prepends `@<Role>` to the
`ServerOnlineMessage` / `ServerOfflineMessage` text so all members of that role
receive a Discord ping when the server starts or stops.
Leave **empty** to post status messages without any mention.

**Default:** *(empty)*

| Value | Effect |
|-------|--------|
| *(empty)* | No mention is added to status messages |
| A Discord role ID | `@RoleName` is prepended to each status message |
| `everyone` | `@everyone` is prepended |
| `here` | `@here` is prepended |

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode),
then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

**Example:**
```ini
; Ping the "Server News" role whenever the server comes online or goes offline
ServerStatusNotifyRoleId=123456789012345678

; Ping @everyone
ServerStatusNotifyRoleId=everyone
```

---

### `ServerOnlineMessage`

Posted to the status channel (or main channel if `StatusChannelId` is empty)
when the dedicated server finishes loading and the bridge connects.
Leave **empty** to disable this notification.

**Default:** `:green_circle: Server is now **online**!`

---

### `ServerOfflineMessage`

Posted when the server shuts down gracefully.
Leave **empty** to disable this notification.

**Default:** `:red_circle: Server is now **offline**.`

---

**Examples:**

```ini
; Custom emoji + text (note: %ServerName% is not substituted in status messages)
ServerOnlineMessage=🟢 Server is back online!
ServerOfflineMessage=🔴 Server has gone offline.

; Route status messages to a dedicated channel
StatusChannelId=111222333444555666777
ServerOnlineMessage=:white_check_mark: Server online
ServerOfflineMessage=:x: Server offline

; Disable offline notification entirely
ServerOfflineMessage=

; Disable all status notifications
ServerStatusMessagesEnabled=False
```

---

## Behaviour

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `IgnoreBotMessages` | bool | `True` | When `True`, messages from Discord bot accounts are silently dropped. This prevents echo loops when other bots are active in the same channel. Set to `False` only if you intentionally want bot messages relayed into the game. |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
