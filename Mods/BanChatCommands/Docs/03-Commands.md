# BanChatCommands – Commands Reference

← [Back to index](README.md)

---

## Quick reference

| Command | Role | Syntax |
|---------|:----:|--------|
| `/ban` | Admin | `/ban <player\|UID\|IP:address> [reason...]` |
| `/tempban` | Admin | `/tempban <player\|UID\|IP:address> <minutes> [reason...]` |
| `/unban` | Admin | `/unban <UID\|IP:address>` |
| `/unbanname` | Admin | `/unbanname <name_substring>` |
| `/bancheck` | Admin | `/bancheck <player\|UID\|IP:address>` |
| `/banlist` | Admin | `/banlist [page]` |
| `/linkbans` | Admin | `/linkbans <UID1> <UID2>` |
| `/unlinkbans` | Admin | `/unlinkbans <UID1> <UID2>` |
| `/playerhistory` | Admin | `/playerhistory <name_substring\|UID>` |
| `/banname` | Admin | `/banname <name_substring> [reason...]` |
| `/reloadconfig` | Admin | `/reloadconfig` |
| `/warn` | Admin | `/warn <player\|UID> <reason...>` |
| `/warnings` | Admin | `/warnings <player\|UID>` |
| `/clearwarns` | Admin | `/clearwarns <player\|UID>` |
| `/clearwarn` | Admin | `/clearwarn <player\|UID> <id>` |
| `/reason` | All | `/reason <UID>` |
| `/banreason` | Admin | `/banreason <UID> <new reason>` |
| `/announce` | Admin | `/announce <message...>` |
| `/stafflist` | Moderator | `/stafflist` |
| `/note` | Admin | `/note <player\|UID> <text>` |
| `/notes` | Admin | `/notes <player\|UID>` |
| `/duration` | Admin | `/duration <UID>` |
| `/extend` | Admin | `/extend <UID> <minutes>` |
| `/appeal` | All | `/appeal <reason...>` |
| `/staffchat` | Moderator | `/staffchat <message...>` |
| `/scheduleban` | Admin | `/scheduleban <player\|UID> <timestamp> [reason]` |
| `/qban` | Admin | `/qban <template> <player\|UID>` |
| `/reputation` | Admin | `/reputation <player\|UID>` |
| `/bulkban` | Admin | `/bulkban <UID1> <UID2> ... [reason]` |
| `/kick` | Moderator | `/kick <player\|UID> [reason...]` |
| `/modban` | Moderator | `/modban <player\|UID> [reason...]` |
| `/mute` | Admin | `/mute <player\|UID> [duration] [reason...]` |
| `/unmute` | Admin | `/unmute <player\|UID>` |
| `/tempunmute` | Moderator | `/tempunmute <player\|UID>` |
| `/mutecheck` | Moderator | `/mutecheck <player\|UID>` |
| `/mutelist` | Moderator | `/mutelist` |
| `/mutereason` | Admin | `/mutereason <player\|UID> <reason>` |
| `/freeze` | Admin | `/freeze <player\|UID>` |
| `/clearchat` | Admin | `/clearchat [reason...]` |
| `/report` | All | `/report <player> [reason...]` |
| `/history` | All | `/history` |
| `/whoami` | All | `/whoami` |

---

## Player targeting

Commands that accept `<player|UID>` resolve the argument in this order:

1. **Compound UID** — a string starting with `EOS:` is used as-is.
2. **32-character hex string** — treated as a raw EOS Product User ID (`EOS:` prefix added automatically).
3. **Display name** — case-insensitive substring match against currently connected players. If more than one player matches, the command lists all ambiguous matches and asks you to be more specific. Offline players must be targeted by UID.

> **Tip:** Use `/playerhistory <name>` to find the UID for a player who is no longer online.

---

## /ban

```
/ban <player|UID|IP:address> [reason...]
```

Permanently bans a player or IP address. The ban is stored in BanSystem's database and enforced at every future login attempt.

**Requires admin.**

| Argument | Description |
|----------|-------------|
| `player` | Display name (case-insensitive substring; player must be **online**) |
| `UID` | Compound UID (`EOS:xxx`) or raw 32-char EOS PUID |
| `IP:address` | IP address — e.g. `IP:1.2.3.4` |
| `reason` | Optional free-text reason (everything after the target) |

```
/ban SomePlayer Griefing
/ban 00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Duplicate account
/ban IP:1.2.3.4 VPN evader
```

**Notes:**
- If the target is an online player, they are kicked immediately.
- The ban persists across server restarts.
- To ban an offline player by name, use `/banname` instead.

---

## /tempban

```
/tempban <player|UID|IP:address> <minutes> [reason...]
```

Temporarily bans a player or IP address for the specified number of minutes. The ban is lifted automatically when the time expires; BanSystem prunes expired bans on startup and periodically at runtime.

**Requires admin.**

| Argument | Description |
|----------|-------------|
| `minutes` | Duration in whole minutes (minimum 1) |
| `reason` | Optional free-text reason |

```
/tempban SomePlayer 60 Spamming
/tempban 00020aed06f0a6958c3c067fb4b73d51 1440 Toxic behaviour (24 h)
/tempban EOS:00020aed06f0a6958c3c067fb4b73d51 10080 Griefing (1 week)
/tempban IP:1.2.3.4 60 Suspicious traffic
```

**Notes:**
- If the target is online they are kicked immediately; they can rejoin once the ban expires.
- Use `/bancheck` to see the remaining expiry time.

---

## /unban

```
/unban <UID|IP:address>
```

Removes an existing ban. Accepts a compound EOS UID (`EOS:xxx`), a raw 32-char PUID, or an IP address (`IP:x.x.x.x`). Display names are **not** accepted — use `/bancheck` to look up the exact UID first if needed.

**Requires admin.**

```
/unban EOS:00020aed06f0a6958c3c067fb4b73d51
/unban 00020aed06f0a6958c3c067fb4b73d51
/unban IP:1.2.3.4
```

**Notes:**
- Unbanning a UID does **not** automatically remove linked UIDs. Use `/bancheck` to see linked UIDs and `/unban` each separately if needed.
- To unban an offline player by display name, use `/unbanname` instead.

---

## /unbanname

```
/unbanname <name_substring>
```

Removes the ban for an offline player by searching the **player session registry** for a matching display name. This is the counterpart to `/banname` — the player does not need to be currently connected.

- Removes the EOS PUID ban if one exists.
- Also removes the associated IP ban if an IP address was recorded at their last login.
- If the name substring matches more than one session record, all matches are listed and you must use a more specific substring.

**Requires admin.**

```
/unbanname SomePlayer
/unbanname some
```

> **Note:** The player must have connected at least once while the session registry was active. If no record exists, use `/unban <PUID>` directly.

**Example output (success):**
```
[BanChatCommands] Removed EOS ban for 'SomePlayer' (EOS:00020aed06f0a6958c3c067fb4b73d51).
[BanChatCommands] Also removed IP ban for 1.2.3.4.
```

**Example output (no ban found):**
```
[BanChatCommands] No active EOS ban found for 'SomePlayer' (EOS:00020aed06f0a6958c3c067fb4b73d51).
```

**Example output (ambiguous name):**
```
[BanChatCommands] Ambiguous name 'some' — 2 matches: "SomePlayer" (EOS:xxx...), "SomeOther" (EOS:yyy...). Use a more specific substring.
```

---

## /bancheck

```
/bancheck <player|UID|IP:address>
```

Reports the current ban status for a player or IP address. Shows the ban reason, expiry time (for temp bans), and linked UIDs if any.

**Requires admin.**

```
/bancheck SomePlayer
/bancheck 00020aed06f0a6958c3c067fb4b73d51
/bancheck EOS:00020aed06f0a6958c3c067fb4b73d51
/bancheck IP:1.2.3.4
```

**Notes:**
- If a temp ban has already expired it will be shown as "expired" rather than active.
- The output includes any linked UIDs so you can see all connected identities.

---

## /banlist

```
/banlist [page]
```

Lists all currently active bans (permanent and unexpired temporary). Results are paginated at 10 entries per page. Pass an optional page number to view further pages.

**Requires admin.**

```
/banlist
/banlist 2
/banlist 3
```

Each entry is shown in the format:
```
[EOS] 00020aed06f0a6958c3c067fb4b73d51 | SomePlayer | By: AdminName | Expires: permanent
[IP]  1.2.3.4                           | -          | By: AdminName | Expires: 2024-06-01T12:00:00
```

---

## /linkbans

```
/linkbans <UID1> <UID2>
```

Links two compound UIDs so that a ban on one also blocks the player when they connect under the other identity. Both UIDs must already have ban records. The link is bidirectional.

**Requires admin.**

```
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

**Notes:**
- Both UIDs **must** already have ban records before linking. Create them first with `/ban`.
- Linking works across EOS UID pairs as well as EOS UID + IP pairs.
- The link is stored on both records so enforcement works in both directions.

**Typical ban-evasion workflow:**
```
; 1. Player reconnects under a new EOS PUID — find it:
/playerhistory SomePlayer
; 2. Ban the new UID:
/ban EOS:aabbccdd11223344aabbccdd11223344 Cheating (ban evasion)
; 3. Link the two bans:
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344
```

---

## /unlinkbans

```
/unlinkbans <UID1> <UID2>
```

Removes the bidirectional link between two UIDs that was created by `/linkbans`. The underlying bans are **not** removed — use `/unban` separately if you also want to lift them.

**Requires admin.**

```
/unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344
/unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

---

## /playerhistory

```
/playerhistory <name_substring|UID>
```

Searches the BanSystem player session registry. Pass a display name substring to find all UIDs that have connected under that name, or pass a UID to find the display name and IP recorded for it. Returns up to 20 results sorted by most recently seen.

**Requires admin.**

```
/playerhistory SomePlayer
/playerhistory some
/playerhistory EOS:00020aed06f0a6958c3c067fb4b73d51
```

**Use cases:**
- Find a player's UID after they have disconnected.
- Correlate past identities when a player reconnects with a different EOS PUID.
- Retrieve the IP address recorded at a player's last login (to add a `/ban IP:...` or `/banname`).

---

## /whoami

```
/whoami
```

Shows the calling player's own EOS Product User ID. No admin required — any connected player can use this command to find the exact PUID they need for a ban lookup or to give to a server admin.

On CSS Dedicated Server, all players — regardless of launch platform (Steam, Epic, etc.) — are identified by their EOS PUID.

**No admin required.**

Example output:
```
[BanChatCommands] Your EOS PUID: 00020aed06f0a6958c3c067fb4b73d51
```

**Tip:** Use this command to find your own PUID so you can add it to the admin list in `BanChatCommands.ini`.

---

## /banname

```
/banname <name_substring> [reason...]
```

Bans a player by display-name substring using the **player session registry** — the player does not need to be currently connected. When a match is found, both their EOS PUID **and** their recorded IP address (if any) are permanently banned and the two records are linked together.

If the name substring matches more than one session record, the command lists all ambiguous matches and asks you to use a more specific substring.

**Requires admin.**

```
/banname SomePlayer Griefing
/banname some Cheating
```

> **Note:** The player must have connected at least once while the session registry was active. If no record exists, use `/ban <PUID>` directly.

**Comparison with `/ban`:**

| | `/ban <name>` | `/banname <name>` |
|---|---|---|
| Player online? | Required | Not required |
| Bans IP? | No | Yes (if recorded) |
| Links EOS + IP? | No | Yes |

---

## /reloadconfig

```
/reloadconfig
```

Forces BanChatCommands to re-read the admin list from disk immediately — no server restart required. Useful after editing `DefaultBanChatCommands.ini` or `Saved/Config/<Platform>/BanChatCommands.ini` while the server is running.

**Requires admin (or server console).**

```
/reloadconfig
```

---

## IP address banning

All commands that accept a UID also accept an **IP address** in the format `IP:<address>` (IPv4 or IPv6).

IP bans are enforced at connection time — the player's remote IP is captured at `PreLogin` and checked against every `IP:` ban record before they enter the game. This means a banned player cannot evade by creating a new EOS account if their IP ban is still active.

### UID format for IP bans

| Format | Example |
|--------|---------|
| `IP:<address>` | `IP:1.2.3.4` |

### Quick reference

```
; Permanently ban an IP address
/ban IP:1.2.3.4 VPN evader

; Temporarily ban an IP for 60 minutes
/tempban IP:1.2.3.4 60 Suspicious traffic

; Remove an IP ban
/unban IP:1.2.3.4

; Check if an IP is banned
/bancheck IP:1.2.3.4
```

### Finding a player's IP

Use `/playerhistory` to look up the IP address recorded at a player's last login:

```
/playerhistory SomePlayer
```

The response includes the EOS compound UID and IP from the session registry. You can then ban the IP separately or use `/banname` to ban both the EOS PUID and IP in a single step.

### Banning EOS + IP together

`/banname <name>` bans both the EOS PUID **and** the IP address (if recorded) in one command, and links the two records:

```
/banname SomePlayer Griefing
```

To do the same manually:

```
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban IP:1.2.3.4 Cheating
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 IP:1.2.3.4
```

---

## /kick

```
/kick <player|UID> [reason...]
```

Disconnects a player from the server without creating a ban record. The player can
reconnect immediately after being kicked.

**Requires moderator or admin.**

```
/kick SomePlayer Stop griefing
/kick EOS:00020aed06f0a6958c3c067fb4b73d51 Please calm down
```

> **Tip:** Set `bCreateWarnOnKick=True` in `BanChatCommands.ini` to automatically
> record a warning entry when a player is kicked, preserving the kick reason in
> their warning history.

---

## /modban

```
/modban <player|UID> [reason...]
```

Issues a **30-minute temporary ban** — a quick moderator shortcut that doesn't
require specifying a duration. For longer bans, use `/tempban` (admin required).

**Requires moderator or admin.**

```
/modban SomePlayer Spamming chat
/modban 00020aed06f0a6958c3c067fb4b73d51 Harassment
```

---

## /warn

```
/warn <player|UID> <reason...>
```

Issues a formal warning to a player. Warnings are stored persistently in
BanSystem's `PlayerWarningRegistry`. If BanSystem's warning-escalation thresholds
are configured, reaching a threshold automatically bans the player.

**Requires admin.**

```
/warn SomePlayer Please follow the server rules
/warn 00020aed06f0a6958c3c067fb4b73d51 Toxic chat behaviour
```

---

## /warnings

```
/warnings <player|UID>
```

Lists all recorded warnings for a player, showing the warning number, reason,
issuing admin, and date.

**Requires admin.**

```
/warnings SomePlayer
/warnings EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /clearwarns

```
/clearwarns <player|UID>
```

Clears all warnings for a player from the `PlayerWarningRegistry` and reports
how many were removed.

**Requires admin.**

```
/clearwarns SomePlayer
/clearwarns EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /announce

```
/announce <message...>
```

Broadcasts a message to **all connected players** via the in-game chat. Also
mirrors the announcement to the bridged Discord channel (via DiscordBridge, if
installed).

**Requires admin.**

```
/announce Server restarting in 5 minutes — please save and disconnect!
/announce Welcome to the server! Type /whoami to see your UID.
```

---

## /stafflist

```
/stafflist
```

Lists all currently-online admins and moderators so players know who to contact
for help or to report rule violations.

**Requires moderator or admin.** (Server console can always run this.)

---

## /reason

```
/reason <UID>
```

Displays the ban reason stored for a given compound UID. Useful when reviewing
a `/banlist` entry to recall why a player was banned.

**No role required — available to all players.**

```
/reason EOS:00020aed06f0a6958c3c067fb4b73d51
/reason IP:1.2.3.4
```

---

## /history

```
/history
```

Shows the calling player's own past session records and any warnings they have
received. No admin required — any connected player can run this.

**No role required.**

---

## /mute

```
/mute <player|UID> [duration] [reason...]
```

Silences a player's in-game chat messages. Mutes are held in the in-memory
`UMuteRegistry` subsystem and are **not** persisted — they are cleared when the
server restarts.

- `duration`: Optional. Accepts an integer number of minutes (`30`), or shorthand
  like `30m`, `1h`, `2h30m`, `1d`, `7d`, `1d12h`. Omit for an indefinite mute.

**Requires admin.**

```
/mute SpamBot 30m Spamming chat
/mute SpamBot 2h Harassment
/mute 00020aed06f0a6958c3c067fb4b73d51 1d Repeated violations
/mute SomePlayer          ; indefinite (until unmuted or restart)
```

---

## /unmute

```
/unmute <player|UID>
```

Removes an in-memory mute immediately.

**Requires admin.**

```
/unmute SpamBot
/unmute 00020aed06f0a6958c3c067fb4b73d51
```

---

## /clearwarn

```
/clearwarn <player|UID> <warning_id>
```

Remove a specific warning by its numeric ID. Use `/warnings` first to find the ID.

**Requires admin.**

```
/clearwarn SomePlayer 3
/clearwarn EOS:00020aed06f0a6958c3c067fb4b73d51 1
```

---

## /banreason

```
/banreason <UID> <new reason...>
```

Edit the ban reason for an existing ban record.

**Requires admin.**

```
/banreason EOS:00020aed06f0a6958c3c067fb4b73d51 Updated: Griefing and harassment
```

---

## /note

```
/note <player|UID> <text...>
```

Add a persistent admin note to a player's record. Notes are stored alongside warnings
and session data.

**Requires admin.**

```
/note SomePlayer Verbal warning about language in chat
/note EOS:00020aed06f0a6958c3c067fb4b73d51 Contacted about base placement
```

---

## /notes

```
/notes <player|UID>
```

List all admin notes for a player.

**Requires admin.**

```
/notes SomePlayer
```

---

## /duration

```
/duration <UID>
```

Show the remaining time on a temporary ban.

**Requires admin.**

```
/duration EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /extend

```
/extend <UID> <minutes>
```

Add time to an existing temporary ban.

**Requires admin.**

```
/extend EOS:00020aed06f0a6958c3c067fb4b73d51 1440
```

---

## /appeal

```
/appeal <reason...>
```

Submit your own ban appeal to the server's appeal registry. The appeal is
forwarded to configured staff channels for review.

**No role required — available to all players.**

```
/appeal I understand the rules now and won't do it again
```

---

## /staffchat

```
/staffchat <message...>
```

Send a message visible only to online admins and moderators. Not relayed to Discord.

**Requires moderator or admin.**

```
/staffchat Let's keep an eye on the new player
```

---

## /tempunmute

```
/tempunmute <player|UID>
```

Remove a timed mute immediately.

**Requires moderator or admin.**

---

## /mutecheck

```
/mutecheck <player|UID>
```

Check if a player is currently muted and show remaining duration.

**Requires moderator or admin.**

```
/mutecheck SomePlayer
```

---

## /mutelist

```
/mutelist
```

List all currently active mutes.

**Requires moderator or admin.**

---

## /mutereason

```
/mutereason <player|UID> <reason...>
```

Edit the reason for an existing mute.

**Requires admin.**

```
/mutereason SomePlayer Updated: Continued spam after warning
```

---

## /freeze

```
/freeze <player|UID>
```

Toggle movement lock on a player. When frozen, the player cannot move but can
still chat. Run again to unfreeze.

**Requires admin.**

```
/freeze SomePlayer
```

---

## /clearchat

```
/clearchat [reason...]
```

Flush the in-game chat history for all players and post a notification embed
to the bridged Discord channel.

**Requires admin.**

---

## /report

```
/report <player> [reason...]
```

Submit a player report to the configured staff channels. Available to any
connected player — no role required.

**No role required — available to all players.**

```
/report SomePlayer Suspected speed hacking
```

---

## /scheduleban

```
/scheduleban <player|UID> <timestamp> [reason...]
```

Schedule a ban to take effect at a future UTC timestamp (ISO 8601 format).

**Requires admin.**

```
/scheduleban SomePlayer 2025-06-01T12:00:00Z Repeated rule violations
```

---

## /qban

```
/qban <template_name> <player|UID>
```

Apply a pre-configured quick-ban template. Templates are defined in BanSystem's
config via `BanTemplates=`.

**Requires admin.**

```
/qban griefing SomePlayer
/qban cheating EOS:00020aed06f0a6958c3c067fb4b73d51
```

---

## /reputation

```
/reputation <player|UID>
```

Show a composite reputation score for a player based on their warning count,
ban history, and session behaviour.

**Requires admin.**

```
/reputation SomePlayer
```

---

## /bulkban

```
/bulkban <UID1> <UID2> ... [reason...]
```

Ban multiple players at once. All provided UIDs are banned with the same reason.

**Requires admin.**

```
/bulkban EOS:aaa... EOS:bbb... EOS:ccc... Coordinated griefing
```
