# Features Roadmap

> **Last updated:** 2026-04-16  
> This file is maintained by Copilot and updated whenever you ask for a fresh suggestions pass.  
> ✅ **Server-side only** — no client download required for any feature in this file.

**Legend**

| Symbol | Meaning |
|--------|---------|
| ✅ | Already implemented |
| 🔨 | In progress |
| 🟢 **Now** | High-priority, implement next (Phase 1) |
| 🔵 **Next** | Implement after Phase 1 (Phase 2) |
| ⚪ **Later** | Nice-to-have, low urgency (Phase 3) |

Each suggested item also carries:
- **Impact:** `🔴 High` / `🟡 Med` / `⚪ Low`
- **Complexity:** `S` (small — hours) / `M` (medium — days) / `L` (large — week+)

---

## Implementation Phases

| Phase | Focus |
|-------|-------|
| **Phase 1 — Now** | DiscordBridge chat/status/AFK improvements + Ticket productivity |
| **Phase 2 — Next** | BanSystem API reliability + BanChatCommands QoL |
| **Phase 3 — Later** | SMLWebSocket advanced capabilities + cross-cutting polish |

---

## DiscordBridge

### Chat Bridge
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Two-way in-game ↔ Discord chat relay | — | — |
| ✅ | `%PlayerName%`, `%Message%`, `%ServerName%`, `%Role%` format placeholders | — | — |
| ✅ | Discord role labels resolved to `%Role%` placeholder | — | — |
| ✅ | Bot-message filtering (ignore other bots) | — | — |
| 🟢 **Now** | **Multi-channel relay** — mirror chat to multiple Discord channels simultaneously (e.g. one public + one private staff copy) | 🔴 High | S |
| 🟢 **Now** | **Keyword/phrase alerts** — if an in-game message contains a configured word or regex, ping a staff role in a private channel | 🔴 High | S |
| ⚪ **Later** | **Message editing sync** — when a Discord message is edited, post a follow-up notice in-game | ⚪ Low | M |

### Server Status & Presence
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Online / offline notifications with configurable messages | — | — |
| ✅ | Dedicated `StatusChannelId` for status messages | — | — |
| ✅ | Live player-count bot presence (configurable interval, activity type) | — | — |
| 🟢 **Now** | **`/server status` Discord command** — on-demand embed: uptime, player count, active bans, active mutes | 🔴 High | S |
| 🟢 **Now** | **Auto-post periodic server stats embed** — post the same embed on a configurable interval to a status channel | 🟡 Med | S |
| ✅ | Scheduled announcements (via `DefaultInGameMessages.ini`) | — | — |

### Player Notifications
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Join / leave / timeout notifications | — | — |
| ✅ | Dedicated `PlayerEventsChannelId` | — | — |
| ✅ | Private admin channel with EOS PUID + IP on join | — | — |
| 🔵 **Next** | **Rich join embed** — include session count, previous-ban flag, alt-account detection warning | 🔴 High | M |
| 🟢 **Now** | **AFK kick notification** — send an in-game message to the kicked player explaining why they were removed | 🔴 High | S |
| 🔵 **Next** | **Duplicate-name alert** — warn admins when a joining player's name closely matches a known banned player | 🟡 Med | M |

### Whitelist
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Whitelist enforcement on join | — | — |
| ✅ | Timed whitelist entries (`ExpiresAt`) | — | — |
| ✅ | Group/tier tags, `MaxSlots` capacity, partial-name search | — | — |
| ✅ | Whitelist audit log | — | — |
| 🟢 **Now** | **`/whitelist bulk` Discord command** — add multiple players at once via comma-separated list or modal | 🔴 High | S |
| 🟢 **Now** | **Discord role sync on whitelist add** — optionally assign a configurable Discord role when a player is whitelisted | 🟡 Med | S |
| 🔵 **Next** | **Whitelist ticket auto-action** — when a whitelist ticket is approved, automatically add the user to the whitelist | 🔴 High | M |

### AFK Kick
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | AFK detection via pawn-location delta (50 UU) + build-count delta | — | — |
| ✅ | Configurable AFK timeout | — | — |
| 🟢 **Now** | **AFK warning message** — send an in-game warning N minutes before the kick fires | 🔴 High | S |
| 🟢 **Now** | **AFK exemption list** — EOS PUIDs that are never AFK-kicked (e.g. trusted builders on long automation runs) | 🟡 Med | S |

### In-Game Messages
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Scheduled broadcast messages (`DefaultInGameMessages.ini`) | — | — |
| 🔵 **Next** | **Condition-based broadcasts** — only post a scheduled message if player count ≥ N | 🟡 Med | S |
| 🔵 **Next** | **`/announce` in-game command** — admin in-game command parity with the Discord panel announce button | 🟡 Med | S |

---

## BanSystem

### Database & Records
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Permanent & temporary bans, IP bans, linked UIDs | — | — |
| ✅ | Player session registry (PUID, name, IP, last-seen) | — | — |
| ✅ | Formal warnings with decay (`WarnDecayDays`) | — | — |
| ✅ | Ban templates (`/qban <slug>`) | — | — |
| ✅ | Scheduled ban registry | — | — |
| 🔵 **Next** | **Ban reason changelog** — append an entry every time a ban reason is edited; visible in `/ban check` | 🔴 High | M |
| 🔵 **Next** | **Evidence URL field on bans** — attach a screenshot/link URL to a ban entry (stored in JSON, shown in `/ban check`) | 🔴 High | S |
| ⚪ **Later** | **Permanent mute** — no-expiry mute record type (currently only temp mutes) | 🟡 Med | S |
| ⚪ **Later** | **IP range bans** — CIDR-style blocking (e.g. `192.168.1.0/24`) | 🟡 Med | M |
| ⚪ **Later** | **Session stats in ban history** — total hours played, first-seen, last-seen inside `/ban check` output | ⚪ Low | S |

### Automation
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Warn escalation tiers (`WarnEscalationTiers`) | — | — |
| ✅ | Auto-ban on warn threshold (`AutoBanWarnCount`) | — | — |
| ✅ | Chat-filter auto-warn (`ChatFilterAutoWarnThreshold`) | — | — |
| ✅ | Admin ban rate limiting | — | — |
| ✅ | GeoIP region blocking | — | — |
| 🔵 **Next** | **Mute escalation tiers** — same concept as `WarnEscalationTiers` but triggers automatic mutes | 🔴 High | M |
| 🔵 **Next** | **Chat-spam detection** — auto-mute players sending > N messages in M seconds | 🔴 High | M |
| ⚪ **Later** | **Auto-warn on temp-ban expiry** — add a warning when a temp-ban expires so the escalation ladder still advances | 🟡 Med | S |
| ⚪ **Later** | **VPN/proxy detection** — flag or auto-kick players whose IP resolves as a known VPN range via a free API | 🟡 Med | L |

### REST API
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | `GET /bans`, `POST /bans`, `DELETE /bans/:uid`, `GET /bans/check/:uid` | — | — |
| ✅ | `GET /players`, `POST /players/prune` | — | — |
| ✅ | `POST /bans/backup`, `POST /bans/prune` | — | — |
| ✅ | `X-Api-Key` authentication for mutating endpoints | — | — |
| 🔵 **Next** | **`GET /warns/:uid` + `DELETE /warns/:id`** — warning management via API | 🔴 High | S |
| 🔵 **Next** | **`GET /mutes` + `DELETE /mutes/:uid`** — mute list via API | 🔴 High | S |
| 🔵 **Next** | **`GET /stats`** — aggregate stats: total bans, active bans, warns issued this month, etc. | 🟡 Med | S |
| 🔵 **Next** | **Webhook retry queue** — if the Discord webhook POST fails, queue and retry with exponential backoff | 🔴 High | M |
| ⚪ **Later** | **`POST /bans/import`** — bulk-import a JSON array of bans (migration from other systems) | 🟡 Med | M |

### Multi-Server
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Ban sync to peer servers via `PeerWebSocketUrls` | — | — |
| 🔵 **Next** | **`GET /sync/status` endpoint** — last-sync timestamp and peer reachability | 🔴 High | S |
| ⚪ **Later** | **Cross-server session identity sync** — when a ban syncs to a peer, also sync the player's display name and session history | 🟡 Med | M |

---

## BanChatCommands

### Commands
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist` | — | — |
| ✅ | `/warn`, `/warnings`, `/clearwarn`, `/clearallwarns` | — | — |
| ✅ | `/kick`, `/mute`, `/unmute`, `/tempmute` | — | — |
| ✅ | `/note`, `/notes`, `/staffchat`, `/mutelist`, `/mutecheck` | — | — |
| ✅ | `/playerhistory`, `/linkbans`, `/unlinkbans`, `/whoami` | — | — |
| ✅ | `/qban <slug>` quick-ban from templates | — | — |
| 🔵 **Next** | **`/mutes` command** — list all currently active mutes (like `/banlist` but for mutes) | 🔴 High | S |
| 🔵 **Next** | **`/qwarn <slug> <player>`** — quick-warn from a warn-template preset | 🔴 High | S |
| 🔵 **Next** | **`/admin audit [page]`** — list recent moderation actions with admin name, action type, and target | 🔴 High | M |
| ⚪ **Later** | **`/session <player>`** — show current session stats: time connected, build count | 🟡 Med | S |
| ⚪ **Later** | **`/history export <player>`** — output full history (bans, warns, notes, sessions) as one formatted block | ⚪ Low | S |
| ⚪ **Later** | **`/freeze <player>`** — prevent a player from moving without kicking them (investigation hold) | 🟡 Med | L |

### Permission Tiers
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Admin tier (`AdminEosPUIDs`) — full access | — | — |
| ✅ | Moderator tier (`ModeratorEosPUIDs`) — kick + modban + limited mute | — | — |
| ✅ | `MaxModMuteDurationMinutes` cap | — | — |
| 🔵 **Next** | **Helper permission tier** — `HelperEosPUIDs` with read-only commands only (`/bancheck`, `/warnings`, `/mutecheck`) | 🔴 High | M |

### Logging & Notifications
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Discord webhook on ban/warn/kick | — | — |
| ✅ | `KickAddsWarning` flag | — | — |
| ✅ | `ReloadConfigWebhookUrl` on `/admin reloadconfig` | — | — |
| 🔵 **Next** | **Kick Discord notification** — when `KickAddsWarning=True`, also post the kick itself to the webhook | 🟡 Med | S |

---

## DiscordBridge — Ticket System

### Core Ticket Flow
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | Button panel with built-in types: whitelist / help / report / appeal | — | — |
| ✅ | Custom `TicketReason` types | — | — |
| ✅ | Reason modal before ticket creation | — | — |
| ✅ | Private channel per ticket, notify-role ping | — | — |
| ✅ | Close / reopen with grace period (reopen-once limit) | — | — |
| ✅ | Ban context + Approve/Deny buttons on appeal tickets | — | — |
| ✅ | Ticket macros and auto-responses | — | — |
| 🟢 **Now** | **Ticket inactivity auto-close** — if no message for N hours, post a warning then close (`TicketInactivityHours` config key) | 🔴 High | M |
| 🟢 **Now** | **Ticket assignment** — "Assign to me" button; claimed-by admin shown in channel topic | 🔴 High | M |
| 🟢 **Now** | **Ticket priority** — Low / Medium / High / Urgent buttons; changes channel name prefix | 🟡 Med | M |
| 🟢 **Now** | **Ticket transcript** — on close, post full message history to a `TicketLogChannelId` | 🔴 High | M |
| 🟢 **Now** | **`/ticket stats` command** — open count, closed this week, average first-response time | 🟡 Med | M |
| 🔵 **Next** | **Ticket escalation** — if SLA warning fires with no staff response, auto-ping a higher role | 🔴 High | M |
| 🔵 **Next** | **Report evidence field** — allow the reporter to paste a screenshot URL in the report modal | 🟡 Med | S |

---

## SMLWebSocket

### Implemented
| Status | Feature |
|--------|---------|
| ✅ | WebSocket server with live JSON event push |
| ✅ | `bPushEventsToWebSocket` toggle in BanSystem |
| ✅ | Auto-reconnect with exponential backoff and jitter |

### Suggested
| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| 🔵 **Next** | **Per-connection auth token** — clients pass a bearer token on connect; invalid tokens are rejected | 🔴 High | S |
| 🔵 **Next** | **Event-type filtering** — clients subscribe to only the event types they need (e.g. `ban`, `warn`, skip `chat`) | 🟡 Med | M |
| ⚪ **Later** | **Event replay buffer** — store last N events in memory; freshly connected clients can request recent history | 🟡 Med | M |
| ⚪ **Later** | **Bi-directional commands** — client sends `{"action":"ban","uid":"...","reason":"..."}` and the server executes it | 🟡 Med | L |

---

## Cross-Cutting / Quality of Life

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ✅ | `/admin reloadconfig` (Discord + in-game) | — | — |
| ✅ | Audit log (`BanAuditLog`) | — | — |
| ✅ | Admin panel embed with dashboard stats | — | — |
| 🔵 **Next** | **Admin activity dashboard** — track which admins issued how many bans/warns/kicks this month; surface in `/admin stats` | 🔴 High | M |
| 🔵 **Next** | **`/player lookup <discord_id>` Discord command** — resolve a Discord user ID to their known EOS PUID / IP | 🔴 High | S |
| ⚪ **Later** | **Config hot-reload watcher** — watch config files for on-disk changes and reload automatically without a command | 🟡 Med | M |
| ⚪ **Later** | **Backup health-check alert** — if the scheduled backup fails, post an alert to a Discord admin channel | 🟡 Med | S |
| ⚪ **Later** | **In-game `/help` command** — paginated list of all available commands for the caller's permission level | ⚪ Low | M |
