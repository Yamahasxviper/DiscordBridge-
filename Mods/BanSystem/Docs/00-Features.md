# BanSystem – Features Overview

← [Back to index](README.md)

BanSystem is a server-only Satisfactory mod that enforces player bans on dedicated servers. It uses EOS Product User IDs rather than display names, so bans cannot be evaded by renaming.

---

## Login-time enforcement

Banned players are checked at the moment they connect — before they enter the game world. The enforcer hooks `PostLogin` and kicks the player synchronously when a matching ban is found. If EOS identity resolution is still in progress (CSS async auth), the enforcer polls every 0.5 s for up to 20 s before giving up.

Both **EOS PUID bans** and **IP address bans** are enforced at login time. The player's remote IP is captured via the `PreLogin` hook and checked against any `IP:` ban records in the database — so a banned player cannot evade by creating a new EOS account while their IP ban is still active.

---

## Permanent and temporary bans

Both permanent and timed bans are supported. Temporary bans carry an expiry timestamp. On server startup, all expired bans are pruned automatically so players whose ban elapsed while the server was offline are never incorrectly kicked.

---

## Cross-platform UID linking

A single player may connect and later change their EOS identity (e.g. after an account migration or evasion). `/linkbans` associates two compound UIDs so that a ban on either identity blocks the player regardless of which EOS PUID they log in with.

→ See [Cross-Platform Linking](05-CrossPlatform.md)

---

## Persistent JSON storage

All bans are stored in a single JSON file (`bans.json`) that survives server restarts and mod updates. The file is written immediately after every change.

---

## REST management API

A local HTTP server (default port 3000) provides 41 REST endpoints for
comprehensive ban, warning, session, appeal, and audit management.
Includes Prometheus metrics export, CSV exports, a self-service appeals portal,
and a unified admin dashboard.

→ See [REST API](04-RestApi.md)

---

## Player session registry

Every player UID, display name, and **remote IP address** seen at join time is written to `player_sessions.json`. Admins can query the registry with `/playerhistory` to look up past UIDs and IPs for a player — useful for IP banning, UID linking, or tracking evasion attempts.

---

## IP address banning

In addition to EOS PUID bans, BanSystem supports **IP address bans**. IP UIDs use the format `IP:<address>` (e.g. `IP:1.2.3.4`).

- The player's remote IP is captured via the `PreLogin` hook at every connection.
- If the player's IP matches an active `IP:` ban record, they are kicked — even if they log in under a completely new EOS account.
- IP bans can be created from in-game chat (`/ban IP:1.2.3.4 Reason`) or via the REST API (`"platform": "IP"`).
- Use `/banname` to ban an offline player by name and automatically add an IP ban if their IP was recorded in the session registry.

→ See [In-Game Commands](03-ChatCommands.md) for chat command IP ban examples.
→ See [REST API](04-RestApi.md) for REST IP ban examples.

---

## In-game chat commands (BanChatCommands mod)

The optional **BanChatCommands** mod provides 42 in-game chat commands
including `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist`, `/linkbans`,
`/unlinkbans`, `/playerhistory`, `/banname`, `/reloadconfig`, `/kick`, `/modban`,
`/warn`, `/warnings`, `/clearwarns`, `/clearwarn`, `/announce`, `/stafflist`,
`/reason`, `/banreason`, `/history`, `/mute`, `/unmute`, `/tempunmute`,
`/mutecheck`, `/mutelist`, `/mutereason`, `/note`, `/notes`,
`/duration`, `/extend`, `/appeal`, `/staffchat`, `/freeze`, `/clearchat`,
`/report`, `/scheduleban`, `/qban`, `/reputation`, `/bulkban`, and `/whoami`
directly from the Satisfactory in-game chat.

→ See [In-Game Commands](03-ChatCommands.md)

---

## Warning system

BanSystem's `PlayerWarningRegistry` subsystem stores formal warnings issued by
admins via `/warn`. Warnings are persistent — they survive server restarts and are
attached to the player's compound EOS UID.

Automatic escalation bans can be configured:

- **Simple threshold** (`AutoBanWarnCount` / `AutoBanWarnMinutes`): ban the player
  when they reach N warnings, for a fixed duration.
- **Escalation tiers** (`WarnEscalationTiers`): define multiple thresholds, each
  with its own ban duration. The highest matching tier wins.

---

## Discord webhook notifications

When `DiscordWebhookUrl` is set, `BanDiscordNotifier` posts a rich embed to the
configured Discord webhook whenever:

- A player is **banned** (permanent or temporary)
- A player is **unbanned**
- A **warning** is issued
- A player is **kicked**
- A temporary ban **expires** (opt-in via `bNotifyBanExpired=True`)
- An **auto-escalation ban** is triggered by the warning system
- A **ban appeal** is submitted or reviewed
- A player is **blocked by geo-IP** filtering

---

## Scheduled bans

The `ScheduledBanRegistry` subsystem allows scheduling bans for a future UTC
timestamp. Scheduled bans are persisted to `scheduled_bans.json` and checked
every 30 seconds. When the scheduled time arrives, the ban is automatically
applied. Manage via `/scheduleban` in-game or `POST /scheduled` REST API.

---

## Ban appeals

Players can submit ban appeals via the REST API (`POST /appeals`) or the
self-service HTML portal at `/appeals/portal`. Appeals are tracked with
submission timestamps and review status. Discord notifications are sent when
an appeal is submitted or reviewed.

---

## Multi-server ban synchronisation

`BanSyncClient` enables real-time ban synchronisation across multiple
dedicated servers via WebSocket connections. Configure peer server URLs with
`+PeerWebSocketUrls` in `DefaultBanSystem.ini`. Bans and unbans are
automatically pushed to all connected peers.

---

## Player reputation scoring

A composite reputation score is calculated for each player based on their
warning count, ban history, and session behaviour. Query via `/reputation`
in-game or `GET /reputation/:uid` REST API.

---

## Automatic scheduled backups

When `BackupIntervalHours` is non-zero, BanSystem schedules a recurring timer that
writes a timestamped backup of `bans.json` automatically. This supplements the
on-demand `POST /bans/backup` REST endpoint.

---

## Requirements

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
