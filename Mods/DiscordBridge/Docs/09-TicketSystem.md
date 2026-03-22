# TicketSystem – Discord Support-Ticket Panel

← [Back to index](README.md)

The **TicketSystem** mod adds a button-based Discord support-ticket panel to
your Satisfactory dedicated server.  It is a **fully standalone mod** – it has
no compile-time dependency on DiscordBridge.  **DiscordBridge powers
TicketSystem** by implementing the `IDiscordBridgeProvider` interface that
TicketSystem exposes.

Members click a button to open a **private ticket channel** visible only to
them and a configured admin/support role.  No commands are required for regular
members – everything is driven by button clicks and a short reason modal.

TicketSystem is a separate, optional mod.  Installing it alongside DiscordBridge
enables the full ticket workflow; no changes to `DefaultDiscordBridge.ini` are
needed.

---

## Requirements

| Mod | Notes |
|-----|-------|
| **TicketSystem** | The standalone ticket mod (this mod) |
| **DiscordBridge** | Provides the Discord bot connection and implements `IDiscordBridgeProvider` |
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

## How DiscordBridge powers TicketSystem

TicketSystem exposes an `IDiscordBridgeProvider` interface
(`Source/TicketSystem/Public/IDiscordBridgeProvider.h`).  DiscordBridge
implements this interface and calls `UTicketSubsystem::SetProvider(this)` during
its own `Initialize()`.  TicketSystem then uses the provider for all Discord
communication; it has no direct knowledge of DiscordBridge internals.

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
