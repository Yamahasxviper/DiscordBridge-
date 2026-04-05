# TicketSystem – Standalone Discord Support-Ticket Panel

A fully standalone server-only Satisfactory mod that adds a **button-based Discord support-ticket panel** to your dedicated server. Members click a button to open a private ticket channel visible only to them and your admin/support role. No commands are required for regular members — everything is driven by button clicks and a short reason modal.

TicketSystem can run on its own with a dedicated Discord bot (**standalone mode**), or pair with DiscordBridge to share its existing bot connection (**paired mode**).

---

## Modes of operation

### Standalone mode (no DiscordBridge required)

Set `BotToken` in `DefaultTickets.ini` and TicketSystem will connect directly to the Discord Gateway using its own bot. DiscordBridge is **not** needed.

```ini
[TicketSystem]
BotToken=your-token-here
```

### Paired mode (DiscordBridge installed alongside TicketSystem)

Leave `BotToken` empty. DiscordBridge injects itself as the Discord provider and powers the ticket panel through its own bot connection. When both mods are installed, DiscordBridge always takes priority regardless of whether `BotToken` is set.

---

## Features

| Feature | Details |
|---------|---------|
| Button-based ticket panel | Members click a button — no slash commands required |
| Reason modal | A popup form collects context before the channel is created |
| Private ticket channels | Only the member + admin/support role can see each ticket channel |
| Admin @mention | The configured role is pinged in every new ticket channel |
| Built-in ticket types | Whitelist Request, Help / Support, Report a Player |
| Custom ticket reasons | Unlimited `TicketReason=Label\|Desc` entries in the config |
| Close Ticket button | Either party can click to delete the channel when done |
| Standalone Discord bot | Runs without DiscordBridge via `BotToken` in the config |
| Paired mode | Automatically uses DiscordBridge's bot connection when installed |
| Config backup | Settings are backed up and auto-restored after mod updates |

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

| Button | `custom_id` | Description |
|--------|-------------|-------------|
| Whitelist Request | `ticket_wl` | Request to be added to the server whitelist |
| Help / Support | `ticket_help` | General help or support question |
| Report a Player | `ticket_report` | Report another player to the admins |

Each can be hidden individually:

```ini
TicketWhitelistEnabled=False   ; hide the Whitelist Request button
TicketHelpEnabled=False        ; hide the Help / Support button
TicketReportEnabled=False      ; hide the Report a Player button
```

### Custom ticket reasons

```ini
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
TicketReason=Appeal|Appeal a ban or other moderation action
```

Each `TicketReason=` line adds one button to the panel. Discord allows at most **25 buttons** per message (5 rows × 5 columns).

---

## Requirements

| Mod | Version | Notes |
|-----|---------|-------|
| SML | `^3.11.3` | Required |
| SMLWebSocket | `^1.0.0` | Required — provides the Discord Gateway WebSocket client |
| DiscordBridge | `^1.0.2` | Optional — powers the panel in paired mode |

### Required bot permissions

| Permission | Why |
|------------|-----|
| Manage Channels | Create and delete private ticket channels |
| View Channel | Read channels in the server |
| Send Messages | Post the welcome message and close button |

### Required Gateway intents (standalone mode)

Enable these in the [Discord Developer Portal](https://discord.com/developers/applications) under **Bot → Privileged Gateway Intents**:

| Intent | Why |
|--------|-----|
| Server Members Intent | Role info in `MESSAGE_CREATE` |
| Message Content Intent | Read the `!ticket-panel` command |

---

## Quick setup

### Paired mode (DiscordBridge installed)

1. Install both DiscordBridge and TicketSystem on the dedicated server.
2. Open `<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini`.
3. Set **`TicketNotifyRoleId`** to your admin/support role ID.
4. Set **`TicketPanelChannelId`** to your read-only "open-a-ticket" channel ID.
5. *(Optional)* Set `TicketCategoryId` and add `TicketReason=` entries.
6. Restart the server.
7. Type `!ticket-panel` in Discord while holding the `TicketNotifyRoleId` role.

### Standalone mode

1. Go to <https://discord.com/developers/applications> and **create a new bot**.
2. Enable **Server Members Intent** and **Message Content Intent** under **Bot → Privileged Gateway Intents**.
3. Invite the bot with permissions: **Manage Channels**, **View Channel**, **Send Messages**.
4. Copy the bot token and set **`BotToken=`** in `DefaultTickets.ini`.
5. Follow steps 3–7 from the paired setup above.

---

## Configuration

The config file lives at:

```
<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini
```

A backup is written automatically to `Saved/TicketSystem/TicketSystem.ini` on every server start, and settings are restored from the backup after a mod update.

### Settings reference

| Setting | Default | Description |
|---------|---------|-------------|
| `BotToken` | *(empty)* | Discord bot token for standalone mode. Leave empty in paired mode. |
| `GuildId` | *(empty)* | Guild ID override. Auto-detected from the Discord READY event if empty (recommended). |
| `TicketChannelId` | *(empty)* | Channel(s) for admin notifications. Supports comma-separated IDs. |
| `TicketWhitelistEnabled` | `True` | Show the Whitelist Request button. |
| `TicketHelpEnabled` | `True` | Show the Help / Support button. |
| `TicketReportEnabled` | `True` | Show the Report a Player button. |
| `TicketNotifyRoleId` | *(empty)* | Role @mentioned in every ticket; also authorises `!ticket-panel`. |
| `TicketPanelChannelId` | *(empty)* | Channel where the button panel is posted. |
| `TicketCategoryId` | *(empty)* | Category under which ticket channels are created. |
| `TicketReason=Label\|Desc` | *(none)* | Custom ticket reason buttons. |

### Example configuration (standalone mode)

```ini
[TicketSystem]
; Bot token for the dedicated ticket bot
BotToken=your-token-here

; Admin/support role — pinged on new tickets, gets access to ticket channels
TicketNotifyRoleId=987654321098765432

; Read-only channel where the button panel lives
TicketPanelChannelId=111222333444555678

; Optional: group all ticket channels under a "Tickets" category
TicketCategoryId=111222333444555666

; Optional: admin notification channel
TicketChannelId=345678901234567890

; Built-in types are all enabled by default
TicketWhitelistEnabled=True
TicketHelpEnabled=True
TicketReportEnabled=True

; Custom reasons
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
```

After saving, restart the server and type `!ticket-panel` in Discord (as a member holding `TicketNotifyRoleId`) to post the button panel.

---

## Provider architecture

TicketSystem communicates with Discord exclusively through the `IDiscordBridgeProvider` interface defined in `Source/TicketSystem/Public/IDiscordBridgeProvider.h`.

When `BotToken` is set, TicketSystem creates its own `UTicketDiscordProvider` implementation at startup (standalone mode). When DiscordBridge is also installed, it calls `UTicketSubsystem::SetProvider(this)`, which disconnects the built-in provider and switches to DiscordBridge's connection.

### Interface methods

| Method | Purpose |
|--------|---------|
| `SubscribeInteraction()` / `UnsubscribeInteraction()` | Receive button clicks and modal submits (`INTERACTION_CREATE`) |
| `SubscribeRawMessage()` / `UnsubscribeRawMessage()` | Receive `MESSAGE_CREATE` events (for the `!ticket-panel` command) |
| `RespondToInteraction()` | Acknowledge button clicks (types 4 / 5 / 6) |
| `RespondWithModal()` | Show a reason modal popup (Discord response type 9) |
| `CreateDiscordGuildTextChannel()` | Create a private ticket channel |
| `DeleteDiscordChannel()` | Close / delete a ticket channel |
| `SendMessageBodyToChannel()` | Post the welcome message and close button |
| `SendDiscordChannelMessage()` | Notify the admin channel of a new ticket |
| `GetBotToken()` / `GetGuildId()` / `GetGuildOwnerId()` | Bot / guild metadata |

### Writing your own provider

Any mod that wants to power TicketSystem must:

1. Add `"TicketSystem"` as a dependency in its `.uplugin` and `Build.cs`.
2. Inherit `IDiscordBridgeProvider` in its main subsystem class.
3. Implement all pure-virtual methods in the interface.
4. Call `UTicketSubsystem::SetProvider(this)` in `Initialize()`.
5. Call `UTicketSubsystem::SetProvider(nullptr)` in `Deinitialize()`.

```cpp
void UMyDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    if (UTicketSubsystem* Tickets = GetGameInstance()->GetSubsystem<UTicketSubsystem>())
        Tickets->SetProvider(this);
}

void UMyDiscordSubsystem::Deinitialize()
{
    if (UTicketSubsystem* Tickets = GetGameInstance()->GetSubsystem<UTicketSubsystem>())
        Tickets->SetProvider(nullptr);
    Super::Deinitialize();
}
```

---

## Build targets

| Target | Platform |
|--------|----------|
| Windows | Windows listen-server / single-player host |
| WindowsServer | Windows dedicated server |
| LinuxServer | Linux dedicated server |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
