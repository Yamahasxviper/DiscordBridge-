# DiscordBridge – Connection Settings

← [Back to index](README.md)

These settings identify **which bot** to run and **which channel** to bridge.
They are required — the bridge will not start without `BotToken` and `ChannelId`.

---

## Config file

All connection settings live in the primary config file:

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

## What is a Discord ID?

Discord calls its unique numeric identifiers **Discord IDs**.  Every Discord object — channel, role, user, server — has an 18-19 digit Discord ID.  Whenever a setting in this documentation asks for a *Discord ID*, it just means that 18-19 digit number you copy from Discord.

To copy any Discord ID:
1. Enable **Developer Mode** in Discord (User Settings → Advanced → Developer Mode).
2. Right-click the channel, role, or user you need the ID for.
3. Choose **Copy Channel ID** / **Copy Role ID** / **Copy User ID**.

The result is a long number like `123456789012345678` — that is the Discord ID you paste into the config.

---

## Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `BotToken` | string | *(empty)* | Your Discord bot token. Treat this like a password – never share it. The bridge will not start if this is empty. |
| `ChannelId` | string | *(empty)* | The Discord channel ID to bridge. Must be a numeric ID (e.g. `123456789012345678`). Supports multiple channels — see below. |
| `ServerName` | string | *(empty)* | A display name for this server. Used as the `%ServerName%` placeholder in message formats and the player-count presence. Example: `My Satisfactory Server`. |

---

## Bridging multiple Discord channels

`ChannelId` accepts a **comma-separated list** of Discord channel IDs.  All
listed channels receive game-to-Discord messages and are watched for
Discord-to-game relaying and bot commands.

```ini
; Bridge two channels simultaneously
ChannelId=123456789012345678,987654321098765432
```

> **Tip:** Useful when you want the same game chat mirrored in a public channel
> and a private admin channel, or when running multiple Discord servers against
> one game server.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
