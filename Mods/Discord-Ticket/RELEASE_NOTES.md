# DiscordBridge – Release Notes

## v1.11.0 – Standalone Kick Command

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – `!kick` command (kick without banning)

DiscordBridge now has a dedicated **kick command** that removes a player from
the server without adding them to the permanent ban list.  The player can
reconnect immediately — use this for temporary disciplinary actions when a full
ban is not warranted.

#### Discord command

```
!kick <PlayerName>
!kick <PlayerName> <custom reason>
```

#### In-game chat command

```
!kick <PlayerName>
!kick <PlayerName> <custom reason>
```

#### Configuration (`DefaultBan.ini`)

| Setting | Default | Description |
|---------|---------|-------------|
| `KickCommandRoleId` | *(empty)* | Discord role ID whose members may use `!kick`. Leave empty to disable for everyone. |
| `KickCommandPrefix` | `!kick` | Command prefix. |
| `KickCommandsEnabled` | `True` | Master on/off toggle. |
| `KickDiscordMessage` | `:boot: **%PlayerName%** was kicked by an admin.` | Discord notification. Supports `%PlayerName%` and `%Reason%`. |
| `KickReason` | *(empty → built-in default)* | Reason shown in-game to the kicked player. |
| `InGameKickCommandPrefix` | `!kick` | In-game chat prefix. |

#### Comparison with `!ban add`

| | `!kick` | `!ban add` |
|-|---------|-----------|
| Player can reconnect | ✅ immediately | ❌ blocked until unbanned |
| Permanent ban record | ❌ none | ✅ saved to `ServerBanlist.json` |
| Platform ID also banned | ❌ no | ✅ auto-banned if online |

> **Tip:** Set `KickCommandRoleId` to the same role as `BanCommandRoleId` to
> give your existing ban admins kick access without adding a new role.

---

## v1.10.0 – EOSIntegration Removal, GameplayEvents & ReliableMessaging

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Change – EOSIntegration dependency removed

DiscordBridge no longer depends on the separate **EOSIntegration** plugin.
All EOS platform detection and player platform-ID resolution has been moved
directly into DiscordBridge using the CSS-native **OnlineIntegration** plugin
(`Plugins/Online/OnlineIntegration`) which ships with the dedicated server.

> **No action needed for existing servers.** Simply remove the `EOSIntegration/`
> mod folder from `<ServerRoot>/FactoryGame/Mods/` if it is present — it is no
> longer used.  SMM will handle this automatically on reinstall.

---

### New feature – GameplayEvents integration

DiscordBridge now fires **Gameplay Events** (using the CSS `GameplayEvents` plugin)
for key bridge lifecycle moments.  Other mods can subscribe to these events without
taking a direct code dependency on DiscordBridge:

| Event tag | Fired when |
|-----------|-----------|
| `DiscordBridge.Connected` | Bot successfully connects to Discord |
| `DiscordBridge.Disconnected` | Bot disconnects from Discord |
| `DiscordBridge.Player.Joined` | A player joins the server |
| `DiscordBridge.Player.Left` | A player leaves the server |
| `DiscordBridge.Message.FromDiscord` | A Discord message is relayed into the game |

Other mods can also post Discord messages by dispatching the
`DiscordBridge.Message.ToDiscord` event.

---

### New feature – ReliableMessaging relay channel

DiscordBridge registers a **ReliableMessaging** channel
(`EDiscordRelayChannel::ForwardToDiscord`, value `200`) so that client-side mods can
relay UTF-8 payloads to Discord via `GameToDiscordFormat` without needing a server-side
reference to DiscordBridge.

---

### New placeholder – `%Platform%` in join/leave messages

`PlayerJoinMessage` and `PlayerLeaveMessage` now support a `%Platform%` placeholder
that expands to the player's platform display name (e.g. `Steam` or `Epic Games Store`).

```ini
; Example: show which platform each player joined from
PlayerJoinMessage=:arrow_right: **%PlayerName%** joined via %Platform%.
PlayerLeaveMessage=:arrow_left: **%PlayerName%** (%Platform%) left the server.
```

---

## v1.9.0 – Cross-Platform Ban ID Linking

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – Automatic cross-platform ban ID linking (Steam64 ↔ EOS PUID)

Satisfactory is cross-platform: both **Steam** and **Epic Games** players can join the
same server.  On an EOS-mode server (`DefaultPlatformService=EOS`) every player —
whether they launch via Steam or the Epic Games Launcher — is assigned an
**EOS Product User ID (PUID)**.

Previously, if an admin banned a player using their **Steam64 ID** and the player
reconnected in a session where EOS was fully initialised (skipping the brief Steam64
fast-path phase), the ban would not be enforced because the player's PUID was not in
the ban list.  Conversely, a player banned by EOS PUID but whose Steam64 ID had
not been linked required the 60-second followup timer to fire on every join to enforce
the ban.

The EOS PUID followup check now performs **automatic cross-platform ID linking**:
when a Steam player's Steam64 ↔ EOS PUID mapping is discovered (i.e. the EOS PUID
resolves while the followup timer is running), the system:

1. Checks **both** identifiers against the ban list.
2. If the **Steam64 ID** is banned but the EOS PUID is not — automatically adds the
   EOS PUID to the ban list and notifies the ban channel, so future joins that
   skip the Steam64 phase are caught at the primary check.
3. If the **EOS PUID** is banned but the Steam64 ID is not — automatically adds the
   Steam64 to the ban list, so future joins during the Steam64 fast-path phase
   are caught immediately without needing the 60-second followup.
4. Kicks the player if **either** identifier is on the ban list.

Once the server has observed a player's Steam64 ↔ EOS PUID pair, a ban added via
**either** identifier will be enforced on all future logins automatically — no manual
`!ban id add` step for the second ID is needed.

The ban channel receives a `:link: Cross-platform ban link` notification when a new ID
is added automatically so admins have a full audit trail.

> **Note:** Cross-platform linking requires at least one join where the player goes
> through the Steam64 fast-path phase so the server can observe both IDs.  If a Steam
> player always connects after EOS has fully warmed up (presenting only an EOS PUID),
> use `!ban epic add <eos_puid>` or `!ban id add <eos_puid>` to ban by EOS PUID
> directly.  EOS PUID bans cover **both** Steam and Epic players on EOS-mode servers.

---

## v1.8.0 – Login-Reject Notifications & Platform-Aware Ban Status

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – Discord notification when Login hook rejects a banned player

A new config key **`BanLoginRejectDiscordMessage`** (in `DefaultBan.ini`) controls a
notification that fires when a banned player is blocked at the very first connection
handshake — before they have fully joined the server.

This is different from `BanKickDiscordMessage`, which fires after the player has
joined and is then kicked:

| Event | Config key | Player name available? |
|-------|-----------|------------------------|
| Login-time rejection (new) | `BanLoginRejectDiscordMessage` | ✗ — player hasn't joined yet |
| Post-join kick | `BanKickDiscordMessage` | ✓ |

**Default message:**
```
:no_entry: A banned player (%PlatformType% `%PlatformId%`) tried to connect and was rejected.
```

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlatformId%` | The banned Steam64 ID or EOS PUID |
| `%PlatformType%` | `Steam` or `EOS PUID` depending on the ID format |

The notification is **rate-limited to once per platform ID per 60 seconds** to prevent
Discord spam when a banned player retries the connection rapidly.

Leave the value **empty** to disable login-reject notifications:

```ini
BanLoginRejectDiscordMessage=
```

---

### Fix – `!ban status` now shows the correct platform label

`!ban status` (and the in-game equivalent) previously always displayed "EOS platform"
even when the server runs with `DefaultPlatformService=Steam`.  The status line now
correctly reflects the runtime platform:

| Server mode | `!ban status` output |
|-------------|----------------------|
| EOS (`DefaultPlatformService=EOS`) | `:satellite: Platform (EOS): active – N platform IDs banned (Steam & Epic covered)` |
| Steam (`DefaultPlatformService=Steam`) | `:satellite: Platform (Steam): active – N Steam64 IDs banned` |
| Platform unavailable | `:warning: Platform IDs: enforcement inactive – platform unavailable` |

---

## v1.7.0 – Auto-ban by Platform ID & Dedicated Log File

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – `!ban add <name>` auto-bans by platform ID

When you run `!ban add <PlayerName>` and the named player is **currently connected** to
the server, the bot now automatically bans their platform ID (Steam64 ID or EOS Product
User ID) in the same command.  Admins no longer need the separate
`!ban id lookup <name>` → `!ban id add <id>` two-step flow in the common case.

**Response examples:**

```
:hammer: PlayerName has been banned from the server. They have been kicked.
:satellite: Platform ID 0002abc123def456 (Steam) also banned automatically.
```

```
:hammer: PlayerName has been added to the ban list.
:information_source: Player is not currently connected — banned by name only.
  Use !ban id add <id> to also ban their platform ID.
```

```
:hammer: PlayerName has been banned from the server.
:warning: Player is connected (Steam) but their EOS PUID is not yet available.
  Use !ban id lookup PlayerName then !ban id add <id> once it resolves.
```

The in-game `!ban add <name>` command has the same behaviour.

---

### New feature – dedicated bot log file

Every server session now writes a dedicated log file at:

```
<ServerRoot>/FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log
```

The file captures all log output from the DiscordBridge, SMLWebSocket, BanManager, and
WhitelistManager categories (everything tagged `LogDiscordBridge`, `LogSMLWebSocket`,
`LogBanManager`, and `LogWhitelistManager`) in one place.  Each line is timestamped in
UTC to the millisecond.  The file is opened in **append mode** so multiple server
restarts accumulate in a single file — session boundaries are clearly marked:

```
[2026.03.11-12.00.00 UTC] ===== DiscordBot session started =====
[2026.03.11-12.00.01.234 UTC][Log][LogDiscordBridge] DiscordBridge: ...
[2026.03.11-12.30.00 UTC] ===== DiscordBot session ended =====
```

Use `tail -f` or any log-viewing tool to monitor the file while the server is running.

---

## v1.6.0 – EOS Integration & Enhanced Ban Commands

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New dependency – EOSIntegration plugin

DiscordBridge now depends on a standalone **EOSIntegration** plugin that ships
alongside it.  The plugin centralises all Epic Online Services (EOS) platform
detection and safe platform-ID resolution in one place so that other mods can
query EOS state without reimplementing the `mIsOnline` / local-user guards
required in CSS UE 5.3.

> **No action needed for existing servers.**  SMM installs the plugin
> automatically alongside DiscordBridge.  Manual installs should copy the
> `EOSIntegration/` folder into the same `Mods/` directory.

---

### New ban command – `!ban players`

```
!ban players
```

Lists every currently connected player together with their platform name
(Steam / Epic) and their **EOS Product User ID (PUID)** in one Discord message.
Each entry also shows `:hammer: BANNED` flags when the player's name or PUID is
already on the ban list.  This is the quickest way to discover a player's PUID
before issuing a `!ban id add` command.

---

### New ban command – `!ban check <PlayerName>`

```
!ban check <PlayerName>
```

Checks whether a player is banned **by name**, and—if they are currently
connected—also checks their PUID.  Responds with:
- `:hammer: BANNED` by name and/or platform ID, or
- `:white_check_mark: Not banned.`

If the player is online, their platform name and PUID are included in the reply.

---

### New ban command – `!ban id lookup <PlayerName>`

```
!ban id lookup <PlayerName>
```

Looks up the EOS Product User ID of a **currently connected** player by
in-game name.  Returns the PUID together with a ready-to-copy
`!ban id add <puid>` command string.  Works for both Steam and Epic Games
players.

---

### New server info command – `!server eos`

```
!server eos
```

Runs a full EOS platform diagnostic through the EOSIntegration subsystem and
posts the result to Discord.  The report includes:

- Whether EOS credentials are present in the engine config
- Whether the EOS session interface initialised at startup
- The number of platform IDs currently in the ban list
- Fix instructions when EOS is unavailable

No role is required to run this command (it is read-only and contains no
secrets).

---

### Updated `!ban status`

The existing `!ban status` command now also shows the **EOS platform state**
so admins can see at a glance whether platform-ID enforcement is active:

```
Server access control (each system is independent):
:hammer: Ban system: enabled — 3 names banned
:no_entry_sign: Whitelist: disabled
:satellite: EOS platform: active — 2 platform IDs banned (Steam & Epic covered)
```

---

### Updated `!server status`

`!server status` now appends a `:warning:` line when the EOS platform is
unavailable, so operators know early that `!ban id` enforcement is inactive.

---

## v1.5.0 – Split Config Files & Per-Feature Backups

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Breaking change – config files reorganised

Settings for the whitelist, ban system, and ticket system have been moved out of
`DefaultDiscordBridge.ini` into three dedicated files that each ship with the mod:

| File | Settings |
|------|----------|
| `DefaultDiscordBridge.ini` | Connection, chat, server status, presence, server info, server control |
| `DefaultWhitelist.ini` | All `Whitelist*` and `InGameWhitelistCommandPrefix` settings |
| `DefaultBan.ini` | All `Ban*` and `InGameBanCommandPrefix` settings |
| `DefaultTickets.ini` | All `Ticket*` settings |

All four files live in the same `Config/` folder:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/
```

**Existing installations are migrated automatically** on the first server start after
the update.  The mod reads your current settings from the combined
`Saved/Config/DiscordBridge.ini` backup and writes them into the new split files, so
no reconfiguration is required.

---

### New feature – per-feature backup files

The single combined backup (`Saved/Config/DiscordBridge.ini`) has been split into four
separate backup files, one per config file:

| Backup file | Mirrors |
|-------------|---------|
| `Saved/Config/DiscordBridge.ini` | `DefaultDiscordBridge.ini` |
| `Saved/Config/Whitelist.ini` | `DefaultWhitelist.ini` |
| `Saved/Config/Ban.ini` | `DefaultBan.ini` |
| `Saved/Config/Tickets.ini` | `DefaultTickets.ini` |

All four backup files are written on **every server start**.  If a mod update resets
any primary config file, the mod restores the affected settings from the matching
backup automatically.

---

### Upgrade notes

- No manual action is required for existing servers.
- If you previously kept custom copies of the combined
  `DefaultDiscordBridge.ini`, copy the relevant sections into the new split files
  after the first start (by which point the mod will have already created them from
  your backup).
- The old `DefaultDiscordBridgeBan.ini` and `DefaultDiscordBridgeWhitelist.ini`
  filenames (from earlier documentation) are superseded by `DefaultBan.ini` and
  `DefaultWhitelist.ini` respectively.

---

## v1.4.0 – Ticket / Support System

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – button-based ticket panel

Any Discord member can now open a support ticket by clicking a button on a panel
posted in a dedicated channel — no commands to remember.  Each click creates a
**private Discord channel** visible only to the member who clicked and the
admin/support role (who receive an @mention ping automatically).  Either party
can close the ticket by clicking the **Close Ticket** button, which deletes the
private channel.

**Built-in ticket types:**

| Button | Purpose |
|--------|---------|
| Whitelist Request | Member requests to be added to the server whitelist |
| Help / Support | Member submits a general help or support question |
| Report a Player | Member reports another player to the admins |

**Custom ticket reasons** can be added to `DefaultTickets.ini` — each
`TicketReason=Label|Description` line creates an additional button on the panel.

**New config file** – `DefaultTickets.ini` (in the same `Config/` folder):

```ini
; Discord channel ID for admin ticket notifications (optional, comma-separated)
TicketChannelId=

; Per-built-in-type toggles (hide the button and disable that type)
TicketWhitelistEnabled=True
TicketHelpEnabled=True
TicketReportEnabled=True

; Discord role ID: @mentioned on new tickets, given access to ticket channels
TicketNotifyRoleId=

; Channel where the button panel is posted (run !admin ticket-panel to post it)
TicketPanelChannelId=

; Optional Discord category ID to group all ticket channels together
TicketCategoryId=

; Custom ticket reasons (one per line, Label|Description format)
; TicketReason=Bug Report|Report a bug or technical issue with the server
; TicketReason=Appeal|Appeal a ban or other moderation action
```

**To post the ticket panel**, run `!admin ticket-panel` in any channel where you
hold the `ServerControlCommandRoleId` role.  The bot posts the button panel to
`TicketPanelChannelId` (or the main channel when that setting is empty).

**Required bot permissions** for the ticket panel:
- **Manage Channels** — to create and delete private ticket channels.

All ticket settings are written to `Saved/Config/Tickets.ini` as a backup on
every server start and restored automatically after a mod update.

---

## v1.3.0 – Server Control Commands & Multi-Channel Support

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – admin server control commands (`!admin`)

Authorised Discord members can now stop or restart the dedicated server directly
from the bridged channel.  Commands are deny-by-default and require
`ServerControlCommandRoleId` to be set.

| Command | Exit code | Effect |
|---------|-----------|--------|
| `!admin start` | *(no exit)* | Replies that the server is already online |
| `!admin stop` | `0` | Gracefully shuts down the server (supervisor does **not** restart on exit code 0) |
| `!admin restart` | `1` | Gracefully shuts down the server (supervisor restarts on exit code 1) |

**New config keys:**

```ini
; Prefix for server control commands. Default: !admin
ServerControlCommandPrefix=!admin

; Discord ID of the role allowed to run !admin commands. Leave empty to
; disable for everyone (deny-by-default).
ServerControlCommandRoleId=
```

> For `!admin stop` to actually prevent a restart, the process supervisor
> must use `Restart=on-failure` (systemd) or `--restart on-failure` (Docker),
> **not** `Restart=always` / `--restart always`.

---

### New feature – multiple channel IDs (`ChannelId`, `StatusChannelId`, …)

`ChannelId`, `StatusChannelId`, `BanChannelId`, and `WhitelistChannelId` all now accept **comma-separated lists** of Discord IDs,
allowing messages to be broadcast to or monitored from multiple channels
simultaneously.

```ini
; Bridge two channels at once
ChannelId=123456789012345678,987654321098765432

; Post status messages to two announcement channels
StatusChannelId=111222333444555666777,888999000111222333444
```

---

## v1.2.0 – @mention notify for server status messages

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – @mention role on server online/offline notifications

Server operators can now configure a Discord role (or @everyone / @here) to be
mentioned whenever the server comes online or goes offline.  The mention is
prepended to the existing `ServerOnlineMessage` / `ServerOfflineMessage` text so
subscribed members receive a Discord ping.

**New config key:**

```ini
; Discord role ID to @mention in server online/offline messages.
; Special values: "everyone" to mention @everyone, "here" to mention @here.
; Leave empty to post status messages without any mention (default).
; Example: ServerStatusNotifyRoleId=123456789012345678
ServerStatusNotifyRoleId=
```

Set this to A Discord role ID obtained from Discord (Developer Mode → right-click
role → Copy Role ID).  Use `everyone` or `here` for the built-in Discord mentions.

Existing configs are upgraded automatically: `ServerStatusNotifyRoleId=` is
appended to `DefaultDiscordBridge.ini` on the next server start if the key is
absent.

---

## v1.1.0 – Server info commands (`!server`)

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### New feature – server-information Discord commands

Any member of the bridged Discord channel can now query the game server directly
without requiring an admin role.

| Command | Effect |
|---------|--------|
| `!server players` | List the in-game names of all currently online players |
| `!server online` | Alias for `!server players` |
| `!server status` | Show the server name, online/offline state, and player count |
| `!server help` | List all available bot commands |

**New config key:**

```ini
; Prefix for server-info commands. Set to empty to disable. Default: !server
ServerInfoCommandPrefix=!server
```

Existing configs are upgraded automatically: `ServerInfoCommandPrefix=!server` is
appended to `DefaultDiscordBridge.ini` on the next server start if the key is absent.

---

## v1.0.2 – Config backup restore corruption fix

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Bug fix – config corruption when restoring from backup

When the server detected that `DefaultDiscordBridge.ini` had been reset by a mod
update (empty `BotToken`/`ChannelId`) and restored settings from the
`Saved/Config/DiscordBridge.ini` backup, any format string that contained a
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
The Discord channel ID of a dedicated channel for server status messages.
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
and whitelist/ban command responses always reply to the channel where the
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
- Manage a server-side ban list directly from the bridged Discord channel or from
  inside the game using `!ban` commands.
- Ban players by **in-game name** or by **platform unique ID** (Steam64 ID or
  EOS Product User ID) — platform ID bans are more robust because players cannot
  change their Steam or Epic Games account ID even if they change their display name.
- Banned players are kicked automatically when they attempt to join the server.
- Dedicated Discord channel support (`BanChannelId`) provides an isolated admin
  audit log for ban events.
- Role-gated commands (`BanCommandRoleId`) restrict who can issue `!ban` commands
  from Discord.  All `!ban` and `!ban id` commands require this role.
- Role management commands (`!ban role add/remove <discord_id>`) let existing ban
  admins promote or demote other Discord members without requiring Discord server
  admin access.
- Configurable kick message shown to the player in-game (`BanKickReason`) and
  Discord notification posted when a banned player is kicked (`BanKickDiscordMessage`).
- The ban command interface can be disabled independently of ban enforcement
  (`BanCommandsEnabled=False` keeps bans active without accepting new commands).

**Discord ban commands** (all require `BanCommandRoleId`)

| Command | Effect |
|---------|--------|
| `!ban on` | Enable the ban system |
| `!ban off` | Disable the ban system |
| `!ban add <name>` | Ban a player by in-game name |
| `!ban remove <name>` | Unban a player by in-game name |
| `!ban id add <platform_id>` | Ban by Steam64 ID or EOS Product User ID |
| `!ban id remove <platform_id>` | Unban by Steam64 ID or EOS Product User ID |
| `!ban id list` | List all banned platform IDs |
| `!ban list` | List all banned player names |
| `!ban status` | Show enabled/disabled state of ban system and whitelist |
| `!ban role add <discord_id>` | Grant the ban admin role to a Discord user |
| `!ban role remove <discord_id>` | Revoke the ban admin role from a Discord user |

**In-game ban commands** (`!ban on/off/add/remove/id add/id remove/id list/list/status`)
are also supported, excluding role management which is Discord-only.  The in-game
prefix can be configured independently of the Discord prefix via `InGameBanCommandPrefix`.

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
| `!whitelist status` | Show enabled/disabled state of whitelist and ban system |
| `!whitelist role add <discord_id>` | Grant the whitelist role to a Discord user |
| `!whitelist role remove <discord_id>` | Revoke the whitelist role from a Discord user |

**In-game whitelist commands** (`!whitelist on/off/add/remove/list/status`) are also
supported, excluding role management which is Discord-only.  The in-game prefix can
be configured independently of the Discord prefix via `InGameWhitelistCommandPrefix`.

#### Server info commands
- Any Discord member can query the server with `!server players`, `!server status`,
  `!server online`, and `!server help` — no admin role required.
- The command prefix (`ServerInfoCommandPrefix`) is configurable and can be
  disabled by setting it to an empty string.

#### Ticket / support system
- Members open support tickets by clicking a button on a panel posted in a dedicated Discord channel — no commands to type.
- Each button click creates a **private Discord channel** visible only to the member and the admin/support role (`TicketNotifyRoleId`), who receive an @mention ping automatically.
- Three built-in ticket types: Whitelist Request, Help / Support, and Report a Player; each can be toggled independently (`TicketWhitelistEnabled`, `TicketHelpEnabled`, `TicketReportEnabled`).
- Unlimited custom ticket reasons can be added via `TicketReason=Label|Description` lines in `DefaultTickets.ini`.
- Post the button panel by running `!admin ticket-panel` (requires `ServerControlCommandRoleId`).

#### Server control commands
- Authorised Discord members can stop (`!admin stop`) or restart
  (`!admin restart`) the server remotely.
- Commands are deny-by-default: disabled until `ServerControlCommandRoleId` is set.
- Uses exit codes (0 = stop, 1 = restart) to signal intent to the process
  supervisor — works correctly with `Restart=on-failure` in systemd / Docker.

#### Configuration
- Single primary config file (`DefaultDiscordBridge.ini`) covers all settings.
- Automatic full backup: **all** settings (connection, chat, presence, whitelist,
  ban) are saved to `<ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini` on
  every startup.  If a mod update resets the primary config, the bridge restores
  every setting from the backup automatically on the next server start.
- All ban data is persisted in `<ServerRoot>/FactoryGame/Saved/ServerBanlist.json`
  and survives server restarts.
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
