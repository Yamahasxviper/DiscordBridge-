# DiscordBridge – Ticket / Support System

← [Back to index](README.md)

The built-in ticket system lets **any** Discord member submit whitelist requests,
help requests, player reports, **or any custom reason you define** via a
**button-based ticket panel**.  Each button click opens a **private Discord channel**
visible only to the member and the admin/support role (who are @mentioned
automatically).  No commands to remember.

---

## Config file

All ticket settings live in their own dedicated file:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultTickets.ini
```

This file ships with the mod and is created automatically on the first server start
if it is missing.  Edit the file and restart the server to apply changes.

> **Backup:** the mod writes `<ServerRoot>/FactoryGame/Saved/Config/Tickets.ini` on
> every server start.  If a mod update resets `DefaultTickets.ini`, the backup is
> used to restore your settings automatically.

---

## Quick-start: button-based ticket panel

Members click a button instead of typing commands.
Each click creates a **private Discord channel** visible only to the member who
clicked and members of the admin/support role.  Either party can close the ticket
with a **Close Ticket** button, which automatically deletes the channel.

### Step-by-step setup

1. **Create a read-only "open-a-ticket" channel** in Discord.
   Members should be able to see it but not send messages there so the panel
   stays pinned at the top.

2. **Grant the bot `Manage Channels` permission** at the server level
   (or category level if you use `TicketCategoryId`).  This is needed to create
   and delete ticket channels.

3. **Set `TicketNotifyRoleId`** to the Discord ID of your admin / support role.
   This role is @mentioned on every new ticket **inside the ticket channel** and
   receives view / write access to each private ticket channel.

4. **Set `TicketPanelChannelId`** to the ID of the read-only channel from step 1.

5. *(Optional)* **Set `TicketCategoryId`** to a Discord category ID.
   New ticket channels will be placed inside that category so they are grouped
   together in the sidebar.

6. *(Optional)* **Add custom ticket reasons** using `TicketReason=` lines
   (see [Custom Ticket Reasons](#custom-ticket-reasons) below).

7. **Restart the server** so the new settings are applied.

8. **Post the panel** by typing `!admin ticket-panel` in any channel where you
   have the server-admin role.  The bot posts the button panel to
   `TicketPanelChannelId` and confirms success in the channel where you ran the
   command.

> **Note:** `!admin ticket-panel` requires the sender to hold the role configured
> in `ServerControlCommandRoleId`.  See [11-ServerControlCommands.md](11-ServerControlCommands.md).

After posting, the panel looks like this in Discord:

```
:ticket: Support Tickets
Click one of the buttons below to open a support ticket.
A private channel will be created that only you and the support team can see.

:clipboard: Whitelist Request – request to be added to the server whitelist
:question: Help / Support – ask for help or report a problem
:warning: Report a Player – report another player to the admins
:small_blue_diamond: Bug Report – Report a bug or technical issue    (example custom reason)

[ Whitelist Request ]  [ Help / Support ]  [ Report a Player ]  [ Bug Report ]
```

When a member clicks a button, a **reason modal** (popup form) appears so the
member can optionally describe their request before the ticket is created.
Once the modal is submitted, the member receives an ephemeral acknowledgement and
a private channel named `ticket-{type}-{username}` is created with:
- **@everyone**: cannot see the channel.
- **Admin/support role** (`TicketNotifyRoleId`): full view and write access + @mention ping.
- **Ticket opener**: full view and write access.

The ticket channel includes a welcome message (with an admin role ping) and a
red **Close Ticket** button.  Either the ticket opener or any member of the
admin/support role can click that button to close (delete) the channel.

---

## Custom Ticket Reasons

You can add as many custom ticket reason buttons as you need.  Each `TicketReason=`
line in `DefaultTickets.ini` creates one additional button on the panel.

### Format

```ini
TicketReason=Label|Description
```

| Part | Description |
|------|-------------|
| `Label` | The text shown on the Discord button.  Keep it short (80 chars max). |
| `Description` | A brief summary line shown in the panel message body and as context in the ticket channel. |

Separate `Label` and `Description` with a single `|` character.

### Example

```ini
; DefaultTickets.ini – custom ticket reasons
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
TicketReason=Appeal|Appeal a ban or other moderation action
```

This adds three extra buttons after the built-in ones.  Each creates a private
ticket channel named `ticket-bug-report-username`, `ticket-suggestion-username`,
or `ticket-appeal-username` respectively.

### Limits

Discord allows at most **25 buttons per message** (5 action rows × 5 buttons each).
The combined count of enabled built-in types and custom reasons must not exceed 25.

### Adding / removing reasons

1. Edit `DefaultTickets.ini` – add, change, or delete `TicketReason=` lines.
2. Restart the server so the new config is loaded.
3. Run `!admin ticket-panel` again to repost the panel with the updated buttons.

Removing a line simply hides that button from the next panel post.  Any tickets
that were already opened before the change continue to exist until they are closed.

---

## Settings reference

### `TicketChannelId`

The Discord ID of the **dedicated admin channel** where ticket notifications are
posted for review.  Leave **empty** to post notifications to the main bridged
channel (`ChannelId`).

**Default:** *(empty)*

Supports a **comma-separated list** of channel IDs:

```ini
TicketChannelId=345678901234567890,678901234567890123
```

> This setting does **not** control the private ticket channels created via the
> button panel — those are stand-alone private channels managed automatically.

---

### `TicketWhitelistEnabled` / `TicketHelpEnabled` / `TicketReportEnabled`

Individual on/off switches for each built-in ticket category.
Set a category to `False` to hide the corresponding button from the panel.

**Default:** `True` for all three.

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `TicketWhitelistEnabled` | bool | `True` | Show the **Whitelist Request** button. |
| `TicketHelpEnabled` | bool | `True` | Show the **Help / Support** button. |
| `TicketReportEnabled` | bool | `True` | Show the **Report a Player** button. |

---

### `TicketNotifyRoleId`

The Discord role ID to @mention in every ticket notification **and** to grant
access to newly created ticket channels.

- The bot prepends `@<Role>` to each message posted inside a new ticket channel
  as well as any notification in `TicketChannelId`.
- This role receives view/write permission for every private ticket channel.
  Without it, only the ticket opener can see the channel.

Leave **empty** to post tickets without any mention and to create ticket channels
without any admin access (not recommended).

**Default:** *(empty)*

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode),
then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

```ini
; Ping and grant access to the "Support" role on every new ticket
TicketNotifyRoleId=987654321098765432
```

---

### `TicketPanelChannelId`

The Discord channel ID where the bot posts the **button panel** when you run
`!admin ticket-panel`.  Leave **empty** to post to the main bridged channel.

**Default:** *(empty)*

```ini
; Read-only "#open-a-ticket" channel
TicketPanelChannelId=123456789012345678
```

> Run `!admin ticket-panel` again any time to re-post the panel (e.g. after a
> server restart that cleared the previous message, or after you changed which
> ticket types are enabled or added/removed custom reasons).

---

### `TicketCategoryId`

Optional Discord category ID.  When set, every new ticket channel is created
**inside this category** so all tickets are grouped in one place in the sidebar.

**Default:** *(empty – channels are created at the root of the server)*

```ini
; "Tickets" category in your Discord server
TicketCategoryId=111222333444555666
```

**How to get the category ID:**
Enable Developer Mode, right-click the category name in the channel list, and
choose **Copy Channel ID**.

---

### `TicketReason=Label|Description`

Zero or more custom ticket reason buttons.  Each line adds one button to the
panel.  See [Custom Ticket Reasons](#custom-ticket-reasons) above for full details.

**Default:** *(none – only the three built-in types are shown)*

```ini
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
```

---

## Example configuration

```ini
; DefaultTickets.ini – button panel with admin pings and custom reasons

; Admin channel where ticket notifications are posted
TicketChannelId=345678901234567890

; All three built-in ticket types enabled
TicketWhitelistEnabled=True
TicketHelpEnabled=True
TicketReportEnabled=True

; Admin role: pinged on new tickets, has access to all ticket channels
TicketNotifyRoleId=987654321098765432

; Read-only channel where the button panel lives
TicketPanelChannelId=111222333444555678

; Optional: group all ticket channels under the "Tickets" category
TicketCategoryId=111222333444555666

; Custom ticket reasons
TicketReason=Bug Report|Report a bug or technical issue with the server
TicketReason=Suggestion|Submit a suggestion or feature request
TicketReason=Appeal|Appeal a ban or other moderation action
```

After saving and restarting the server, run `!admin ticket-panel` once to post
the button message.  Members can then click any button to open a private ticket,
and the admin/support role is automatically @mentioned inside each new ticket channel.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
