# TicketSystem – Discord Support-Ticket Panel

← [Back to index](README.md)

The **TicketSystem** subsystem adds a button-based Discord support-ticket panel to
your Satisfactory dedicated server with 23 slash commands for comprehensive ticket management.

Members click a button to open a **private ticket channel** visible only to
them and a configured admin/support role.  No commands are required for regular
members – everything is driven by button clicks and a short reason modal.

---

## Slash commands

All ticket management is performed via `/ticket` slash commands:

| Command | Description |
|---------|-------------|
| `/ticket panel` | Post the ticket panel to a channel |
| `/ticket list` | List open tickets |
| `/ticket assign <@user>` | Assign ticket to staff member |
| `/ticket claim` | Claim ticket for yourself |
| `/ticket unclaim` | Release a claimed ticket |
| `/ticket transfer <@user>` | Transfer ticket to another staff member |
| `/ticket priority <level>` | Set priority (low/normal/high/urgent) |
| `/ticket macro <name>` | Apply a response template |
| `/ticket macros` | List available templates |
| `/ticket stats` | Show ticket statistics |
| `/ticket report <text>` | Submit a report |
| `/ticket tag <tag>` | Add a tag to the current ticket |
| `/ticket untag <tag>` | Remove a tag from the current ticket |
| `/ticket tags` | List all tags on the current ticket |
| `/ticket note <text>` | Add a private staff note |
| `/ticket notes` | List all staff notes |
| `/ticket escalate` | Escalate to escalation role/category |
| `/ticket remind <text>` | Set a follow-up reminder |
| `/ticket blacklist <user>` | Blacklist a user from creating tickets |
| `/ticket unblacklist <user>` | Remove from the ticket blacklist |
| `/ticket blacklistlist` | List all blacklisted users |
| `/ticket merge <ticket_id>` | Merge two tickets together |

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
| Message Content Intent | Read slash commands and interactions |

---

## Quick setup

### Paired mode (DiscordBridge)

1. Install DiscordBridge (TicketSystem is built-in) on the dedicated server.
2. Open `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultTickets.ini`.
3. Set **`TicketNotifyRoleId`** to your admin/support role ID.
4. Set **`TicketPanelChannelId`** to the "open-a-ticket" channel ID.
5. *(Optional)* Set `TicketCategoryId` and add `TicketReason=` entries.
6. Restart the server.
7. Use `/ticket panel` in Discord while holding `TicketNotifyRoleId`.

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
| `TicketNotifyRoleId` | *(empty)* | Role @mentioned in every new ticket; also authorises `/ticket panel`. |
| `TicketPanelChannelId` | *(empty)* | Channel where the button panel is posted. |
| `TicketCategoryId` | *(empty)* | Category under which ticket channels are created. |
| `TicketReason=Label\|Desc` | *(none)* | Custom ticket reason buttons. |
| `TicketSlaWarningMinutes` | `0` | SLA warning threshold in minutes. `0` disables. |
| `TicketEscalationRoleId` | *(empty)* | Discord role pinged on `/ticket escalate`. |
| `TicketEscalationCategoryId` | *(empty)* | Category to move escalated tickets into. |
| `TicketTemplate` | *(none)* | Ticket templates (`TypeSlug\|Field1\|...`). |
| `TicketAutoResponse` | *(none)* | Auto-response on ticket creation (`TypeSlug\|message`). |

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
| `SubscribeRawMessage()` | `MESSAGE_CREATE` | Receives all messages for interaction routing |

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
