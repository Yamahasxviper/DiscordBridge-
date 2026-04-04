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

## Ban system

Ban functionality has been removed from this project. For ban enforcement, consider using a separate dedicated ban mod or server-level tooling.

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

## Configuration & persistence

- Single primary config file (`DefaultDiscordBridge.ini`) covers all settings and is
  **not** overwritten by mod updates.
- A full backup is written to `Saved/Config/DiscordBridge.ini` on every server start;
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
