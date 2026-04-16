# Features Roadmap

> **Last updated:** 2026-04-16  
> This file is maintained by Copilot and updated whenever you ask for a fresh suggestions pass.  
> ✅ = already implemented &nbsp;|&nbsp; 💡 = suggested / not yet built  
> All features are **server-side only** — no client download required.

---

## DiscordBridge

### Chat Bridge
| Status | Feature |
|--------|---------|
| ✅ | Two-way in-game ↔ Discord chat relay |
| ✅ | `%PlayerName%`, `%Message%`, `%ServerName%`, `%Role%` format placeholders |
| ✅ | Discord role labels resolved to `%Role%` placeholder |
| ✅ | Bot-message filtering (ignore other bots) |
| 💡 | **Multi-channel relay** — mirror chat to multiple Discord channels simultaneously (e.g. one public + one private admin copy) |
| 💡 | **Keyword/phrase alerts** — if an in-game message contains a configured word or regex, ping a staff role in a private channel |
| 💡 | **Message editing sync** — when a Discord message is edited, update the in-game relay (or post a follow-up) |

### Server Status & Presence
| Status | Feature |
|--------|---------|
| ✅ | Online / offline notifications with configurable messages |
| ✅ | Dedicated `StatusChannelId` for status messages |
| ✅ | Live player-count bot presence (configurable interval, activity type) |
| 💡 | **Auto-post server stats embed** — periodic embed in a status channel: uptime, player count, active bans, active mutes |
| 💡 | **`/server status` Discord command** — on-demand version of the stats embed |
| 💡 | **Scheduled announcements** — `/admin schedule <time> <message>` posts to a channel at a specified UTC time |

### Player Notifications
| Status | Feature |
|--------|---------|
| ✅ | Join / leave / timeout notifications |
| ✅ | Dedicated `PlayerEventsChannelId` |
| ✅ | Private admin channel with EOS PUID + IP on join |
| 💡 | **Rich join embed** — include session count, previous-ban flag, alt-account detection warning in the admin-channel join post |
| 💡 | **AFK kick notification** — send a configurable in-game message to the kicked player explaining why they were removed |
| 💡 | **Duplicate-name alert** — warn admins in the admin channel when a joining player's name closely matches a known banned player |

### Whitelist
| Status | Feature |
|--------|---------|
| ✅ | Whitelist enforcement on join |
| ✅ | Timed whitelist entries (`ExpiresAt`) |
| ✅ | Group/tier tags, `MaxSlots` capacity, partial-name search |
| ✅ | Whitelist audit log |
| 💡 | **`/whitelist bulk` Discord command** — add multiple players at once via a comma-separated list or a modal text block |
| 💡 | **Discord role sync on whitelist add** — optionally assign a configurable Discord role when a player is whitelisted |
| 💡 | **Whitelist ticket auto-action** — when a whitelist ticket is approved by staff, automatically add the user to the whitelist |

### AFK Kick
| Status | Feature |
|--------|---------|
| ✅ | AFK detection via pawn-location delta (50 UU) + build-count delta |
| ✅ | Configurable AFK timeout |
| 💡 | **AFK warning message** — send an in-game warning N minutes before the kick fires |
| 💡 | **AFK exemption list** — EOS PUIDs that are never AFK-kicked (e.g. trusted builders doing long automation runs) |

### In-Game Messages
| Status | Feature |
|--------|---------|
| ✅ | Scheduled broadcast messages (`DefaultInGameMessages.ini`) |
| 💡 | **Condition-based broadcasts** — only post a scheduled message if player count ≥ N (e.g. "remember the rules" only when the server is active) |
| 💡 | **`/announce` in-game command** — admin in-game command parity with the Discord panel announce button |

---

## BanSystem

### Database & Records
| Status | Feature |
|--------|---------|
| ✅ | Permanent & temporary bans, IP bans, linked UIDs |
| ✅ | Player session registry (PUID, name, IP, last-seen) |
| ✅ | Formal warnings with decay (`WarnDecayDays`) |
| ✅ | Ban templates (`/qban <slug>`) |
| ✅ | Scheduled ban registry |
| 💡 | **Ban reason changelog** — append an entry every time a ban reason is edited; visible in `/ban check` |
| 💡 | **Permanent mute** — no-expiry mute record type (currently only temp mutes) |
| 💡 | **IP range bans** — CIDR-style blocking (e.g. `192.168.1.0/24`) |
| 💡 | **Session stats in ban history** — show total hours played, first-seen, last-seen inside `/ban check` output |
| 💡 | **Evidence URL field on bans** — attach a screenshot/link URL to a ban entry (stored in JSON, shown in `/ban check`) |

### Automation
| Status | Feature |
|--------|---------|
| ✅ | Warn escalation tiers (`WarnEscalationTiers`) |
| ✅ | Auto-ban on warn threshold (`AutoBanWarnCount`) |
| ✅ | Chat-filter auto-warn (`ChatFilterAutoWarnThreshold`) |
| ✅ | Admin ban rate limiting |
| ✅ | GeoIP region blocking |
| 💡 | **Mute escalation tiers** — same concept as `WarnEscalationTiers` but triggers automatic mutes |
| 💡 | **Auto-warn on temp-ban expiry** — add a warning when a temp-ban expires so the escalation ladder still advances |
| 💡 | **Chat-spam detection** — auto-mute players sending > N messages in M seconds |
| 💡 | **VPN/proxy detection** — flag or auto-kick players whose IP resolves as a known hosting/VPN range via a free API |
| 💡 | **Scheduled bans** — set a ban to activate at a future time (e.g. during the next maintenance window) |

### REST API
| Status | Feature |
|--------|---------|
| ✅ | `GET /bans`, `POST /bans`, `DELETE /bans/:uid`, `GET /bans/check/:uid` |
| ✅ | `GET /players`, `POST /players/prune` |
| ✅ | `POST /bans/backup`, `POST /bans/prune` |
| ✅ | `X-Api-Key` authentication for mutating endpoints |
| 💡 | **`GET /warns/:uid` + `DELETE /warns/:id`** — warning management via API |
| 💡 | **`GET /mutes` + `DELETE /mutes/:uid`** — mute list via API |
| 💡 | **`POST /bans/import`** — bulk-import a JSON array of bans (migration from other systems) |
| 💡 | **`GET /stats`** — aggregate stats: total bans, active bans, warns issued this month, etc. |
| 💡 | **Webhook retry queue** — if the Discord webhook POST fails, queue and retry with backoff instead of silently dropping |

### Multi-Server
| Status | Feature |
|--------|---------|
| ✅ | Ban sync to peer servers via `PeerWebSocketUrls` |
| 💡 | **Cross-server session identity sync** — when a ban syncs to a peer, also sync the player's display name and session history |
| 💡 | **Sync status endpoint** — `GET /sync/status` showing last-sync timestamp and peer reachability |

---

## BanChatCommands

### Admin Commands
| Status | Feature |
|--------|---------|
| ✅ | `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist` |
| ✅ | `/warn`, `/warnings`, `/clearwarn`, `/clearallwarns` |
| ✅ | `/kick`, `/mute`, `/unmute`, `/tempmute` |
| ✅ | `/note`, `/notes`, `/staffchat`, `/mutelist`, `/mutecheck` |
| ✅ | `/playerhistory`, `/linkbans`, `/unlinkbans`, `/whoami` |
| ✅ | `/qban <slug>` quick-ban from templates |
| 💡 | **`/mutes` command** — list all currently active mutes (like `/banlist` but for mutes) |
| 💡 | **`/qwarn <slug> <player>`** — quick-warn from a warn-template preset |
| 💡 | **`/session <player>`** — show current session stats: time connected, build count |
| 💡 | **`/history export <player>`** — output full history (bans, warns, notes, sessions) as one formatted block |
| 💡 | **`/freeze <player>`** — prevent a player from moving without kicking them (investigation hold) |

### Permission Tiers
| Status | Feature |
|--------|---------|
| ✅ | Admin tier (`AdminEosPUIDs`) — full access |
| ✅ | Moderator tier (`ModeratorEosPUIDs`) — kick + modban only |
| ✅ | `MaxModMuteDurationMinutes` cap |
| 💡 | **Third permission tier** — "Helpers" with read-only commands only (`/bancheck`, `/warnings`, `/mutecheck`) |

### Logging & Notifications
| Status | Feature |
|--------|---------|
| ✅ | Discord webhook on ban/warn/kick |
| ✅ | `KickAddsWarning` flag |
| ✅ | `ReloadConfigWebhookUrl` on `/admin reloadconfig` |
| 💡 | **Kick Discord notification** — when `KickAddsWarning=True`, also post the kick to the webhook (currently only the warning posts) |
| 💡 | **`/admin audit [page]` command** — list recent moderation actions with admin name, action type, and target |

---

## DiscordBridge — Ticket System

### Core Ticket Flow
| Status | Feature |
|--------|---------|
| ✅ | Button panel with built-in types: whitelist / help / report / appeal |
| ✅ | Custom `TicketReason` types |
| ✅ | Reason modal before ticket creation |
| ✅ | Private channel per ticket, notify-role ping |
| ✅ | Close / reopen with grace period (reopen-once limit) |
| ✅ | Ban context + Approve/Deny buttons on appeal tickets |
| ✅ | Ticket macros and auto-responses |
| 💡 | **Ticket inactivity auto-close** — if no message for N hours, post a warning then close (configurable `TicketInactivityHours`) |
| 💡 | **Ticket assignment** — "Assign to me" button; claimed-by admin shown in channel topic |
| 💡 | **Ticket priority** — Low / Medium / High / Urgent buttons; changes channel name prefix |
| 💡 | **Ticket transcript** — on close, post full message history to a `TicketLogChannelId` as an embed |
| 💡 | **`/ticket stats` command** — open count, closed this week, average first-response time |
| 💡 | **Ticket escalation** — if SLA warning fires with no staff response, auto-ping a higher role |
| 💡 | **Report evidence field** — allow the reporter to paste a screenshot URL in the report modal; display it in the ticket embed |

---

## SMLWebSocket

### Current
| Status | Feature |
|--------|---------|
| ✅ | WebSocket server with live JSON event push |
| ✅ | `bPushEventsToWebSocket` toggle in BanSystem |

### Suggested
| Status | Feature |
|--------|---------|
| 💡 | **Event-type filtering** — clients subscribe to only the event types they need (e.g. `ban`, `warn`, skip `chat`) |
| 💡 | **Reconnect with exponential backoff** — instead of a fixed retry interval |
| 💡 | **Event replay buffer** — store last N events in memory; freshly connected clients can request recent history |
| 💡 | **Per-connection auth token** — clients pass a bearer token on connect; invalid tokens are rejected |
| 💡 | **Bi-directional commands** — client sends `{"action":"ban","uid":"...","reason":"..."}` and the server executes it |

---

## Cross-Cutting / Quality of Life

| Status | Feature |
|--------|---------|
| ✅ | `/admin reloadconfig` (Discord + in-game) |
| ✅ | Audit log (`BanAuditLog`) |
| ✅ | Admin panel embed with dashboard stats |
| 💡 | **Config hot-reload watcher** — watch config files for on-disk changes and reload automatically without a command |
| 💡 | **Backup health-check alert** — if the scheduled backup fails (disk full, permissions), post an alert to a Discord admin channel |
| 💡 | **Admin activity dashboard** — track which admins issued how many bans/warns/kicks this month; surface in `/admin stats` |
| 💡 | **In-game `/help` command** — paginated list of all available commands for the caller's permission level |
| 💡 | **`/player lookup <discord_id>` Discord command** — resolve a Discord user ID to their known EOS PUID / IP (reverse of `/ban check`) |
