# DiscordBridge – Whitelist

← [Back to index](README.md)

The built-in whitelist lets you restrict which players can join the server and manage
the list directly from the bridged Discord channel.

> **The whitelist and the ban system are completely independent.**
> You can use either one, both, or neither — enabling or disabling one never affects the other.
>
> | Goal | Config |
> |------|--------|
> | Whitelist only | `WhitelistEnabled=True`, `BanSystemEnabled=False` |
> | Ban system only | `WhitelistEnabled=False`, `BanSystemEnabled=True` *(default)* |
> | Both | `WhitelistEnabled=True`, `BanSystemEnabled=True` |
> | Neither | `WhitelistEnabled=False`, `BanSystemEnabled=False` |

---

## Settings

### `WhitelistEnabled`

Controls whether the whitelist is active when the server starts.
This setting is applied on **every** server restart — change it and restart the server to enable or disable the whitelist.

`!whitelist on` / `!whitelist off` Discord commands update the in-memory state for the current session only; the config setting takes effect again on the next restart.

**Default:** `False` (all players can join)

---

### `WhitelistCommandRoleId`

The snowflake ID of the Discord role whose members are allowed to run `!whitelist` management commands.

**Default:** *(empty — !whitelist commands are disabled for all Discord users)*

When set, **only members who hold this role** can run `!whitelist` commands in the bridged Discord channel. When left empty, `!whitelist` commands are fully disabled (deny-by-default) — no one can run them until a role ID is provided.

> **IMPORTANT:** Holding this role does **not** grant access to the game server. These members are still subject to the normal whitelist and ban checks when they try to join.

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

**Example:**
```ini
WhitelistCommandRoleId=123456789012345678
```

The whitelist admin role and the ban admin role are **completely independent** — you can assign different roles to each, or use the same role for both.

---

### `WhitelistCommandPrefix`

The prefix that triggers whitelist management commands when typed in the bridged Discord channel.
Set to an **empty string** to disable Discord-based whitelist management entirely.

**Default:** `!whitelist`

**Supported commands** (type these in the bridged Discord channel):

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist (only whitelisted players can join) |
| `!whitelist off` | Disable the whitelist (all players can join) |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players and current enabled/disabled state |
| `!whitelist status` | Show the current enabled/disabled state of **both** the whitelist and the ban system |
| `!whitelist role add <discord_id>` | Grant the `WhitelistRoleId` Discord role to a user |
| `!whitelist role remove <discord_id>` | Revoke the `WhitelistRoleId` Discord role from a user |

---

### `WhitelistRoleId`

The snowflake ID of the Discord role used to identify whitelisted members.
Leave **empty** to disable Discord role integration.

**Default:** *(empty)*

When set:
- Discord messages sent to `WhitelistChannelId` are relayed to the game **only when the sender holds this role**.
- The `!whitelist role add/remove <discord_id>` commands assign or revoke this role via the Discord REST API (the bot must have the **Manage Roles** permission on your server).
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

### `InGameWhitelistCommandPrefix`

The prefix that triggers whitelist management commands when typed in the **Satisfactory in-game chat**.
This lets server admins manage the whitelist directly from inside the game without needing Discord.
Set to an **empty string** to disable in-game whitelist commands.

**Default:** `!whitelist`

**Supported commands** (type these in the Satisfactory in-game chat):

| Command | Effect |
|---------|--------|
| `!whitelist on` | Enable the whitelist |
| `!whitelist off` | Disable the whitelist (all players can join) |
| `!whitelist add <name>` | Add a player by in-game name |
| `!whitelist remove <name>` | Remove a player by in-game name |
| `!whitelist list` | List all whitelisted players and current enabled/disabled state |
| `!whitelist status` | Show the current enabled/disabled state of **both** the whitelist and the ban system |

> **Note:** In-game whitelist commands support the same operations as the Discord commands,
> except for role management (`!whitelist role add/remove`) which is Discord-only.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
