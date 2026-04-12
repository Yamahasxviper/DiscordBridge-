# DiscordBridge – Whitelist

← [Back to index](README.md)

The built-in whitelist lets you restrict which players can join the server and manage
the list from Discord slash commands or in-game chat commands.

> **The whitelist is an independent access-control feature.**
> Enable or disable it freely without affecting any other functionality.

> **Note:** All Discord whitelist commands now use slash commands (`/whitelist`).
> The old `!whitelist` prefix commands and `WhitelistCommandPrefix` /
> `InGameWhitelistCommandPrefix` config fields have been removed.
> In-game commands use SML registered commands (`/ingamewhitelist`).

---

## Discord `/whitelist` slash commands

The following slash commands are available when the bot is in your Discord server:

| Command | Effect |
|---------|--------|
| `/whitelist on` | Enable the whitelist (only whitelisted players can join) |
| `/whitelist off` | Disable the whitelist (all players can join) |
| `/whitelist add <name>` | Add a player by in-game name (supports `group:GroupName` token) |
| `/whitelist remove <name>` | Remove a player by in-game name |
| `/whitelist list` | List all whitelisted players (supports optional group filter) |
| `/whitelist status` | Show the current enabled/disabled state of the whitelist |
| `/whitelist role add <discord_id>` | Grant the `WhitelistRoleId` Discord role to a user |
| `/whitelist role remove <discord_id>` | Revoke the `WhitelistRoleId` Discord role from a user |
| `/whitelist apply` | Player-initiated whitelist application (when enabled) |
| `/whitelist link` | Link Discord account to in-game player |
| `/whitelist search <partial>` | Search whitelist by partial name match |
| `/whitelist groups` | List all whitelist groups |

---

## In-game `/ingamewhitelist` commands

The following SML chat commands are available in-game:

| Command | Effect |
|---------|--------|
| `/ingamewhitelist on` | Enable the whitelist |
| `/ingamewhitelist off` | Disable the whitelist (all players can join) |
| `/ingamewhitelist add <name>` | Add a player by in-game name |
| `/ingamewhitelist remove <name>` | Remove a player by in-game name |
| `/ingamewhitelist list` | List all whitelisted players |
| `/ingamewhitelist status` | Show the current enabled/disabled state of the whitelist |

> **Note:** In-game whitelist commands support the same core operations as the Discord commands,
> except for role management, application, link, search, and groups which are Discord-only.

---

## Settings

### `WhitelistEnabled`

Controls whether the whitelist is active when the server starts.
This setting is applied on **every** server restart — change it and restart the server to enable or disable the whitelist.

`/whitelist on` / `/whitelist off` Discord slash commands update the in-memory state for the current session only; the config setting takes effect again on the next restart.

**Default:** `False` (all players can join)

---

### `WhitelistCommandRoleId`

The snowflake ID of the Discord role whose members are allowed to run `/whitelist` management commands.

**Default:** *(empty — /whitelist commands are disabled for all Discord users)*

When set, **only members who hold this role** can run `/whitelist` commands in Discord. When left empty, `/whitelist` commands are fully disabled (deny-by-default) — no one can run them until a role ID is provided.

> **IMPORTANT:** Holding this role does **not** grant access to the game server. These members are still subject to the normal whitelist and ban checks when they try to join.

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

**Example:**
```ini
WhitelistCommandRoleId=123456789012345678
```

The whitelist admin role and the ban admin role are **completely independent** — you can assign different roles to each, or use the same role for both.

---

### `WhitelistRoleId`

The snowflake ID of the Discord role used to identify whitelisted members.
Leave **empty** to disable Discord role integration.

**Default:** *(empty)*

When set:
- Discord messages sent to `WhitelistChannelId` are relayed to the game **only when the sender holds this role**.
- The `/whitelist role add/remove <discord_id>` commands assign or revoke this role via the Discord REST API (the bot must have the **Manage Roles** permission on your server).
- **Players whose in-game name matches a Discord display name (server nickname, global name, or username) of a member who holds this role are automatically allowed through the whitelist, even if they are not listed in `ServerWhitelist.json`.** The bot fetches and caches the role-member list when it connects, and keeps it up to date as members gain or lose the role.

> **Tip:** For reliable matching, set each player's Discord server nickname to their exact in-game (Steam/Epic) name before granting them the whitelist role.

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

---

### `WhitelistChannelId`

The snowflake ID of a dedicated Discord channel for whitelisted members.
Leave **empty** to disable the whitelist-only channel.

**Default:** *(empty)*

When set:
- In-game messages from players on the server whitelist are **also** posted to this channel (in addition to the main `ChannelId`).
- Discord messages sent to this channel are relayed to the game **only when the sender holds `WhitelistRoleId`** (if `WhitelistRoleId` is configured).

Get the channel ID the same way as `ChannelId` (right-click the channel in Discord with Developer Mode enabled → **Copy Channel ID**).

---

### `WhitelistKickDiscordMessage`

The message posted to the **main** Discord channel whenever a non-whitelisted player attempts to join and is kicked.
Leave **empty** to disable this notification.

**Default:** `:boot: **%PlayerName%** tried to join but is not on the whitelist and was kicked.`

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | The in-game name of the player who was kicked |

**Example:**

```ini
WhitelistKickDiscordMessage=:no_entry: **%PlayerName%** is not whitelisted and was removed from the server.
```

---

### `WhitelistKickReason`

The reason shown **in-game** to the player when they are kicked for not being on the whitelist.
This is the text the player sees in the disconnected / kicked screen.

**Default:** `You are not on this server's whitelist. Contact the server admin to be added.`

**Example:**

```ini
WhitelistKickReason=You are not whitelisted. DM an admin on Discord to request access.
```

---

### New settings (v1.1.0)

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `WhitelistApplicationChannelId` | string | *(empty)* | Channel for whitelist applications |
| `bWhitelistApplyEnabled` | bool | `False` | Enable `/whitelist apply` from Discord |
| `WhitelistApprovedDmMessage` | string | *(empty)* | DM sent when application is approved |
| `WhitelistExpiryWarningHours` | float | `24.0` | Hours before expiry to warn |
| `bWhitelistVerificationEnabled` | bool | `False` | Enable Discord verification workflow |
| `WhitelistVerificationChannelId` | string | *(empty)* | Channel for verification messages |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
