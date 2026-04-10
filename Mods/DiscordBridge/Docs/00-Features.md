# DiscordBridge – Features Overview

← [Back to index](README.md)

DiscordBridge is a server-only Satisfactory mod that creates a live two-way bridge
between your dedicated server's in-game chat and a Discord text channel.  Everything
is configured through a single INI file with no external service or dashboard required.

---

## Two-way chat bridge

- In-game player messages are forwarded to Discord in real time.
- Discord messages are relayed into the game's in-game chat in real time.
- Both directions use configurable format strings (`%PlayerName%`, `%Username%`,
  `%Message%`, `%ServerName%`).
- Discord markdown (bold, italics, code blocks, etc.) is preserved when posting game
  messages to Discord.
- Messages from other Discord bots can be silently dropped (`IgnoreBotMessages=True`)
  to prevent echo loops when another bot shares the channel.
- **Find-and-replace blocklist** (`ChatRelayBlocklistReplacements`): substitute matched
  patterns with a replacement string (default `***`) before relaying to Discord —
  separate from the hard-drop blocklist (`ChatRelayBlocklist`).

→ See [Chat Bridge](03-ChatBridge.md)

---

## Server status announcements

- A configurable message is posted to the bridged channel when the server comes online
  (`ServerOnlineMessage`).
- A configurable message is posted when the server shuts down gracefully
  (`ServerOfflineMessage`).
- Either notification can be suppressed by clearing its value, or both can be disabled
  globally with `ServerStatusMessagesEnabled=False`.
- A dedicated status channel (`StatusChannelId`) can be used to keep online/offline
  pings separate from regular chat.

→ See [Server Status & Behaviour](06-ServerStatus.md)

---

## Live player-count presence

- The bot's Discord "Now Playing" activity line shows the current player count in real
  time.
- The format string (`PlayerCountPresenceFormat`), update interval
  (`PlayerCountUpdateIntervalSeconds`, minimum 15 s), and activity verb
  (`PlayerCountActivityType`: Playing / Listening to / Watching / Competing in) are all
  configurable.
- The presence can be disabled entirely with `ShowPlayerCountInPresence=False`.

→ See [Player Count Presence](07-PlayerCountPresence.md)

---

## Player notifications

- Posts a Discord message when a player **joins**, **leaves**, or **times out**.
- All three events can be sent to a dedicated channel (`PlayerEventsChannelId`) or
  left to use the main `ChannelId`.
- Each message is independently configurable (`PlayerJoinMessage`,
  `PlayerLeaveMessage`, `PlayerTimeoutMessage`).
- Join messages support additional placeholders: `%SteamId%` and `%EOSProductUserId%`.
- **Rich embed mode** (`bUseEmbedsForPlayerEvents=True`): post events as Discord embeds
  instead of plain text — required for reaction voting.
- **Admin join log** (`PlayerJoinAdminChannelId`): a private channel receives full join
  details including EOS PUID and IP address, keeping sensitive data off the public feed.
- Disabled by default — set `PlayerEventsEnabled=True` to opt in.

→ See [Player Notifications](04-PlayerNotifications.md)

---

## Whitelist

- Restrict which players can join and manage the list from Discord or in-game using
  `!whitelist` commands.
- Optional Discord role integration (`WhitelistRoleId`): members holding the whitelist
  role are automatically allowed by display-name matching, without a manual
  `!whitelist add` entry.
- A dedicated whitelist channel (`WhitelistChannelId`) mirrors in-game messages from
  whitelisted players and accepts Discord messages only from role holders.
- Configurable kick reason (`WhitelistKickReason`) and Discord kick notification
  (`WhitelistKickDiscordMessage`).

**Discord & in-game `!whitelist` commands**

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist |
| `!whitelist off` | Disable the whitelist |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players |
| `!whitelist status` | Show the state of the whitelist and ban system |
| `!whitelist role add <discord_id>` | Grant the whitelist role *(Discord only)* |
| `!whitelist role remove <discord_id>` | Revoke the whitelist role *(Discord only)* |

→ See [Whitelist](05-Whitelist.md)

---

## Game phase and schematic unlock announcements

- Posts a Discord message whenever the server's **game phase** advances (e.g. Pioneer
  → Ficsit Pioneer Candidate).
- Posts a Discord message whenever a **schematic (milestone)** is purchased on the server.
- Both event types can be routed to a dedicated channel (`PhaseEventsChannelId`,
  `SchematicEventsChannelId`) or left to fall back to `ChannelId`.

→ See [Game Events](10-GameEvents.md)

---

## Discord stats commands

- **`!stats`** — any Discord user can type this in the bridged channel to get a live
  server summary (player count, uptime, and aggregate session counters).
- **`!playerstats <PlayerName>`** — retrieves per-player session counters (time online,
  messages sent, etc.) for any player who has been seen on the server.
- Both command prefixes are configurable and can be disabled by clearing the value.

→ See [Stats Commands](11-StatsCommands.md)

---

## `!players` command

- Typing `!players` in the bridged channel returns the current online player list.
- The response can be routed to a dedicated channel (`PlayersCommandChannelId`) or
  sent to the originating channel.

---

## Reaction-based vote-kick

- When enabled, a rich embed is posted for each joining player.
- Discord members react with 👎 to vote against the player.
- Once the configurable threshold (`VoteKickThreshold`) is reached within the vote
  window (`VoteWindowMinutes`), the player is automatically kicked.
- Requires `bUseEmbedsForPlayerEvents=True`.

→ See [Reaction Voting](12-ReactionVoting.md)

---

## AFK auto-kick

- Players who are idle for longer than `AfkKickMinutes` minutes are automatically
  kicked with a configurable reason (`AfkKickReason`).
- Disabled by default — set `AfkKickMinutes` to a positive value to enable.

→ See [AFK Kick](13-AfkKick.md)

---

## Bot info channel & `!help` command

- On the **first server start** the bot automatically posts a full feature and
  command reference as rich Discord embeds into a configurable dedicated channel
  (`BotInfoChannelId`).
- Discord members can type **`!help`** or **`!commands`** in the main bridged
  channel at any time to re-post the same reference on demand.
- The reference is split into two embeds: one for general / server commands
  (chat bridge, `!server`, `!online`, `!stats`, `!playerstats`, whitelist) and
  one for admin/moderator BanSystem commands (only shown when BanSystem is active).
- Command prefixes (e.g. `!players`) in the embed reflect live config values.
- Leave `BotInfoChannelId` empty to skip the automatic post; `!help` still works.

→ See [Bot Info Channel & !help](14-BotInfoChannel.md)

---

## Ban events channel

- When used alongside **BanSystem**, ban and unban actions can be posted to a
  dedicated Discord channel (`BanEventsChannelId`).
- Falls back to `ChannelId` when the setting is empty.

---

## Discord invite URL broadcast

- Set `DiscordInviteUrl` to automatically announce the server's Discord invite link
  to players in-game.

---

## Configuration & persistence

- Single primary config file (`DefaultDiscordBridge.ini`) covers all settings and is
  **not** overwritten by mod updates.
- A full backup is written to `Saved/DiscordBridge/DiscordBridge.ini` on every server start;
  if the primary file is ever missing the bridge automatically restores all settings
  from the backup.
- Whitelist data persists in `Saved/ServerWhitelist.json` across server restarts.

→ See [Getting Started](01-GettingStarted.md) and [Connection Settings](02-ConnectionSettings.md)

---

## TicketSystem (optional add-on mod)

The **TicketSystem** mod extends DiscordBridge with a button-based Discord
support-ticket panel.

- Members click a button to open a **private ticket channel** visible only to
  them and an admin/support role.
- A reason modal (popup) collects context before the channel is created.
- Built-in ticket types: Whitelist Request, Help / Support, Report a Player.
- Unlimited custom ticket reasons configurable via `TicketReason=Label|Desc`.
- Admins post the panel by typing `!ticket-panel` in any Discord channel.
- Either party can click **Close Ticket** to delete the channel when done.
- No changes to `DefaultDiscordBridge.ini` are needed; TicketSystem reads its
  own `DefaultTickets.ini`.

→ See [TicketSystem](09-TicketSystem.md)

---

## Requirements

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |
| SMLWebSocket | ≥ 1.0.0 |

The Discord bot must have **Server Members Intent** and **Message Content Intent**
enabled in the Discord Developer Portal, plus **Send Messages** and **Read Message
History** permissions in the target channel.  The **Manage Roles** permission is also
required when using `!whitelist role` commands.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
