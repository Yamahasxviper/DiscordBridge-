# DiscordBridge – Bot Info Channel & `/help` Command

← [Back to index](README.md)

---

## Overview

The **Bot Info Channel** feature lets the bot automatically post a comprehensive
feature and command reference to a dedicated Discord channel every time the server
starts.  Discord members can also type `/help` (or `/commands`) at any time to
retrieve the same reference on demand.

The reference is posted as two rich Discord embeds:

| Embed | Contents |
|-------|----------|
| 📖 **DiscordBridge — Features & Commands** | Chat bridge, server/player commands, whitelist commands, `/help` itself |
| 🛡️ **Moderation Commands (BanSystem)** | Admin ban/history/warn/note commands; moderator kick/mute/announce commands |

The second embed only appears when **BanSystem** and **BanChatCommands** are
installed alongside DiscordBridge.

---

## `BotInfoChannelId`

Snowflake ID of a dedicated Discord channel where the bot posts the full
feature/command reference **once per server start** (on the first Gateway
connection after the server launches).

**Default:** *(empty — automatic post is disabled)*

When **empty**, the automatic post on server start is skipped.  The `/help`
command still works at any time.

### Recommended setup

1. Create a read-only channel in your Discord server (e.g. **#bot-commands** or
   **#how-to-use**) so the reference stays pinned and uncluttered by chat.
2. Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode).
3. Right-click the channel and choose **Copy Channel ID**.
4. Paste the ID into your config and restart the server.

```ini
# DefaultDiscordBridge.ini

# Snowflake ID of the channel where the full command reference is posted on startup.
# Leave empty to disable the automatic post.
BotInfoChannelId=111222333444555666777
```

> **Tip:** Set the channel to read-only for regular members so only the bot can
> post there.  The reference stays visible and easy to find without anyone
> accidentally sending messages on top of it.

---

## `/help` and `/commands` — on-demand reference

Any Discord user can type either of these slash commands at any time to trigger the
same pair of embeds on demand:

```
/help
/commands
```

The response is always posted to the channel where the command was typed.

> **Note:** These commands are fixed — they cannot be renamed or disabled via
> config.  To suppress the automatic startup post, simply leave `BotInfoChannelId`
> empty.

---

## What the embeds show

### Embed 1 — General commands

| Field | Commands covered |
|-------|-----------------|
| 💬 **Chat Bridge** | Description of how two-way relay works |
| 🖥️ **Server & Player Commands** | `/players`, `/stats`, `/playerstats` |
| 🔒 **Whitelist Commands** | `/whitelist on/off/add/remove/list/status/role/apply/link/search/groups` |
| ℹ️ **Help** | How to re-trigger the reference with `/help` or `/commands` |

### Embed 2 — Moderation commands *(BanSystem)*

| Field | Commands covered |
|-------|-----------------|
| ⚔️ **Admin — Ban Management** | `/ban add/temp/remove/removename/byname/check/reason/list/extend/duration/schedule/quick/bulk` |
| 📋 **Admin — History, Warnings & Notes** | `/player history`, `/warn add/list/clearall/clearone`, `/player note/notes/reason` |
| ⚙️ **Admin — Server Links & Config** | `/ban link/unlink`, `/admin reloadconfig` |
| 👮 **Moderator Commands** | `/mod kick/modban/mute/unmute/tempmute/tempunmute/mutecheck/mutelist/mutereason/announce/stafflist/staffchat` |

Admin and moderator commands require the respective Discord role IDs to be
configured in `DefaultBanBridge.ini` (`AdminRoleId` / `ModeratorRoleId`).  See
the [BanSystem documentation](../../BanSystem/Docs/README.md) for details.

---

## Behaviour details

| Detail | Description |
|--------|-------------|
| **Posted once per restart** | The embed is only posted on the very first Gateway connection after the server process starts.  Gateway reconnects (network drops, Discord maintenance) do **not** trigger a repeat post. |
| **Fallback when `BotInfoChannelId` is empty** | The automatic post is skipped entirely.  `/help` is the only way to retrieve the reference. |
| **Dynamic content** | Slash command names are shown in the embed as registered with Discord. |
| **Whitelist section** | The whitelist command block is always shown since `/whitelist` is a registered slash command. |

---

## Configuration summary

| Key | Type | Default | Description |
|-----|------|---------|-------------|
| `BotInfoChannelId` | string | *(empty)* | Snowflake ID of the channel to post the feature/command reference to on server start. Leave empty to disable the automatic post. |

---

## Full example

```ini
# [DiscordBridge] section of DefaultDiscordBridge.ini

# -- BOT INFO / HELP CHANNEL -------------------------------------------------
# Set to the channel ID of a read-only #bot-commands channel.
# Leave empty to disable the automatic startup post.
BotInfoChannelId=111222333444555666777
```

With this config, every time the server starts the bot will post the full
feature and command reference into the `#bot-commands` channel.  Members can
also type `/help` to retrieve the reference at
any time.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
