# TicketSystem – Standalone Discord Support-Ticket Panel

The **TicketSystem** mod adds a button-based Discord support-ticket panel to
your Satisfactory dedicated server.  It is a **fully standalone mod** – it can
run on its own with a dedicated Discord bot, or pair with DiscordBridge to use
its existing bot connection.

Members click a button to open a **private ticket channel** visible only to
them and the configured admin/support role.  No commands required for members –
everything is driven by button clicks and a short reason modal.

---

## Modes of operation

### Standalone mode (no DiscordBridge required)

Set `BotToken` in `DefaultTickets.ini` and TicketSystem will connect directly
to the Discord Gateway with its own bot.  DiscordBridge is **not** needed.

```ini
[TicketSystem]
BotToken=your-token-here
```

### Paired mode (DiscordBridge installed alongside TicketSystem)

Leave `BotToken` empty.  DiscordBridge will inject itself as the Discord
provider and power the ticket panel through its own bot connection.  Both mods
coexist; DiscordBridge always takes priority when present regardless of whether
`BotToken` is also set in the ticket config.

---

## Requirements

| Mod | Version | Notes |
|-----|---------|-------|
| SML | `^3.11.3` | Required |
| SMLWebSocket | `^1.0.0` | Required (provides the Gateway WebSocket client) |
| DiscordBridge | `^1.0.4` | Optional – only needed in paired mode |

**Required bot permissions** (grant via the Discord Developer Portal or server
settings):

| Permission | Why |
|------------|-----|
| Manage Channels | Create and delete ticket channels |
| View Channel | Read channels in the server |
| Send Messages | Post welcome and close-button messages |

**Required Gateway intents** (Discord Developer Portal → Bot → Privileged
Gateway Intents):

| Intent | Why |
|--------|-----|
| Server Members Intent | Role info in MESSAGE_CREATE |
| Message Content Intent | Read the `!ticket-panel` command |

---

## Building with Alpakit

The mod uses the standard Alpakit build targets defined in `Config/Alpakit.ini`:

```ini
[ModTargets]
+Targets=Windows
+Targets=WindowsServer
+Targets=LinuxServer
```

This produces binaries for Windows listen-server hosts, Windows dedicated
servers, and Linux dedicated servers.

To build, open the project in the Unreal Engine editor with Alpakit installed
and click **Package Mod** on the TicketSystem plugin, or run the Alpakit CLI.

---

## Configuration

All settings live in:

```
<ServerRoot>/FactoryGame/Mods/TicketSystem/Config/DefaultTickets.ini
```

A backup is written to `Saved/TicketSystem/TicketSystem.ini` on every server start
so your settings survive mod updates.

### Quick setup (standalone mode)

1. Go to <https://discord.com/developers/applications> and **create a new bot**.

2. Under **Bot → Token**, copy the token and set `BotToken=` in the config.

3. Enable **Server Members Intent** and **Message Content Intent** under
   **Bot → Privileged Gateway Intents** in the Developer Portal.

4. **Invite the bot** to your server with permissions:
   `Manage Channels`, `View Channel`, `Send Messages`.

5. **Create a read-only "open-a-ticket" channel** in Discord.
   Members should be able to see it but not send messages there.

6. **Grant the bot `Manage Channels`** at the server (or category) level.

7. **Set `TicketNotifyRoleId`** to the ID of your admin/support role.
   This role is @mentioned in every new ticket channel and receives view/write
   access.  Members holding this role can also post the panel with
   `!ticket-panel`.

8. **Set `TicketPanelChannelId`** to the channel ID from step 5.

9. *(Optional)* Set `TicketCategoryId` to group ticket channels under one
   category.

10. *(Optional)* Add custom ticket reasons:
    ```ini
    TicketReason=Bug Report|Report a bug or technical issue
    TicketReason=Suggestion|Submit a feature request
    TicketReason=Appeal|Appeal a ban or moderation action
    ```

11. **Restart the server** to load the new settings.

12. **Post the panel** by typing `!ticket-panel` in any Discord channel while
    holding the `TicketNotifyRoleId` role.

---

## How it works

```
Member clicks button → reason modal appears
                     → private ticket channel created
                     → @role pinged inside channel
                     → admin sees and responds
                     → either party clicks "Close Ticket" → channel deleted
```

### Built-in ticket types

| Button | custom_id | Description |
|--------|-----------|-------------|
| Whitelist Request | `ticket_wl` | Request to be added to the server whitelist |
| Help / Support | `ticket_help` | General help or support question |
| Report a Player | `ticket_report` | Report another player to admins |

All three are enabled by default and can be disabled individually:

```ini
TicketWhitelistEnabled=False   ; hide the Whitelist Request button
TicketHelpEnabled=False        ; hide the Help / Support button
TicketReportEnabled=False      ; hide the Report a Player button
```

### Custom ticket reasons

```ini
; DefaultTickets.ini
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
TicketReason=Appeal|Appeal a ban or other moderation action
```

Each `TicketReason=` line adds one grey button to the panel.  Discord allows
at most **25 buttons** per message (5 rows × 5 buttons).

---

## Settings reference

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `BotToken` | string | *(empty)* | Discord bot token for standalone mode. |
| `GuildId` | string | *(empty)* | Guild ID override (auto-detected if empty). |
| `TicketChannelId` | string | *(empty)* | Channel(s) for admin notifications. |
| `TicketWhitelistEnabled` | bool | `True` | Show Whitelist Request button. |
| `TicketHelpEnabled` | bool | `True` | Show Help / Support button. |
| `TicketReportEnabled` | bool | `True` | Show Report a Player button. |
| `TicketNotifyRoleId` | string | *(empty)* | Role @mentioned in every new ticket. |
| `TicketPanelChannelId` | string | *(empty)* | Channel for the button panel. |
| `TicketCategoryId` | string | *(empty)* | Category for new ticket channels. |
| `TicketReason=Label\|Desc` | multi | *(none)* | Custom ticket reason buttons. |

---

## Example configuration (standalone)

```ini
[TicketSystem]
; Bot token for the dedicated ticket bot (standalone mode)
BotToken=your-token-here

; Admin channel for ticket notifications
TicketChannelId=345678901234567890

; All built-in types enabled
TicketWhitelistEnabled=True
TicketHelpEnabled=True
TicketReportEnabled=True

; Admin/support role – pinged on new tickets, gets access to ticket channels
TicketNotifyRoleId=987654321098765432

; Read-only channel where the panel lives
TicketPanelChannelId=111222333444555678

; Group all ticket channels under the "Tickets" category
TicketCategoryId=111222333444555666

; Custom reasons
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
```

After saving and restarting, type `!ticket-panel` in Discord (holding the
`TicketNotifyRoleId` role) to post the button panel.

---

## Provider architecture

TicketSystem communicates with the Discord bot exclusively through the
`IDiscordBridgeProvider` interface defined in
`Source/TicketSystem/Public/IDiscordBridgeProvider.h`.

When `BotToken` is set in the config, TicketSystem creates its own internal
`UTicketDiscordProvider` implementation at startup (standalone mode).  When
DiscordBridge is also installed it calls `UTicketSubsystem::SetProvider(this)`,
which automatically disconnects the built-in provider and switches to
DiscordBridge's connection instead.

The interface exposes:

| Method | Purpose |
|--------|---------|
| `SubscribeInteraction()` / `UnsubscribeInteraction()` | Receive button clicks and modal submits (`INTERACTION_CREATE`) |
| `SubscribeRawMessage()` / `UnsubscribeRawMessage()` | Receive raw `MESSAGE_CREATE` events (for the `!ticket-panel` command) |
| `RespondToInteraction()` | Acknowledge button clicks / show modals |
| `RespondWithModal()` | Show a reason modal popup (Discord response type 9) |
| `CreateDiscordGuildTextChannel()` | Create a private ticket channel |
| `DeleteDiscordChannel()` | Close/delete a ticket channel |
| `SendMessageBodyToChannel()` | Post the welcome message and close button |
| `SendDiscordChannelMessage()` | Notify the admin channel of a new ticket |
| `GetBotToken()` / `GetGuildId()` / `GetGuildOwnerId()` | Bot / guild metadata |

### Writing your own provider

Any mod that wants to power TicketSystem should:

1. Add `"TicketSystem"` as a dependency in its `.uplugin` and `Build.cs`.
2. Inherit `IDiscordBridgeProvider` in its main subsystem class.
3. Implement all pure-virtual methods in the interface.
4. In `Initialize()` look up `UTicketSubsystem` and call `SetProvider(this)`.
5. In `Deinitialize()` call `SetProvider(nullptr)` then release the pointer.

```cpp
// Example sketch (adapt to your own subsystem)
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

*For help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
