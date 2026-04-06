# DiscordBridge – Release Notes

## v1.0.4 – Player Join / Leave / Timeout Notifications

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – player join / leave / timeout notifications

A new group of settings lets the bridge post a Discord message whenever a player
joins, leaves, or times out from the server.  All three notification types are
**disabled by default**; set `PlayerEventsEnabled=True` to opt in.

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `PlayerEventsEnabled` | bool | `False` | Master switch for all player join/leave/timeout notifications. |
| `PlayerEventsChannelId` | string | *(empty)* | Snowflake ID of a dedicated channel for these notifications. Falls back to the main `ChannelId` when empty. |
| `PlayerJoinMessage` | string | `:green_circle: **%PlayerName%** joined the server.` | Posted when a player joins. Leave empty to disable. |
| `PlayerLeaveMessage` | string | `:red_circle: **%PlayerName%** left the server.` | Posted when a player leaves cleanly. Also used as a fallback when `PlayerTimeoutMessage` is empty. Leave empty to disable. |
| `PlayerTimeoutMessage` | string | `:yellow_circle: **%PlayerName%** timed out.` | Posted when a player's connection is lost without a clean disconnect. Leave empty to fall back to `PlayerLeaveMessage`. |

**Placeholders available in `PlayerJoinMessage`**

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | In-game display name of the joining player |
| `%SteamId%` | Steam64 ID (17-digit decimal). Empty string when the player did not connect through Steam. |
| `%EOSProductUserId%` | EOS Product User ID (32-char lowercase hex). Empty string when no EOS session is present. |

**Minimal example**

```ini
PlayerEventsEnabled=True
PlayerJoinMessage=:green_circle: **%PlayerName%** joined the server.
PlayerLeaveMessage=:red_circle: **%PlayerName%** left the server.
PlayerTimeoutMessage=:yellow_circle: **%PlayerName%** timed out.
```

**Routing notifications to a separate channel**

```ini
PlayerEventsEnabled=True
PlayerEventsChannelId=111222333444555666777
```

→ See [Player Notifications](Docs/04-PlayerNotifications.md)

---

## v1.0.3 – Config no longer overwritten by mod updates

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Improvement – primary config survives mod updates

`DefaultDiscordBridge.ini` is now **excluded from the packaged mod**, so installing
or updating DiscordBridge through SMM or a manual install will never overwrite a
server operator's configured file.  Previously the file was shipped inside the
package and would be overwritten on every mod update, which triggered the backup
restore mechanism on the next server start and caused credentials and other settings
to appear to "reset".

**What changes for server operators**

| Scenario | Before | After |
|----------|--------|-------|
| Fresh install | Mod ships a template file | Mod creates the template on first start |
| Mod update via SMM | Config overwritten → backup restores on next start | Config untouched |
| Manual mod update | Config overwritten → backup restores on next start | Config untouched |

The automatic backup (`Saved/DiscordBridge/DiscordBridge.ini`) is still written on every
server start as an extra safety net.  The backup restore path still works for any
edge case where the primary file is missing (e.g. a full mod-directory wipe).

**What changes for mod developers using Alpakit dev mode (`CopyToGameDirectory`)**

Alpakit dev mode deletes and recreates the entire mod directory on each deploy, so
`DefaultDiscordBridge.ini` will still be absent after each deploy and the backup
restore will run once on the next server start.  This is expected behaviour for the
dev workflow; the backup file in `Saved/DiscordBridge/` acts as the persisted copy of your
settings between deploys.

---

## v1.0.2 – Config backup restore corruption fix

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Bug fix – config corruption when restoring from backup

When the server detected that `DefaultDiscordBridge.ini` had been reset by a mod
update (empty `BotToken`/`ChannelId`) and restored settings from the
`Saved/DiscordBridge/DiscordBridge.ini` backup, any format string that contained a
`%ServerName%` placeholder could be silently corrupted.

**Root cause**: the backup was read via Unreal Engine's `FConfigFile::Read` +
`GetString`, which expands `%property%` references in values against other keys
in the same INI section.  Because `ServerName` **is** a key in the
`[DiscordBridge]` section, `%ServerName%` in a value like
`GameToDiscordFormat=**[%ServerName%] %PlayerName%**: %Message%` was expanded to
the current server name before being written back into the primary config by
`PatchLine`.  This permanently replaced the dynamic placeholder with the literal
server name, breaking all subsequent message formatting.

**Fix**: the backup is now read with raw line-by-line string parsing (no
`FConfigFile` processing), matching how the backup is written.  Format strings and
all other values are preserved **verbatim**, exactly as they were stored.

---

## v1.0.1 – Server status channel routing & toggle

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New settings

**`ServerStatusMessagesEnabled`** (bool, default `True`)
A master on/off switch for online/offline Discord notifications.  Set to `False`
to suppress both `ServerOnlineMessage` and `ServerOfflineMessage` globally without
having to clear their text values.

```ini
; Disable all server status notifications
ServerStatusMessagesEnabled=False
```

**`StatusChannelId`** (string, default empty)
The snowflake ID of a dedicated Discord channel for server status messages.
When set, `ServerOnlineMessage` and `ServerOfflineMessage` are posted to this
channel instead of the main bridged channel (`ChannelId`), keeping server
online/offline pings separate from regular chat.

```ini
; Route status messages to a dedicated announcements channel
StatusChannelId=111222333444555666777
```

---

### Bug fix – correct channel routing for kick notifications and command responses

`SendStatusMessageToDiscord()` was previously used for kick notifications and
whitelist command responses as well as server online/offline messages.  This
caused two regressions when `StatusChannelId` or `ServerStatusMessagesEnabled`
was configured:

1. **Wrong channel** — kick notifications and command responses were routed to
   `StatusChannelId` instead of the main `ChannelId` / originating channel.
2. **Silent drops** — setting `ServerStatusMessagesEnabled=False` accidentally
   suppressed kick notifications and command responses alongside the intended
   online/offline messages.

`SendStatusMessageToDiscord()` is now called exclusively for the two server
status events (online/offline).  Kick notifications always go to `ChannelId`
and whitelist command responses always reply to the channel where the
command was typed.

---

## v1.0.0 – Initial Release

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Overview

DiscordBridge is a server-only Satisfactory mod that creates a live two-way bridge
between your dedicated server's in-game chat and a Discord text channel via a Discord
bot token. Everything is configured through a single INI file with no external service
or dashboard required.

---

### Features

#### Two-way chat bridge
- In-game player messages are forwarded to Discord and Discord messages are relayed
  into the game in real time.
- Both directions are fully customisable via format strings with placeholder support
  (`%PlayerName%`, `%Username%`, `%Message%`, `%ServerName%`).
- Discord markdown (bold, italics, code blocks, etc.) is preserved when posting
  game messages to Discord.
- Bot messages from other Discord bots can be silently dropped (`IgnoreBotMessages`)
  to prevent echo loops.

#### Server status announcements
- Configurable messages are posted to the bridged channel when the server comes
  online (`ServerOnlineMessage`) and when it shuts down gracefully
  (`ServerOfflineMessage`).
- Either notification can be disabled independently by clearing its value.

#### Live player-count presence
- The bot's Discord "Now Playing" activity line displays the current player count
  in real time.
- The format, update interval (minimum 15 s), and activity verb (Playing /
  Listening to / Watching / Competing in) are all configurable.
- The presence can be disabled entirely with `ShowPlayerCountInPresence=False`.

#### Ban system

> **Note:** The built-in ban system was removed from DiscordBridge in a subsequent
> release.  There is no `!ban` command in the current version.  The whitelist
> (`!whitelist`) remains fully functional.

#### Whitelist
- Restrict which players can join the server and manage the list from Discord or
  in-game using `!whitelist` commands.
- Optional Discord role integration (`WhitelistRoleId`): members holding the
  whitelist role are automatically allowed through the whitelist by display-name
  matching, without needing a manual `!whitelist add` entry.
- Dedicated whitelist channel (`WhitelistChannelId`) mirrors in-game messages from
  whitelisted players and accepts Discord messages only from role holders.
- Configurable kick reason (`WhitelistKickReason`) and Discord kick notification
  (`WhitelistKickDiscordMessage`).

**Discord whitelist commands**

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist |
| `!whitelist off` | Disable the whitelist |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players |
| `!whitelist status` | Show enabled/disabled state of the whitelist |
| `!whitelist role add <discord_id>` | Grant the whitelist role to a Discord user |
| `!whitelist role remove <discord_id>` | Revoke the whitelist role from a Discord user |

**In-game whitelist commands** (`!whitelist on/off/add/remove/list/status`) are also
supported, excluding role management which is Discord-only.

#### Configuration
- Single primary config file (`DefaultDiscordBridge.ini`) covers all settings.
- Automatic full backup: **all** settings (connection, chat, presence, whitelist)
  are saved to `<ServerRoot>/FactoryGame/Saved/DiscordBridge/DiscordBridge.ini` on
  every startup.  If a mod update resets the primary config, the bridge restores
  every setting from the backup automatically on the next server start.
- All whitelist data is persisted in
  `<ServerRoot>/FactoryGame/Saved/ServerWhitelist.json` and survives server restarts.

---

### Requirements

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |
| SMLWebSocket | ≥ 1.0.0 |

> **Why is SMLWebSocket required?**
> DiscordBridge connects to Discord's gateway over a secure WebSocket connection (WSS).
> Unreal Engine's built-in WebSocket module is not available in Alpakit-packaged mods,
> so SMLWebSocket provides the custom RFC 6455 WebSocket client with SSL/OpenSSL
> support that the bridge relies on. Without it the bot cannot connect to Discord at
> all and the bridge will not start. When installing via **Satisfactory Mod Manager
> (SMM)** this dependency is installed automatically alongside DiscordBridge. For
> manual installs, copy the `SMLWebSocket/` folder into
> `<ServerRoot>/FactoryGame/Mods/` the same way you do for `DiscordBridge/`.

The Discord bot must have the following **Privileged Gateway Intents** enabled in the
Discord Developer Portal:
- Server Members Intent
- Message Content Intent

The bot also needs **Send Messages** and **Read Message History** permissions in the
target channel. The **Manage Roles** permission is required when using
`!ban role` or `!whitelist role` commands.

---

### Getting Started

See the [Getting Started guide](Docs/01-GettingStarted.md) and the rest of the
[documentation](Docs/README.md) for full setup instructions.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
