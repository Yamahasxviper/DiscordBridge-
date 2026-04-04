# BanSystem – In-Game Chat Commands

← [Back to index](README.md)

In-game chat commands are provided by the separate **BanChatCommands** mod. Install it alongside BanSystem to manage bans from the Satisfactory in-game chat.

→ See [Getting Started](01-GettingStarted.md) for installation instructions.

---

## Admin setup

Admin access is controlled by platform IDs in `DefaultGame.ini`:

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminSteam64Ids=76561198000000000
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

When both lists are empty, admin commands can only be run from the **server console**. The `/whoami` command is always available to all players regardless of admin status.

**How to find your UID:** Type `/whoami` in-game. The response will show your compound UID in the format `STEAM:xxx` or `EOS:xxx`. Add the raw ID portion (the part after the colon) to the appropriate list.

---

## Command reference

### `/ban`

Permanently ban a player.

```
/ban <player|UID> [reason...]
```

| Argument | Description |
|----------|-------------|
| `player` | In-game display name (case-insensitive substring match; player must be online) |
| `UID` | Compound UID (`STEAM:xxx` or `EOS:xxx`) or raw Steam 64-bit ID / EOS PUID |
| `reason` | Optional ban reason (everything after the UID) |

**Examples:**
```
/ban SomePlayer Griefing
/ban STEAM:76561198000000000 Cheating
/ban 76561198000000000 Cheating
```

---

### `/tempban`

Ban a player for a set number of minutes.

```
/tempban <player|UID> <minutes> [reason...]
```

| Argument | Description |
|----------|-------------|
| `minutes` | Duration in minutes (must be greater than 0) |

**Examples:**
```
/tempban SomePlayer 60 Spamming
/tempban EOS:00020aed06f0a6958c3c067fb4b73d51 1440 Toxic behaviour (24 h)
```

---

### `/unban`

Remove an existing ban.

```
/unban <UID>
```

`UID` must be a full compound UID (`STEAM:xxx` or `EOS:xxx`) or a raw Steam 64-bit ID / EOS PUID. Display names are not accepted here — use `/bancheck` to find the UID first if needed.

**Examples:**
```
/unban STEAM:76561198000000000
/unban 76561198000000000
```

---

### `/bancheck`

Query whether a player or UID is currently banned.

```
/bancheck <player|UID>
```

Returns the ban entry (reason, expiry, linked UIDs) if the player is banned, or a "not banned" message otherwise.

**Examples:**
```
/bancheck SomePlayer
/bancheck STEAM:76561198000000000
```

---

### `/banlist`

List all currently active bans, paginated (10 per page).

```
/banlist [page]
```

**Examples:**
```
/banlist
/banlist 2
```

---

### `/linkbans`

Link two compound UIDs together so that a ban on either identity blocks the player from joining regardless of which platform they use.

```
/linkbans <UID1> <UID2>
```

Both UIDs must already exist as ban records. The link is stored on both records.

→ See [Cross-Platform Linking](05-CrossPlatform.md) for details and use cases.

**Example:**
```
/linkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

### `/unlinkbans`

Remove the link between two compound UIDs.

```
/unlinkbans <UID1> <UID2>
```

**Example:**
```
/unlinkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

### `/playerhistory`

Look up past session records for a player by display name or UID.
Returns up to 20 results from the session registry.

```
/playerhistory <name_substring|UID>
```

Useful for finding a player's compound UID after they have disconnected, or for spotting EOS PUID changes that may indicate ban evasion.

**Examples:**
```
/playerhistory SomePlayer
/playerhistory STEAM:76561198000000000
```

---

### `/whoami`

Show your own compound UID. Available to all players — no admin required.

```
/whoami
```

Use this to find out your Steam 64-bit ID or EOS Product User ID so you can add it to the admin list in `DefaultGame.ini`.

---

## Player name vs UID resolution

When you pass a display name (anything that is not a recognised UID format) to `/ban`, `/tempban`, or `/bancheck`:

1. All currently connected `PlayerController`s are iterated.
2. The first player whose display name **contains** the query (case-insensitive) is selected.
3. Their platform UID is resolved from the active session.

If more than one connected player matches the name, the command lists all ambiguous matches and asks you to be more specific or use a UID directly.

> **Tip:** Use `/playerhistory` to look up UIDs for players who are no longer online, then ban by UID.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
