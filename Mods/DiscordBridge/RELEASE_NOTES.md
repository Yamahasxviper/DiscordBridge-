# DiscordBridge – Release Notes

## v1.1.0 – Slash Commands, Ticket System, BanDiscord Integration, Game Events & More

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

> **Breaking change:** All Discord commands now use **slash commands** (`/`).
> The `!` prefix commands have been completely removed. Old config fields
> such as `WhitelistCommandPrefix`, `InGameWhitelistCommandPrefix`,
> `PlayersCommandPrefix`, `StatsCommandPrefix`, and `PlayerStatsCommandPrefix`
> have been removed — the slash command names are fixed.

---

### New feature – all Discord commands are now slash commands

Every Discord command has been migrated to native Discord slash commands
(Application Commands). This provides auto-complete, validation, and a
consistent user experience.

**Discord Bridge slash commands:**
- `/stats` — Live server summary
- `/playerstats <player>` — Per-player session statistics
- `/players` — Current online player list
- `/whitelist on|off|add|remove|list|status|role` — Whitelist management (with new `apply`, `link`, `search`, `groups` subcommands)

**BanDiscord slash commands (42 subcommands):**
- `/ban add|temp|remove|removename|byname|check|reason|list|extend|duration|link|unlink|schedule|quick|bulk`
- `/warn add|list|clearall|clearone`
- `/mod kick|modban|mute|unmute|tempmute|tempunmute|mutecheck|mutelist|mutereason|announce|stafflist|staffchat`
- `/player history|note|notes|reason|playtime|reputation`
- `/appeal list|dismiss|approve|deny`
- `/admin say|poll|reloadconfig`

**Ticket slash commands (23 subcommands):**
- `/ticket panel|list|assign|claim|unclaim|transfer|priority|macro|macros|stats|report|tag|untag|tags|note|notes|escalate|remind|blacklist|unblacklist|blacklistlist|merge`

---

### New feature – enhanced ticket system

The ticket system has been significantly expanded with new management features:

| Command | Description |
|---------|-------------|
| `/ticket tag <tag>` | Add a tag to the current ticket |
| `/ticket untag <tag>` | Remove a tag from the current ticket |
| `/ticket tags` | List all tags on the current ticket |
| `/ticket note <text>` | Add a private staff note to the ticket |
| `/ticket notes` | List all staff notes on the ticket |
| `/ticket escalate` | Escalate the ticket to the escalation role/category |
| `/ticket remind <text>` | Set a follow-up reminder on the ticket |
| `/ticket blacklist <user>` | Blacklist a user from creating tickets |
| `/ticket unblacklist <user>` | Remove a user from the ticket blacklist |
| `/ticket blacklistlist` | Show all blacklisted users |
| `/ticket merge <ticket_id>` | Merge two tickets together |

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `TicketSlaWarningMinutes` | int | `0` | SLA warning threshold in minutes. `0` disables. |
| `TicketEscalationRoleId` | string | *(empty)* | Discord role pinged on `/ticket escalate`. |
| `TicketEscalationCategoryId` | string | *(empty)* | Category to move escalated tickets into. |
| `TicketTemplate` | array | *(empty)* | Ticket templates (`TypeSlug\|Field1\|...`). |
| `TicketAutoResponse` | array | *(empty)* | Auto-response on ticket creation (`TypeSlug\|message`). |

→ See [TicketSystem](Docs/09-TicketSystem.md)

---

### New feature – enhanced whitelist system

The whitelist now supports group/tier tagging, partial-name search, applications,
and Discord verification.

| Feature | Description |
|---------|-------------|
| Groups | Assign players to whitelist groups with `/whitelist add <name> group:GroupName` |
| Search | `/whitelist search <partial>` finds players by partial name |
| `/whitelist groups` | List all whitelist groups |
| `/whitelist apply` | Player-initiated whitelist application (when enabled) |
| `/whitelist link` | Link Discord account to in-game player |
| Verification | Discord-side verification workflow |
| Expiry warnings | Configurable advance notice before whitelist expiry |

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `WhitelistApplicationChannelId` | string | *(empty)* | Channel for whitelist applications. |
| `bWhitelistApplyEnabled` | bool | `False` | Enable `/whitelist apply` from Discord. |
| `WhitelistApprovedDmMessage` | string | *(empty)* | DM sent when application is approved. |
| `WhitelistExpiryWarningHours` | float | `24.0` | Hours before expiry to warn. |
| `bWhitelistVerificationEnabled` | bool | `False` | Enable Discord verification workflow. |
| `WhitelistVerificationChannelId` | string | *(empty)* | Channel for verification messages. |

→ See [Whitelist](Docs/05-Whitelist.md)

---

### New feature – full BanDiscord integration (42 slash commands)

When used alongside BanSystem and BanChatCommands, DiscordBridge now provides
a comprehensive set of Discord slash commands for server moderation:

- **Ban management** — `/ban add`, `/ban temp`, `/ban remove`, `/ban byname`, `/ban schedule`, `/ban quick`, `/ban bulk`, and more
- **Warning system** — `/warn add`, `/warn list`, `/warn clearall`, `/warn clearone`
- **Moderation** — `/mod kick`, `/mod modban`, `/mod mute/unmute/tempmute`, `/mod announce`, `/mod stafflist`, `/mod staffchat`
- **Player info** — `/player history`, `/player note/notes`, `/player playtime`, `/player reputation`
- **Appeals** — `/appeal list`, `/appeal approve`, `/appeal deny`, `/appeal dismiss`
- **Admin** — `/admin say`, `/admin poll`, `/admin reloadconfig`
- **Per-player moderation thread** — Ban/warn/kick actions are logged to per-player mod-log threads

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `AdminRoleId` | string | *(empty)* | Discord role ID for admin slash commands. |
| `ModeratorRoleId` | string | *(empty)* | Discord role ID for moderator slash commands. |
| `BanCommandChannelId` | string | *(empty)* | Channel for ban command output. |
| `ModerationLogChannelId` | string | *(empty)* | Channel for moderation log entries. |

---

### New feature – game phase and schematic unlock announcements

DiscordBridge now listens to `AFGGamePhaseManager` and `AFGSchematicManager` and
posts Discord messages when the server's game phase changes or a player purchases
a schematic (milestone).

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `PhaseEventsChannelId` | string | *(empty)* | Channel for game-phase-change announcements. Falls back to `ChannelId`. |
| `SchematicEventsChannelId` | string | *(empty)* | Channel for schematic-unlock announcements. Falls back to `PhaseEventsChannelId`, then `ChannelId`. |

→ See [Game Events](Docs/10-GameEvents.md)

---

### New feature – `/stats` and `/playerstats` Discord slash commands

Discord users can now type `/stats` in the bridged channel to get a live server
summary, or `/playerstats <PlayerName>` to retrieve per-player session counters.

> **Note:** These are now slash commands. The old `StatsCommandPrefix` and
> `PlayerStatsCommandPrefix` config fields have been removed.

→ See [Stats Commands](Docs/11-StatsCommands.md)

---

### New feature – reaction-based vote-kick

When enabled, a join notification embed is posted for each player. Discord users
react with 👎 to vote against the player; once the threshold is reached the player
is automatically kicked.

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `bEnableJoinReactionVoting` | bool | `False` | Master switch for reaction voting. |
| `VoteKickThreshold` | int | `0` | Minimum 👎 reactions required to kick. `0` disables kicking even when voting is on. |
| `VoteWindowMinutes` | int | `5` | How long (minutes) after the join embed the vote window remains open. |

→ See [Reaction Voting](Docs/12-ReactionVoting.md)

---

### New feature – AFK auto-kick

Players who have not moved or sent a message for a configurable number of minutes
are automatically kicked.

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `AfkKickMinutes` | int | `0` | AFK threshold in minutes. `0` disables. |
| `AfkKickReason` | string | `Kicked for inactivity (AFK).` | Kick message shown to the player. |

→ See [AFK Kick](Docs/13-AfkKick.md)

---

### New feature – admin-only join log channel

A separate private channel can receive detailed join notifications that include
the player's EOS PUID and IP address — hidden from the public bridged channel.

**New settings**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `PlayerJoinAdminChannelId` | string | *(empty)* | Channel ID for admin-only join details. Leave empty to disable. |
| `PlayerJoinAdminMessage` | string | *(see below)* | Format string for the admin log entry. |

Default `PlayerJoinAdminMessage`:
```
:shield: **%PlayerName%** joined | EOS: `%EOSProductUserId%` | IP: `%IpAddress%`
```

→ See [Player Notifications](Docs/04-PlayerNotifications.md)

---

### New feature – ban events channel

When DiscordBridge is used alongside BanSystem, ban/unban actions can now be
routed to a dedicated Discord channel.

**New setting**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `BanEventsChannelId` | string | *(empty)* | Channel for ban/unban notifications. Falls back to `ChannelId`. |

---

### New feature – player event embeds

Join/leave notifications can now be posted as rich Discord embeds instead of
plain text messages.

**New setting**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `bUseEmbedsForPlayerEvents` | bool | `False` | Post join/leave/timeout events as embed messages. Required for reaction voting. |

→ See [Player Notifications](Docs/04-PlayerNotifications.md)

---

### New feature – chat relay find-and-replace

In addition to the blocklist (which drops entire messages), you can now define
find-and-replace rules that substitute matched text with a replacement string
(default `***`) before the message is relayed to Discord.

**New setting**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `ChatRelayBlocklistReplacements` | array | *(empty)* | Ordered list of `(Pattern, Replacement)` substitution rules. |

**Example (DefaultDiscordBridge.ini):**
```ini
+ChatRelayBlocklistReplacements=(Pattern="badword",Replacement="***")
+ChatRelayBlocklistReplacements=(Pattern="slur",Replacement="[removed]")
```

→ See [Chat Bridge](Docs/03-ChatBridge.md)

---

### New feature – `/players` command

Typing `/players` in the bridged channel now returns the current online player list.

> **Note:** This is now a slash command. The old `PlayersCommandPrefix` config
> field has been removed.

---

### New feature – Discord invite URL broadcast

When `DiscordInviteUrl` is set, the URL is automatically announced in the game
chat so players can see it without being told manually.

**New setting**

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `DiscordInviteUrl` | string | *(empty)* | Discord invite URL. When non-empty it is periodically announced in-game. |

---

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
> release.  There is no `/ban` command in DiscordBridge itself.  Use the
> **BanSystem** + **BanChatCommands** companion mods for full ban management,
> with 42 Discord slash commands via the **BanDiscord** integration.
> The `/whitelist` commands remain fully functional.

#### Whitelist
- Restrict which players can join the server and manage the list from Discord or
  in-game using `/whitelist` slash commands.
- Optional Discord role integration (`WhitelistRoleId`): members holding the
  whitelist role are automatically allowed through the whitelist by display-name
  matching, without needing a manual `/whitelist add` entry.
- Dedicated whitelist channel (`WhitelistChannelId`) mirrors in-game messages from
  whitelisted players and accepts Discord messages only from role holders.
- Configurable kick reason (`WhitelistKickReason`) and Discord kick notification
  (`WhitelistKickDiscordMessage`).

**Discord whitelist slash commands**

| Command | Effect |
|---------|--------|
| `/whitelist on` | Enable the whitelist |
| `/whitelist off` | Disable the whitelist |
| `/whitelist add <name>` | Add a player by in-game name |
| `/whitelist remove <name>` | Remove a player by in-game name |
| `/whitelist list` | List all whitelisted players |
| `/whitelist status` | Show enabled/disabled state of the whitelist |
| `/whitelist role add <discord_id>` | Grant the whitelist role to a Discord user |
| `/whitelist role remove <discord_id>` | Revoke the whitelist role from a Discord user |
| `/whitelist apply` | Player-initiated whitelist application |
| `/whitelist link` | Link Discord account to in-game player |
| `/whitelist search <partial>` | Search whitelist by partial name |
| `/whitelist groups` | List whitelist groups |

**In-game whitelist commands** (`/ingamewhitelist on/off/add/remove/list/status`) are also
supported via the SML chat command system.

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
`/whitelist role` commands.

---

### Getting Started

See the [Getting Started guide](Docs/01-GettingStarted.md) and the rest of the
[documentation](Docs/README.md) for full setup instructions.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
