# TicketSystem – Discord Support-Ticket Panel

← [Back to index](README.md)

The **TicketSystem** mod adds a button-based Discord support-ticket panel on top
of DiscordBridge.  Members click a button to open a **private ticket channel**
visible only to them and a configured admin/support role.  No commands are
required for regular members – everything is driven by button clicks and a short
reason modal.

TicketSystem is a separate, optional mod.  DiscordBridge exposes the integration
points it needs; no changes to DiscordBridge's own config file are required.

---

## Requirements

| Mod | Notes |
|-----|-------|
| DiscordBridge | Must be installed and have a valid `BotToken` configured |
| SML | `^3.11.3` |

The Discord bot must also have the following permissions (in addition to the
standard DiscordBridge permissions):

| Permission | Why |
|------------|-----|
| **Manage Channels** | Create and delete private ticket channels |
| **View Channel** | Read channels in the server |
| **Send Messages** | Post welcome and close-button messages |

---

## Quick setup

1. Install both DiscordBridge and TicketSystem on the dedicated server.
2. Open `<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini`.
3. Set **`TicketNotifyRoleId`** to your admin/support role ID.  
   This role is @mentioned inside every new ticket channel and receives
   view/write access.  Members holding this role can also post the ticket panel
   by typing `!ticket-panel` in any Discord channel.
4. Set **`TicketPanelChannelId`** to the channel where members will click
   buttons to open tickets (a read-only "open-a-ticket" channel works well).
5. *(Optional)* Set `TicketCategoryId` to group ticket channels in one category.
6. *(Optional)* Add custom ticket reason buttons:
   ```ini
   TicketReason=Bug Report|Report a bug or technical issue with the server
   TicketReason=Suggestion|Submit a feature request
   TicketReason=Appeal|Appeal a ban or other moderation action
   ```
7. Restart the server.
8. Type `!ticket-panel` in any Discord channel while holding the
   `TicketNotifyRoleId` role.  The bot posts the button panel.

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
| `TicketChannelId` | *(empty)* | Channel(s) for admin notifications (comma-separated). |
| `TicketWhitelistEnabled` | `True` | Show Whitelist Request button. |
| `TicketHelpEnabled` | `True` | Show Help / Support button. |
| `TicketReportEnabled` | `True` | Show Report a Player button. |
| `TicketNotifyRoleId` | *(empty)* | Role @mentioned in every new ticket; also authorises `!ticket-panel`. |
| `TicketPanelChannelId` | *(empty)* | Channel where the button panel is posted. |
| `TicketCategoryId` | *(empty)* | Category under which ticket channels are created. |
| `TicketReason=Label\|Desc` | *(none)* | Custom ticket reason buttons. |

---

## How TicketSystem integrates with DiscordBridge

TicketSystem subscribes to two native multicast delegates that DiscordBridge
exposes for extension modules:

| Delegate | Event | Purpose |
|----------|-------|---------|
| `OnDiscordInteractionReceived` | `INTERACTION_CREATE` | Receives button clicks and modal submits |
| `OnDiscordRawMessageReceived` | `MESSAGE_CREATE` | Receives all messages so `!ticket-panel` is seen regardless of channel |

It also calls DiscordBridge's public REST helpers to perform all Discord API
operations:

| Method | Used for |
|--------|----------|
| `CreateDiscordGuildTextChannel()` | Create a private ticket channel |
| `DeleteDiscordChannel()` | Close/delete a ticket channel |
| `SendMessageBodyToChannel()` | Post the welcome message and close button |
| `SendDiscordChannelMessage()` | Notify the admin channel of a new ticket |
| `RespondToInteraction()` | Acknowledge button clicks and show modals |

No changes to `DefaultDiscordBridge.ini` are needed.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
