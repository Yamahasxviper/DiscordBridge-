# TicketSystem – Discord Support-Ticket Panel

← [Back to index](README.md)

The **TicketSystem** mod adds a button-based Discord support-ticket panel to
your Satisfactory dedicated server.  It is a **fully standalone mod** – it can
run with its own dedicated Discord bot, or pair with DiscordBridge to use
DiscordBridge's existing bot connection.

Members click a button to open a **private ticket channel** visible only to
them and a configured admin/support role.  No commands are required for regular
members – everything is driven by button clicks and a short reason modal.

---

## Modes of operation

### Standalone mode (no DiscordBridge required)

Set `BotToken` in `DefaultTickets.ini` with a bot token dedicated to the ticket
system.  DiscordBridge is **not** required.

### Paired mode (DiscordBridge installed)

Leave `BotToken` empty in `DefaultTickets.ini`.  DiscordBridge powers the
ticket panel through its own bot connection.  When both mods are installed,
DiscordBridge always takes priority regardless of whether `BotToken` is set in
the ticket config.

---

## Requirements

| Mod | Notes |
|-----|-------|
| **SML** | `^3.11.3` |
| **SMLWebSocket** | `^1.0.0` – provides the Discord Gateway WebSocket client |
| **DiscordBridge** | Optional – only needed in paired mode |

The Discord bot must also have the following permissions (in addition to any
standard DiscordBridge permissions):

| Permission | Why |
|------------|-----|
| **Manage Channels** | Create and delete private ticket channels |
| **View Channel** | Read channels in the server |
| **Send Messages** | Post welcome and close-button messages |

For **standalone mode**, the following Gateway intents must be enabled in the
[Discord Developer Portal](https://discord.com/developers/applications) under
**Bot → Privileged Gateway Intents**:

| Intent | Why |
|--------|-----|
| Server Members Intent | Role info in MESSAGE_CREATE |
| Message Content Intent | Read the `!ticket-panel` command |

---

## Quick setup

### Paired mode (DiscordBridge)

1. Install both DiscordBridge and TicketSystem on the dedicated server.
2. Open `<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini`.
3. Set **`TicketNotifyRoleId`** to your admin/support role ID.
4. Set **`TicketPanelChannelId`** to the "open-a-ticket" channel ID.
5. *(Optional)* Set `TicketCategoryId` and add `TicketReason=` entries.
6. Restart the server.
7. Type `!ticket-panel` in Discord while holding `TicketNotifyRoleId`.

### Standalone mode

1. Create a new bot at <https://discord.com/developers/applications>.
2. Enable **Server Members Intent** and **Message Content Intent** under **Bot → Privileged Gateway Intents**.
3. Invite the bot with **Manage Channels**, **View Channel**, **Send Messages**.
4. Copy the bot token and set **`BotToken=`** in `DefaultTickets.ini`.
5. Follow steps 3–7 from the paired setup above (skip step 1).

---

## How it works

```
Member clicks button
  → reason modal appears (member describes their request)
  → private ticket channel created (only member + admin role can see it)
  → admin role @mentioned inside the channel
  → admin reviews and responds
  → either party clicks "Close Ticket" → channel deleted
```

### Built-in ticket types

| Button | Description |
|--------|-------------|
| **Whitelist Request** | Request to be added to the server whitelist |
| **Help / Support** | General help or support question |
| **Report a Player** | Report another player to the admins |

All three are enabled by default.  Each can be hidden individually:

```ini
TicketWhitelistEnabled=False   ; hide the Whitelist Request button
TicketHelpEnabled=False        ; hide the Help / Support button
TicketReportEnabled=False      ; hide the Report a Player button
```

### Custom ticket reasons

Any number of extra buttons can be added with `TicketReason=` entries:

```ini
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
```

Discord allows at most **25 buttons** per message (5 rows × 5 buttons).  The
combined count of enabled built-in types and custom reasons must not exceed 25.

---

## Settings reference

| Setting | Default | Description |
|---------|---------|-------------|
| `BotToken` | *(empty)* | Discord bot token for standalone mode. |
| `GuildId` | *(empty)* | Guild ID override (auto-detected if empty). |
| `TicketChannelId` | *(empty)* | Channel(s) for admin notifications (comma-separated). |
| `TicketWhitelistEnabled` | `True` | Show Whitelist Request button. |
| `TicketHelpEnabled` | `True` | Show Help / Support button. |
| `TicketReportEnabled` | `True` | Show Report a Player button. |
| `TicketNotifyRoleId` | *(empty)* | Role @mentioned in every new ticket; also authorises `!ticket-panel`. |
| `TicketPanelChannelId` | *(empty)* | Channel where the button panel is posted. |
| `TicketCategoryId` | *(empty)* | Category under which ticket channels are created. |
| `TicketReason=Label\|Desc` | *(none)* | Custom ticket reason buttons. |

---

## How DiscordBridge powers TicketSystem (paired mode)

TicketSystem exposes an `IDiscordBridgeProvider` interface
(`Source/TicketSystem/Public/IDiscordBridgeProvider.h`).  DiscordBridge
implements this interface and calls `UTicketSubsystem::SetProvider(this)` during
its own `Initialize()`.  When it does, TicketSystem automatically disconnects
its built-in provider (if `BotToken` was set) and switches to DiscordBridge's
connection.  TicketSystem has no direct knowledge of DiscordBridge internals.

**Events DiscordBridge delivers to TicketSystem via the interface:**

| Method | Event | Purpose |
|--------|-------|---------|
| `SubscribeInteraction()` | `INTERACTION_CREATE` | Receives button clicks and modal submits |
| `SubscribeRawMessage()` | `MESSAGE_CREATE` | Receives all messages so `!ticket-panel` is seen regardless of channel |

**REST helpers DiscordBridge provides through the interface:**

| Method | Used for |
|--------|----------|
| `CreateDiscordGuildTextChannel()` | Create a private ticket channel |
| `DeleteDiscordChannel()` | Close/delete a ticket channel |
| `SendMessageBodyToChannel()` | Post the welcome message and close button |
| `SendDiscordChannelMessage()` | Notify the admin channel of a new ticket |
| `RespondToInteraction()` | Acknowledge button clicks (types 4, 5, 6) |
| `RespondWithModal()` | Show a reason modal popup (type 9) |

No changes to `DefaultDiscordBridge.ini` are needed.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
