# BanSystem – In-Game Chat Commands

← [Back to index](README.md)

In-game chat commands are provided by the separate **BanChatCommands** mod. Install it alongside BanSystem to manage bans from the Satisfactory in-game chat.

→ See [Getting Started](01-GettingStarted.md) for installation instructions.

---

## Admin setup

Admin access is controlled by EOS PUIDs in `BanChatCommands.ini`:

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

When the list is empty, admin commands can only be run from the **server console**. The `/whoami` command is always available to all players regardless of admin status.

**How to find your UID:** Type `/whoami` in-game. The response will show your 32-character EOS PUID. Add it to the `+AdminEosPUIDs=` list.

---

## Command reference

### `/ban`

Permanently ban a player or IP address.

```
/ban <player|UID|IP:address> [reason...]
```

| Argument | Description |
|----------|-------------|
| `player` | In-game display name (case-insensitive substring match; player must be **online**) |
| `UID` | Compound UID (`EOS:xxx`) or raw 32-char EOS PUID |
| `IP:address` | IP address ban (e.g. `IP:1.2.3.4`) |
| `reason` | Optional ban reason (everything after the target) |

**Examples:**
```
/ban SomePlayer Griefing
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban 00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban IP:1.2.3.4 VPN evader
```

---

### `/tempban`

Ban a player or IP address for a set number of minutes.

```
/tempban <player|UID|IP:address> <minutes> [reason...]
```

| Argument | Description |
|----------|-------------|
| `minutes` | Duration in minutes (must be greater than 0) |

**Examples:**
```
/tempban SomePlayer 60 Spamming
/tempban EOS:00020aed06f0a6958c3c067fb4b73d51 1440 Toxic behaviour (24 h)
/tempban EOS:00020aed06f0a6958c3c067fb4b73d51 10080 Repeated griefing (1 week)
/tempban IP:1.2.3.4 60 Suspicious VPN
```

---

### `/unban`

Remove an existing ban by UID or IP address. Display names are **not** accepted — use `/bancheck` to find the UID first if needed.

```
/unban <UID|IP:address>
```

**Examples:**
```
/unban EOS:00020aed06f0a6958c3c067fb4b73d51
/unban 00020aed06f0a6958c3c067fb4b73d51
/unban IP:1.2.3.4
```

---

### `/unbanname`

Remove the ban for an offline player by display-name substring, using the player session registry. Also removes the associated IP ban if one was recorded.

```
/unbanname <name_substring>
```

**Examples:**
```
/unbanname SomePlayer
/unbanname some
```

**Notes:**
- The player does not need to be currently connected.
- If the name matches more than one session record, all matches are listed and you must use a more specific substring.
- The player must have connected at least once while the session registry was active. If no record exists, use `/unban <PUID>` directly.

---

### `/bancheck`

Query whether a player, UID, or IP address is currently banned.

```
/bancheck <player|UID|IP:address>
```

Returns the ban entry (reason, expiry, linked UIDs) if the player is banned, or a "not banned" message otherwise.

**Examples:**
```
/bancheck SomePlayer
/bancheck EOS:00020aed06f0a6958c3c067fb4b73d51
/bancheck IP:1.2.3.4
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
/banlist 3
```

---

### `/linkbans`

Link two compound UIDs together so that a ban on either identity blocks the player from joining regardless of which account they use.

```
/linkbans <UID1> <UID2>
```

Both UIDs must already exist as ban records. The link is stored on both records. Works for EOS-to-EOS pairs as well as EOS-to-IP pairs.

→ See [UID Linking](05-CrossPlatform.md) for details and use cases.

**Examples:**
```
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

---

### `/unlinkbans`

Remove the link between two compound UIDs. The underlying bans are not removed.

```
/unlinkbans <UID1> <UID2>
```

**Examples:**
```
/unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
/unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

---

### `/playerhistory`

Look up past session records for a player by display name or UID.
Returns up to 20 results from the session registry, sorted by most recently seen.

```
/playerhistory <name_substring|UID>
```

**Examples:**
```
/playerhistory SomePlayer
/playerhistory some
/playerhistory EOS:00020aed06f0a6958c3c067fb4b73d51
```

**Use cases:**
- Find the UID of a player who has disconnected.
- Detect EOS PUID changes that may indicate ban evasion.
- Retrieve the IP address recorded at a player's last login.

---

### `/whoami`

Show your own compound UID. Available to all players — no admin required.

```
/whoami
```

Use this to find out your EOS Product User ID so you can add it to the admin list in `BanChatCommands.ini`.

---

### `/banname`

Ban an offline player by display-name substring using the player session registry. Permanently bans both their EOS PUID and their recorded IP address (if any), and links the two records together.

```
/banname <name_substring> [reason...]
```

The player does not need to be currently connected — the command searches session history. If the name matches more than one record, all matches are listed.

**Examples:**
```
/banname SomePlayer Griefing
/banname some Cheating
```

---

### `/reloadconfig`

Hot-reload the BanChatCommands admin list from disk without restarting the server.

```
/reloadconfig
```

Useful after editing `DefaultBanChatCommands.ini` or `Saved/Config/<Platform>/BanChatCommands.ini` while the server is running.

---

## Player name vs UID resolution

When you pass a display name (anything that is not a recognised UID format) to `/ban`, `/tempban`, or `/bancheck`:

1. All currently connected `PlayerController`s are iterated.
2. The first player whose display name **contains** the query (case-insensitive) is selected.
3. Their platform UID is resolved from the active session.

If more than one connected player matches the name, the command lists all ambiguous matches and asks you to be more specific or use a UID directly.

> **Tip:** Use `/playerhistory` to look up UIDs for players who are no longer online, then ban by UID.

---

## IP address banning

All commands that accept a `UID` also accept an **IP address** in the form `IP:<address>` (IPv4 or IPv6). IP bans are enforced at connection time — the player's remote IP is captured at `PreLogin` and checked against every `IP:` ban record before the player enters the game.

### Banning an IP directly

```
; Permanent IP ban
/ban IP:1.2.3.4 VPN evader

; Temporary IP ban (60 minutes)
/tempban IP:1.2.3.4 60 Suspicious traffic

; Remove an IP ban
/unban IP:1.2.3.4

; Check an IP ban
/bancheck IP:1.2.3.4
```

### Banning EOS + IP together with /banname

`/banname` looks up a player in the session registry and bans both their EOS PUID and their recorded IP address in a single command:

```
/banname SomePlayer Griefing
```

This creates two linked ban records — `EOS:xxx` and `IP:1.2.3.4` — so the player is blocked whether they reconnect under the same EOS account or create a new one from the same IP.

### Finding a player's IP

Use `/playerhistory` to retrieve the IP address recorded at a player's last login:

```
/playerhistory SomePlayer
```

The output includes the compound UID and IP address from the session registry. Copy the IP to use in a standalone `/ban IP:x.x.x.x` command.

### Linking an IP ban to an EOS ban

If you want to link an existing IP ban record to an existing EOS ban so enforcement triggers on either identity:

```
/ban IP:1.2.3.4 Cheating
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
