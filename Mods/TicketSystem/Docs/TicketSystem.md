# TicketSystem – Standalone Discord Support-Ticket Panel

The **TicketSystem** mod adds a button-based Discord support-ticket panel to
your Satisfactory dedicated server.  It is a **fully standalone mod** – it has
no compile-time dependency on DiscordBridge or any other specific Discord mod.
Instead, it exposes an `IDiscordBridgeProvider` interface (defined in
`Source/TicketSystem/Public/IDiscordBridgeProvider.h`) that any mod can
implement to power the ticket system.

[DiscordBridge](../DiscordBridge/README.md) is the reference implementation: it
implements `IDiscordBridgeProvider` and calls `UTicketSubsystem::SetProvider(this)`
during its own `Initialize()` so both mods work together out of the box.

Members click a button to open a **private ticket channel** visible only to
them and the configured admin/support role.  No commands required for members –
everything is driven by button clicks and a short reason modal.

---

## Requirements

| Mod | Version |
|-----|---------|
| SML | `^3.11.3` |

A mod that implements `IDiscordBridgeProvider` (such as DiscordBridge) must be
installed and running for TicketSystem to send or receive Discord events.
DiscordBridge must be configured with a valid `BotToken` before tickets can be
created or closed.

**Required bot permissions** (grant via the Discord Developer Portal or server
settings):

| Permission | Why |
|------------|-----|
| Manage Channels | Create and delete ticket channels |
| View Channel | Read channels in the server |
| Send Messages | Post welcome and close-button messages |

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

A backup is written to `Saved/Config/TicketSystem.ini` on every server start
so your settings survive mod updates.

### Quick setup

1. **Create a read-only "open-a-ticket" channel** in Discord.
   Members should be able to see it but not send messages there.

2. **Grant the bot `Manage Channels`** at the server (or category) level.

3. **Set `TicketNotifyRoleId`** to the ID of your admin/support role.
   This role is @mentioned in every new ticket channel and receives view/write
   access.  Members holding this role can also post the panel with
   `!ticket-panel`.

4. **Set `TicketPanelChannelId`** to the channel ID from step 1.

5. *(Optional)* Set `TicketCategoryId` to group ticket channels under one
   category.

6. *(Optional)* Add custom ticket reasons:
   ```ini
   TicketReason=Bug Report|Report a bug or technical issue
   TicketReason=Suggestion|Submit a feature request
   TicketReason=Appeal|Appeal a ban or moderation action
   ```

7. **Restart the server** to load the new settings.

8. **Post the panel** by typing `!ticket-panel` in any Discord channel while
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
| `TicketChannelId` | string | *(empty)* | Channel(s) for admin notifications. |
| `TicketWhitelistEnabled` | bool | `True` | Show Whitelist Request button. |
| `TicketHelpEnabled` | bool | `True` | Show Help / Support button. |
| `TicketReportEnabled` | bool | `True` | Show Report a Player button. |
| `TicketNotifyRoleId` | string | *(empty)* | Role @mentioned in every new ticket. |
| `TicketPanelChannelId` | string | *(empty)* | Channel for the button panel. |
| `TicketCategoryId` | string | *(empty)* | Category for new ticket channels. |
| `TicketReason=Label\|Desc` | multi | *(none)* | Custom ticket reason buttons. |

---

## Example configuration

```ini
[TicketSystem]
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

## Interaction with a provider (DiscordBridge or custom)

TicketSystem communicates with the Discord bot exclusively through the
`IDiscordBridgeProvider` interface defined in
`Source/TicketSystem/Public/IDiscordBridgeProvider.h`.  The provider is
injected at runtime by calling `UTicketSubsystem::SetProvider(this)`; no
compile-time dependency on any specific Discord mod is required.

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

DiscordBridge is the reference provider implementation – it calls
`UTicketSubsystem::SetProvider(this)` during its `Initialize()` and
`SetProvider(nullptr)` during `Deinitialize()`.

### Writing your own provider

Any future mod that wants to power TicketSystem should:

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
