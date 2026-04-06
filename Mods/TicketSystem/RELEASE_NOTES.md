# TicketSystem – Release Notes

## v1.0.0 – Initial Release

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Overview

TicketSystem is a **standalone server-only** Satisfactory mod that adds a
button-based Discord support-ticket panel to your dedicated server.

Members click a button to open a **private ticket channel** visible only to them
and the configured admin/support role.  No commands are required for regular
members — everything is driven by button clicks and a short reason modal.

---

### Features

#### Two modes of operation

**Standalone mode** — set `BotToken` in `DefaultTickets.ini` and TicketSystem
connects to the Discord Gateway with its own bot.  DiscordBridge is not required.

**Paired mode** — leave `BotToken` empty and install DiscordBridge alongside
TicketSystem.  DiscordBridge injects itself as the Discord provider automatically;
TicketSystem uses the existing bot connection with no extra configuration.

#### Button-based ticket panel

- Members click a button — no slash commands or typing required.
- A reason **modal popup** collects context before the private channel is created.
- Built-in ticket types: **Whitelist Request**, **Help / Support**,
  **Report a Player**.  Each can be hidden individually.
- Unlimited custom ticket reasons via `TicketReason=Label|Desc` config entries.
  Discord allows up to **25 buttons** per message.

#### Private ticket channels

- Each ticket creates a private channel visible only to the member and the admin
  role (`TicketNotifyRoleId`).
- The admin role is @mentioned automatically when the channel is created.
- Either party can click **Close Ticket** to delete the channel when done.

#### Panel management

- Post the button panel to any channel by typing `!ticket-panel` in Discord while
  holding the `TicketNotifyRoleId` role.
- The panel can be re-posted to refresh it at any time.

#### Configuration & backup

- Config lives at
  `<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini`.
- A backup is written to
  `<ServerRoot>/FactoryGame/Saved/TicketSystem/TicketSystem.ini` on every server
  start.  Settings are restored from the backup automatically after a mod update.

---

### Requirements

| Dependency | Minimum version | Notes |
|------------|----------------|-------|
| Satisfactory (dedicated server) | build ≥ 416835 | |
| SML | ≥ 3.11.3 | |
| SMLWebSocket | ≥ 1.0.0 | Required (provides WebSocket/SSL for Discord gateway) |
| DiscordBridge | ≥ 1.0.4 | Optional – only needed in paired mode |

**Required bot permissions** (in your Discord server):

| Permission | Why |
|------------|-----|
| Manage Channels | Create and delete private ticket channels |
| View Channel | Read channels in the server |
| Send Messages | Post the welcome message and close button |

**Required Gateway intents** (standalone mode only — enable in the Discord
Developer Portal under Bot → Privileged Gateway Intents):

| Intent | Why |
|--------|-----|
| Server Members Intent | Role info in `MESSAGE_CREATE` |
| Message Content Intent | Read the `!ticket-panel` command |

---

### Getting Started

See the [TicketSystem documentation](Docs/TicketSystem.md) for full setup
instructions for both standalone and paired mode.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
