# Features Roadmap

> **Last updated:** 2026-04-17 (pass 8)  
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
| 🔵 **Next** | **Ban-evasion risk scoring** — on join, score the player using name-similarity to known bans + linked-UID count + prior session flags; post a risk-level embed to the admin channel for high scores | 🔴 High | M |
| 🔵 **Next** | **Automated false-positive review queue** — collect recently auto-muted / auto-warned cases into a Discord channel so staff can quickly approve or overturn them with buttons | 🔴 High | M |

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
| 🔵 **Next** | **Silent quarantine mode** — `/quarantine <player>` hides that player's chat from everyone else and blocks interactions without kicking; lifts with `/unquarantine` | 🔴 High | L |
| 🔵 **Next** | **`/caseexport <player>`** — bundle all evidence for a player (chat log excerpts, warns, notes, ban history, session timestamps) into one formatted Discord message for staff review | 🔴 High | M |

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
| 🔵 **Next** | **Ticket triage queue** — auto-route new tickets by type/severity to specific staff roles; show queue position and SLA breach countdown in the ticket channel topic | 🔴 High | M |

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
| 🔵 **Next** | **Moderator quality analytics** — extend admin activity dashboard with overturned-action rate, average response time, and consistency score per staff member | 🟡 Med | M |
| 🔵 **Next** | **Two-person approval for high-impact actions** — permabans, UID unlinks, and ban-reason edits require a second staff member to confirm via a Discord button within 5 minutes | 🟡 Med | M |
| ⚪ **Later** | **Config hot-reload watcher** — watch config files for on-disk changes and reload automatically without a command | 🟡 Med | M |
| ⚪ **Later** | **Backup health-check alert** — if the scheduled backup fails, post an alert to a Discord admin channel | 🟡 Med | S |
| ⚪ **Later** | **In-game `/help` command** — paginated list of all available commands for the caller's permission level | ⚪ Low | M |
| ⚪ **Later** | **Incident Mode toggle** — `/incidentmode on/off` instantly applies a strict preset (tighter spam thresholds, auto-mute on first offense, join cooldown) to counter raids; reverts on off | 🔴 High | M |
| ⚪ **Later** | **Config safety profiles** — named presets (e.g. `Normal`, `Strict`, `Event`) stored as config snapshots; `/profile apply <name>` switches instantly with one-step rollback | 🟡 Med | M |
| ⚪ **Later** | **Data integrity watchdog** — periodic validation of `bans.json`, `warnings.json`, and ticket state; posts a Discord alert if checksums mismatch or records are malformed | 🟡 Med | S |

---

## Extended Feature Backlog

> Items below were added in pass 3. All are **server-side only**.  
> † = a related item already exists in the sections above.

---

### Moderation & Enforcement

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Soft-ban** — kick player on join and deny re-entry for a configurable timed window without adding a full ban record | 🔴 High | M |
| ⚪ **Later** | **Account-age minimum** — reject joins from EOS accounts newer than N days (configured in BanBridgeConfig) | 🟡 Med | S |
| ⚪ **Later** | **Alt-account suspicion scoring** — weight name similarity, IP proximity, and session timing to produce an alt-likelihood score on join | 🔴 High | M |
| ⚪ **Later** | **Shared-IP watchlist** — automatically flag any player whose IP matches an IP associated with a current ban | 🔴 High | S |
| ⚪ **Later** | **Ban-evasion auto-flag** †— when suspicion score exceeds threshold, auto-create an admin-channel embed with ban/ignore/false-positive buttons | 🔴 High | M |
| ⚪ **Later** | **Progressive punishment profiles** — per-category config that automatically selects the next punishment type based on prior offense count | 🔴 High | M |
| ⚪ **Later** | **Auto-escalation by category** — separate escalation ladders per offense category (e.g. griefing, hate-speech, cheating) | 🟡 Med | M |
| ⚪ **Later** | **Strike expiry timers** — each individual warning carries its own TTL independent of the global `WarnDecayDays` | 🟡 Med | S |
| ⚪ **Later** | **Reputation / risk score per player** — running composite score updated on every offense and session; visible in `/bancheck` | 🟡 Med | M |
| ⚪ **Later** | **Silent staff notes visible in panel** — surface `/note` entries directly on the admin panel embed for the queried player | 🟡 Med | S |
| ⚪ **Later** | **Priority handling for repeat offenders** — if a joining player's risk score exceeds a threshold, post an immediate admin-channel alert ahead of the normal join embed | 🔴 High | S |
| ⚪ **Later** | **Mass-incident mode** †— automatically tighten all spam and auto-action thresholds when more than N rule violations fire within M minutes | 🔴 High | M |
| ⚪ **Later** | **Raid mode (join throttling)** — if more than N players join within M seconds, queue subsequent joins or reject with a friendly message | 🔴 High | M |
| ⚪ **Later** | **Freeze-chat mode** — `/freezechat on/off` halts all player-to-Discord and in-game chat relay without disconnecting anyone | 🟡 Med | S |
| ⚪ **Later** | **Temporary command lockdown** — `/cmdlock on/off` disables all non-admin chat commands during incidents | 🟡 Med | S |
| ⚪ **Later** | **Global mute window** — config schedule (e.g. 02:00–04:00) that auto-mutes all non-staff chat | ⚪ Low | S |
| ⚪ **Later** | **Staff action confirmation for high-risk actions** †— additional confirm modal for `/ban permanent` and `/unlinkbans` before execution | 🟡 Med | S |
| ⚪ **Later** | **Evidence attachments on punishments** — accept a screenshot URL parameter on `/ban`, `/warn`, `/kick`; stored in the record and shown in ban-check embeds | 🔴 High | S |
| ⚪ **Later** | **Reason templates with required fields** — ban/warn templates can declare required placeholders (e.g. `{location}`) that the admin must fill before the action executes | 🟡 Med | M |
| ⚪ **Later** | **Pre-ban impact preview** — before confirming `/ban permanent`, show how many linked UIDs and active sessions will be affected | 🟡 Med | M |

---

### Anti-Spam & Anti-Abuse

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Duplicate message detection** — auto-warn a player who sends the same (or near-identical) message within a configurable window | 🔴 High | S |
| ⚪ **Later** | **Fuzzy/similarity spam detection** — use edit-distance to catch messages that are slight variations of a blocked phrase | 🔴 High | M |
| ⚪ **Later** | **Link cooldown per player** — a player may only post a URL once every N seconds in chat | 🟡 Med | S |
| ⚪ **Later** | **Mention/tag abuse limiter** — auto-mute if a player @-mentions more than N Discord users in a short window | 🟡 Med | S |
| ⚪ **Later** | **Emoji/symbol spam limiter** — mute if a message contains more than N% non-letter characters | 🟡 Med | S |
| ⚪ **Later** | **Caps ratio limiter** — auto-warn when a message is more than a configurable percentage upper-case | ⚪ Low | S |
| ⚪ **Later** | **Repeated join/leave abuse detection** — track join/leave frequency; auto-ban players who exceed a threshold to prevent reconnect-spam | 🔴 High | S |
| ⚪ **Later** | **Command spam limiter** — rate-limit in-game command usage per player; excess attempts are silently dropped | 🟡 Med | S |
| ⚪ **Later** | **Auto slow-mode by channel** — reduce in-game chat relay speed for a player based on recent violation count | ⚪ Low | S |
| ⚪ **Later** | **Escalating anti-spam penalties** — first offense: warn; second: short mute; third: long mute; fourth: ban — configured as a sub-ladder | 🔴 High | M |
| ⚪ **Later** | **Anti-advertising keyword packs** — loadable keyword lists per category (domain names, social handles, competitor names) in config | 🟡 Med | S |
| ⚪ **Later** | **Invite/code pattern blocker** — regex to detect and auto-remove Discord invite links and referral codes from chat | 🟡 Med | S |
| ⚪ **Later** | **Burst-message quarantine** — if a player sends > N messages in M seconds, shadow-drop their messages without alerting them | 🔴 High | M |
| ⚪ **Later** | **Reputation-based chat limits** — new players (< X sessions) have stricter chat-spam thresholds than veterans | 🟡 Med | M |
| ⚪ **Later** | **First-message moderation mode** — hold the very first in-game message from a new player for staff review before relaying to Discord | 🟡 Med | M |

---

### Tickets & Appeals

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Multi-step appeal form wizard** — guide the appellant through 3–4 modal steps (what happened, what changed, any evidence) instead of a single free-text box | 🟡 Med | M |
| ⚪ **Later** | **Appeal eligibility cooldown** — prevent a player from submitting a second appeal within N days of a denial | 🔴 High | S |
| ⚪ **Later** | **"New evidence" re-open flow** — a closed-appeal ticket can be re-opened once via a Discord button only if the player provides a URL or description of new evidence | 🟡 Med | M |
| ⚪ **Later** | **Appeal SLA timers** †— track time since appeal was opened; post escalation ping if no staff response within a configured window | 🔴 High | M |
| ⚪ **Later** | **Auto-reminder pings for stale appeals** — daily reminder to the ticket channel if no staff activity in N hours | 🟡 Med | S |
| ⚪ **Later** | **Appeal triage tags** — add severity/type emoji tags to the ticket channel name on creation for quick visual scanning | ⚪ Low | S |
| ⚪ **Later** | **Escalate-to-senior-mod button** — a button in the ticket that re-pings a higher staff role when a moderator cannot resolve the case | 🔴 High | S |
| ⚪ **Later** | **Mediation ticket type** — a dedicated ticket category for player-vs-player disputes with a structured evidence-upload flow | 🟡 Med | M |
| ⚪ **Later** | **Staff handoff notes in ticket** — a dedicated `/ticketnote` command that posts a styled embed visible only to staff inside the ticket channel | 🟡 Med | S |
| ⚪ **Later** | **Merge duplicate tickets** — `/ticketmerge <id1> <id2>` collapses two open tickets into one channel, linking both | 🟡 Med | M |
| ⚪ **Later** | **Ticket conflict-of-interest flagging** — if the staff member handling a ticket is the same admin who issued the ban being appealed, auto-post a warning embed | 🔴 High | S |
| ⚪ **Later** | **Private internal staff thread per ticket** — auto-create a private Discord thread inside each ticket channel for staff-only discussion | 🟡 Med | S |
| ⚪ **Later** | **Post-resolution satisfaction prompt** — on ticket close, post a DM-style embed asking the user to rate the experience (👍 / 👎) | ⚪ Low | S |
| ⚪ **Later** | **Auto-close with reopen token** — inactive tickets auto-close but generate a one-time token the player can use to reopen within 48 hours | ⚪ Low | M |
| ⚪ **Later** | **Appeal outcome analytics dashboard** — embed in a staff channel showing monthly appeal volume, approval rate, and average time-to-decision | 🟡 Med | M |

---

### Discord & Admin Panel UX

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Saved quick actions** — per-admin button presets in the panel (e.g. "7-day ban + warn template #3") triggered with one click | 🟡 Med | M |
| ⚪ **Later** | **"Recent offenders" widget** — panel section listing the last 5 players who received any punishment, with a one-click drill-down | 🟡 Med | M |
| ⚪ **Later** | **One-click incident bundles** — panel button that simultaneously applies kick + 24-hr mute + note in a single action | 🔴 High | M |
| ⚪ **Later** | **Live online player drilldown** — clicking the "Players" panel button opens a paginated embed listing all online players with ban/warn/note indicators | 🔴 High | M |
| ⚪ **Later** | **Search by linked identifier** — `/panel search <discord_id|ip|name>` returns all matching player records | 🔴 High | M |
| ⚪ **Later** | **Bulk action queue** — select multiple players from the player list embed and apply the same action to all at once | 🟡 Med | L |
| ⚪ **Later** | **Undo window for non-destructive actions** — for mutes and kicks, a 30-second "Undo" button appears in the admin channel after the action fires | 🟡 Med | M |
| ⚪ **Later** | **Action audit inline in panel** — panel embed footer shows the last 3 actions taken via that panel instance | ⚪ Low | S |
| ⚪ **Later** | **Smart action suggestions** — after a `/bancheck`, the panel auto-suggests the statistically most common next action for that player's profile | ⚪ Low | L |
| ⚪ **Later** | **Context-aware action enable/disable** — panel buttons that don't apply to the current state (e.g. "Unmute" when player is not muted) render as greyed-out | 🟡 Med | M |
| ⚪ **Later** | **Panel rate-limit indicator** — panel embed shows time remaining on the 60-second rate limit per user | ⚪ Low | S |
| ⚪ **Later** | **Panel macro editor** — Discord `/panelmacro add <name> <json_action_chain>` lets admins define custom multi-step panel macros | 🟡 Med | L |
| ⚪ **Later** | **Shift handover summary card** — `/shiftend` command posts an embed summarising all actions taken in the last N hours by the current admin | 🟡 Med | M |

---

### Ban / Mute / Warn Intelligence

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Warn clustering by behavior type** — group a player's warnings by category and surface the dominant category in `/bancheck` | 🟡 Med | S |
| ⚪ **Later** | **Offense heatmap by time/day** — `/stats heatmap` embed showing which hours and days see the most violations | ⚪ Low | M |
| ⚪ **Later** | **Auto-suggest punishment length** — based on a player's offense history and category, the ban modal pre-fills a suggested duration | 🟡 Med | M |
| ⚪ **Later** | **False-positive review queue** †— Discord channel fed by a queue of recent auto-actions with one-click approve/overturn buttons for staff | 🔴 High | M |
| ⚪ **Later** | **Mute evasion detection** — detect when a muted player reconnects under a different name or PUID and re-apply the mute | 🔴 High | M |
| ⚪ **Later** | **Ban effectiveness tracking** — record whether a temp-banned player returned and reoffended; surfaced in monthly digest | 🟡 Med | M |
| ⚪ **Later** | **Recidivism trend reports** — embed showing the percentage of players who reoffend within 30 / 60 / 90 days of a ban expiry | 🟡 Med | M |
| ⚪ **Later** | **Category-specific decay rules** — override `WarnDecayDays` per offense category in config | 🟡 Med | S |
| ⚪ **Later** | **Region/timezone abuse trend report** — surface which GeoIP regions and UTC hours correlate with most violations | ⚪ Low | M |
| ⚪ **Later** | **Trigger phrase impact report** — show which configured chat-filter phrases fire most often and how many auto-warns they produced | ⚪ Low | S |

---

### Whitelist & Access Control

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Temporary event whitelists** — a separate overlay whitelist active only during a configured date/time window | 🟡 Med | M |
| ⚪ **Later** | **Role-based whitelist tiers** †— map Discord roles to whitelist tier tags so role changes automatically adjust access level | 🟡 Med | M |
| ⚪ **Later** | **Invite quota per trusted member** — each whitelisted player can vouch for up to N others; vouches are tracked and can be revoked | 🟡 Med | M |
| ⚪ **Later** | **Auto-expiring invites** — generate a one-time join token with a TTL; joining with the token auto-whitelists the player | 🟡 Med | M |
| ⚪ **Later** | **Application-based whitelist approvals** †— ticket submission auto-creates a whitelist-pending record; approval via Discord button adds to whitelist | 🔴 High | M |
| ⚪ **Later** | **Waitlist when full** — when `MaxSlots` is reached, new applicants are added to an ordered waitlist and notified when a slot opens | 🟡 Med | M |
| ⚪ **Later** | **Priority slots by tier** — reserve a configurable number of whitelist slots exclusively for higher-tier members | ⚪ Low | S |
| ⚪ **Later** | **Geo-aware access policies** — optionally restrict whitelist approvals to players from configured GeoIP regions | 🟡 Med | S |
| ⚪ **Later** | **Seasonal whitelist resets** — `/whitelist reset` archives the current list to a backup file and starts fresh, useful for wipe events | 🟡 Med | S |
| ⚪ **Later** | **Whitelist abuse detection** — flag accounts that were whitelisted, banned, removed, and re-whitelisted more than once | 🟡 Med | S |

---

### Staff Operations

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **On-call rotation reminders** — post a Discord ping to the on-call role at the start of each configured shift window | ⚪ Low | S |
| ⚪ **Later** | **Auto-assign incidents by load** — when a ticket or auto-flag fires and a staff member has fewer than N open items, auto-assign it to them | 🟡 Med | M |
| ⚪ **Later** | **Staff performance metrics (private)** — per-admin stats (actions taken, tickets closed, response times) visible only to senior staff | 🟡 Med | M |
| ⚪ **Later** | **Action peer-review mode** — admins below a threshold action count have their first N punishments require a peer approval | 🟡 Med | M |
| ⚪ **Later** | **Required second approval for permanent bans** †— extends existing two-person approval to cover all permanent bans regardless of the acting admin's level | 🔴 High | S |
| ⚪ **Later** | **Shift notes & continuity log** — `/shiftnote <text>` appends a timestamped note to a running shift log in a private staff channel | 🟡 Med | S |
| ⚪ **Later** | **Incident playbooks / checklists** — configurable step-by-step checklists that auto-post to a ticket or incident channel on creation (e.g. raid playbook) | 🟡 Med | M |
| ⚪ **Later** | **Staff training sandbox commands** — a `/sandbox` mode that runs punishment commands against a test player record without actually executing them | 🟡 Med | M |
| ⚪ **Later** | **Anonymous internal case discussion** — `/caseopinion <ticket_id> <text>` posts to the internal staff thread without attributing the author | ⚪ Low | M |
| ⚪ **Later** | **Queue-load burnout signal** — if any staff member has > N open items for > M hours, post a quiet heads-up to senior staff | ⚪ Low | S |

---

### Player Safety & Community

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Safety keyword escalation** — a separate keyword list that immediately creates a ticket and pings senior staff on match | 🔴 High | S |
| ⚪ **Later** | **Harassment pattern tracking** — detect when Player A repeatedly targets Player B in chat or reports and auto-flag the pattern | 🔴 High | M |
| ⚪ **Later** | **Victim-protection watch flags** — manually tag a player as a protection subject; any future reports or chat involving them auto-escalate | 🔴 High | S |
| ⚪ **Later** | **Repeat-target detection** — surface in `/bancheck` if the target player appears in an unusual number of reports as the accused | 🟡 Med | S |
| ⚪ **Later** | **Auto-safe mode for new players** — stricter chat filtering and a grace period for first-session players before full auto-warn thresholds apply | 🟡 Med | M |
| ⚪ **Later** | **Community health score** — rolling aggregate metric (violations per session, appeal overturn rate, chat toxicity rate) posted weekly to a staff channel | ⚪ Low | M |
| ⚪ **Later** | **Civility reward system** — server-side tracking of sessions without any violation; after N clean sessions a "Trusted" tag is appended to the player's join embed | ⚪ Low | M |
| ⚪ **Later** | **Positive contribution log** — `/commend <player> <reason>` adds a staff-only note of good behaviour visible alongside warn history | ⚪ Low | S |
| ⚪ **Later** | **Newcomer mentoring queue** — new-join embed for players with < 3 sessions includes a "Assign mentor" button that notifies a volunteer staff role | ⚪ Low | M |
| ⚪ **Later** | **Conflict de-escalation prompts** — if two players are involved in repeated mutual reports, auto-post a moderation note to both suggesting they contact staff | 🟡 Med | M |

---

### Logging, Auditing & Compliance

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Immutable audit hash chain** — each audit-log entry includes the SHA-256 hash of the previous entry so tampering is detectable | 🔴 High | M |
| ⚪ **Later** | **Tamper-alert on log alteration** — periodic integrity check compares stored hashes; posts a Discord alert if any mismatch is found | 🔴 High | S |
| ⚪ **Later** | **Per-action evidence snapshotting** — when a ban or warn fires, snapshot the player's last N chat lines and attach to the audit record | 🟡 Med | M |
| ⚪ **Later** | **Redaction tools for sensitive logs** — `/auditredact <entry_id>` replaces PII in a specific audit entry with `[REDACTED]` and logs the redaction itself | 🟡 Med | M |
| ⚪ **Later** | **Audit export bundles by case** — `/caseexport <player>` packages all audit entries, session records, tickets, and notes into a single formatted Discord message or JSON file | 🔴 High | M |
| ⚪ **Later** | **Case timeline generator** — a chronological embed showing every interaction (join, message, warn, ban, appeal, unban) for a player on one scrollable view | 🔴 High | M |
| ⚪ **Later** | **Cross-mod correlation logs** — join a BanSystem event, a TicketSubsystem event, and a DiscordBridge event for the same player under a single case-correlation ID | 🟡 Med | M |
| ⚪ **Later** | **Structured JSON audit streams** — push structured audit events to a configurable HTTP endpoint for ingestion into external SIEM or logging tools | 🟡 Med | M |
| ⚪ **Later** | **Long-term archival policy controls** — config keys for `AuditRetainDays` and `SessionRetainDays`; prune jobs honour them separately | ⚪ Low | S |
| ⚪ **Later** | **Audit query filter presets** — named config filters (`+FilterPreset=name|fields…`) so `/auditquery <preset>` instantly surfaces a curated view | ⚪ Low | M |

---

### Automation & Integrations

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Webhook-based incident triggers** †— configurable outbound webhooks that fire on specific event types (ban, raid mode on, etc.) for external automation | 🟡 Med | M |
| ⚪ **Later** | **External SIEM forwarding** — structured JSON event push to a configurable TCP/HTTP endpoint alongside the WebSocket stream | 🟡 Med | M |
| ⚪ **Later** | **Auto-backup verification checks** — after each scheduled backup completes, parse and validate the output file and post a pass/fail notice | 🟡 Med | S |
| ⚪ **Later** | **Scheduled health self-tests** — periodic self-test sequence that verifies Discord API connectivity, WebSocket listener, and database read/write; posts result to a status channel | 🟡 Med | M |
| ⚪ **Later** | **Rule config validation linter** — on startup and on `/admin reloadconfig`, validate all config files against a schema and surface any misconfiguration as a Discord embed | 🔴 High | M |
| ⚪ **Later** | **Canary mode for new rules** — mark an escalation rule or chat filter as `Canary=true`; it logs matches and suggests actions without executing them | 🟡 Med | M |
| ⚪ **Later** | **Feature flags per mod subsystem** — `bFeatureEnabled_<FeatureName>=true/false` config keys allow disabling individual subsystem features without redeploying | 🟡 Med | M |
| ⚪ **Later** | **Auto-rollback on error spikes** — if more than N errors are logged per minute, revert the active config to the last known-good snapshot and alert admins | 🔴 High | L |
| ⚪ **Later** | **Versioned config snapshots** — every `/admin reloadconfig` archives the previous config with a timestamp; `/configrestore <timestamp>` reverts | 🟡 Med | M |
| ⚪ **Later** | **One-click restore points** — panel button to instantly restore the last config snapshot in an emergency | 🟡 Med | S |

---

### Analytics & Reporting

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Weekly moderation digest** — auto-posted embed every Monday: bans, warns, kicks, tickets opened/closed, top offenses | 🔴 High | M |
| ⚪ **Later** | **Monthly trend report** — longer-horizon digest comparing this month vs. last month across all moderation metrics | 🟡 Med | M |
| ⚪ **Later** | **Top rule violations report** — ranked embed of which chat-filter phrases and ban categories triggered the most actions this month | 🟡 Med | S |
| ⚪ **Later** | **Time-to-resolution stats** — average and P95 time from ticket open to close; surfaced in `/ticket stats` | 🟡 Med | S |
| ⚪ **Later** | **Appeal overturn rate tracking** — percentage of bans overturned on appeal; shown in monthly digest | 🟡 Med | S |
| ⚪ **Later** | **Staff workload balancing report** — show which staff members handled the most and fewest cases; useful for scheduling | 🟡 Med | M |
| ⚪ **Later** | **Churn-after-punishment analysis** — track whether punished players return; post as a retention metric in the monthly digest | ⚪ Low | M |
| ⚪ **Later** | **Peak toxicity windows report** — heatmap of which hours/days have the most auto-actions; helps staff scheduling | ⚪ Low | M |
| ⚪ **Later** | **New vs veteran behaviour comparison** — side-by-side embed of violation rates for players with < 10 sessions vs ≥ 10 sessions | ⚪ Low | M |
| ⚪ **Later** | **"What changed this week" insights** — diff between this week's and last week's key metrics; highlights unusual spikes | 🟡 Med | M |

---

### Performance & Reliability

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Queue backpressure controls** — if the outbound Discord API queue exceeds N items, drop low-priority messages and alert via log | 🔴 High | M |
| ⚪ **Later** | **Retry / dead-letter for failed actions** — failed ban/warn webhook posts are retried with exponential backoff; after N retries, moved to a dead-letter log | 🔴 High | M |
| ⚪ **Later** | **Graceful degraded mode** — if Discord API is unreachable, all in-game moderation commands continue to function; Discord-dependent features queue locally | 🔴 High | M |
| ⚪ **Later** | **Subsystem circuit breakers** — each subsystem tracks consecutive failures; after a threshold, it trips offline and retries on a longer interval | 🔴 High | M |
| ⚪ **Later** | **Config hot-reload safety checks** †— validate the incoming config before applying; abort and alert if validation fails rather than applying a broken config | 🔴 High | S |
| ⚪ **Later** | **Duplicate event suppression** — de-duplicate Discord notifications within a short window using a content hash so the same event never posts twice | 🟡 Med | S |
| ⚪ **Later** | **Idempotent action keys** — each punishment carries a UUID; re-submitted duplicate actions with the same key are silently dropped | 🟡 Med | S |
| ⚪ **Later** | **Crash recovery replay log** — on restart, re-apply any actions that were enqueued but not confirmed before the previous shutdown | 🔴 High | M |
| ⚪ **Later** | **Background task watchdogs** — each periodic ticker (AFK check, backup, prune) reports its last-run time; a watchdog posts an alert if any ticker misses two consecutive runs | 🟡 Med | S |
| ⚪ **Later** | **Metrics & alert thresholds** — configurable keys for queue depth, error rate, and API latency; breaching a threshold posts to a monitoring Discord channel | 🟡 Med | M |

---

### Power Features & Intelligence

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Policy simulator** — `/simulate rule:<name> player:<id>` shows what actions would have fired for a player under a proposed rule without executing them | 🟡 Med | L |
| ⚪ **Later** | **AI-assisted reason drafting** — generate a suggested ban/warn reason from a bullet-point staff note; staff must approve before submission | ⚪ Low | L |
| ⚪ **Later** | **Incident auto-summary generator** — when a ticket closes or a ban is issued, auto-generate a one-paragraph case summary from the audit trail | ⚪ Low | L |
| ⚪ **Later** | **Similar-case lookup** — `/similarcases <player>` searches the audit log for historical cases with matching offense category and player profile | 🟡 Med | M |
| ⚪ **Later** | **Rule conflict detector** — on config load, check for escalation rules or filter entries that contradict each other and post a warning | 🟡 Med | M |
| ⚪ **Later** | **Policy A/B testing** — run two chat-filter rule variants on alternating sessions and compare auto-action rates in the weekly digest | ⚪ Low | L |
| ⚪ **Later** | **Dynamic rules by server state** — escalation thresholds automatically tighten during peak hours or when player count exceeds N | 🟡 Med | M |
| ⚪ **Later** | **Seasonal moderation profiles** †— extend config safety profiles with a calendar schedule so the server automatically switches to Event / Wipe-Day / Normal profiles | 🟡 Med | M |
| ⚪ **Later** | **Event-mode templates** — `/eventmode <name>` applies a named profile (launch day, wipe day, tournament) that bundles config overrides, announcement messages, and staff pings | 🔴 High | M |
| ⚪ **Later** | **Unified command centre across all mods** — a single Discord `/admin` slash command with subcommands that routes to BanSystem, DiscordBridge, BanChatCommands, and TicketSubsystem without needing separate channel commands | 🔴 High | L |

---

## Extended Feature Backlog (Pass 4)

> Items below were added in pass 4. All are **server-side only**.  
> † = a related item already exists in the sections above.

---

### Identity & Risk Detection

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Device-fingerprint risk tagging** — on join, parse available device/client fingerprint fields and compare against known banned device signatures; flag high-similarity matches to the admin channel | 🔴 High | M |
| ⚪ **Later** | **Unicode confusable name detection** — flag player names containing Unicode lookalike characters (e.g. Cyrillic `а` vs Latin `a`) used to evade name bans; post a warning embed and optionally block the join | 🔴 High | S |
| ⚪ **Later** | **Auto-flag suspicious name changes** — detect when a returning player's display name differs significantly from all previous sessions and post an alert embed with old/new name and PUID | 🔴 High | S |
| ⚪ **Later** | **Staff player watchlist with join alerts** — `/watch <player> <reason>` adds a PUID to a configurable watchlist; the moment that player joins, an alert embed is posted to the admin channel with the watch reason | 🔴 High | S |

---

### Moderation Intelligence

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Mod note privacy levels** — notes tagged `staff` are visible to all staff tiers; notes tagged `lead` are visible only to the lead-admin tier; enforced in `/notes` and `/bancheck` output | 🟡 Med | S |
| ⚪ **Later** | **One-click repeat-offender bundle** — panel button (or `/repeatoffender <player>`) that pulls the last 5 punishments plus all notes for a player into a single formatted embed for quick review | 🔴 High | S |
| ⚪ **Later** | **Role-based command cooldowns** — per-command cooldown that varies by permission tier (e.g. moderators have a 5-minute cooldown on `/kick`, admins have 30 seconds) | 🟡 Med | S |
| ⚪ **Later** | **Scoped permissions by command + context** — extend the permission config to allow granting a command only in specific Discord channels or only against players below a configured risk score | 🟡 Med | M |
| ⚪ **Later** | **Secure staff command aliases** — config-defined short aliases for long commands (e.g. `/b` → `/ban`) that are only active for admin-tier users; aliases are logged the same as the full command | ⚪ Low | S |

---

### Ticket & Appeal Enhancements

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Appeal prioritization queue** — sort open appeals by ban severity and recency; surface the highest-priority appeals at the top of a staff dashboard embed in the ticket channel | 🔴 High | M |
| ⚪ **Later** | **Ticket ownership lock + takeover** — once a ticket is claimed, other staff see an "Override Claim" button instead of "Assign"; takeover events are audit-logged with the reason | 🟡 Med | S |
| ⚪ **Later** | **Ticket category tags** — visual emoji/label prefix added to the ticket channel name on creation based on type and severity (e.g. 🔴 for urgent, ⚠️ for harassment) for quick visual scanning | ⚪ Low | S |
| ⚪ **Later** | **Transcript export to JSON/CSV** — `/ticketexport <id>` posts a structured JSON or CSV file of the full ticket message history to a configured staff-log channel | 🟡 Med | M |
| ⚪ **Later** | **Ticket reopen reason requirement** — require the user to submit a short text reason via modal before a closed ticket can be reopened; reason is appended to the ticket transcript | 🟡 Med | S |
| ⚪ **Later** | **Per-category required form fields** — ticket category config supports declaring extra required form fields beyond the default reason box (e.g. report tickets require an accused player name) | 🟡 Med | M |
| ⚪ **Later** | **Toxicity flag in closed transcript** — when a ticket is closed, scan the transcript for configured phrases and append a summary note to the transcript log embed if any matches are found | 🟡 Med | M |
| ⚪ **Later** | **"Needs senior review" ticket status** — a button inside the ticket that sets a visual status flag and pings the senior-staff role without transferring ownership | 🔴 High | S |
| ⚪ **Later** | **Appeal evidence attachment index** — track all URLs submitted across a ticket's history in a pinned index message that is automatically updated each time a new link is posted | 🟡 Med | S |
| ⚪ **Later** | **Staff response quality checklist** — configurable checklist (e.g. "☐ Greeted user ☐ Read ban context ☐ Checked prior history") posted to the internal staff thread automatically when a ticket is opened | ⚪ Low | S |

---

### Audit, Analytics & Reporting

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Top recurring offenders leaderboard** — weekly embed listing the 10 players with the most combined punishments over the period; posted automatically to the staff channel | 🟡 Med | S |
| ⚪ **Later** | **Action reversal report** — monthly embed tracking how many bans/mutes/warns were reversed (unban/unmute/clearwarn), the reversal rate, and which admin performed each reversal | 🟡 Med | S |
| ⚪ **Later** | **Category-specific incident spike alerts** — if violations in a single offense category exceed N within M minutes, immediately post a Discord alert to the admin channel with a count and breakdown | 🔴 High | S |
| ⚪ **Later** | **Config drift detection** — compare the running config hash to the last saved hash on each reload; post an alert embed if they diverge (e.g. file edited on disk without a reload command) | 🟡 Med | S |
| ⚪ **Later** | **Policy-change impact tracking** — when a ban template or escalation rule is added/modified, auto-post a diff embed to a staff changelog channel showing what changed and who triggered the reload | 🟡 Med | S |
| ⚪ **Later** | **Staff action anomaly detection** †— alert senior staff if any admin's action count spikes dramatically above their personal baseline within a short window (beyond the existing rate limit) | 🔴 High | M |
| ⚪ **Later** | **Daily moderation digest** †— condensed one-liner-per-metric digest (bans, warns, kicks, tickets, auto-actions) posted each day to a staff channel; complements the existing weekly digest | 🟡 Med | S |

---

### Whitelist & Access Control

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Whitelist conflict checker** — on add, check if the player already exists under a different name or EOS PUID and surface a warning embed before creating a potentially duplicate entry | 🟡 Med | S |
| ⚪ **Later** | **Expiry-notice channel post** — N days before a timed whitelist entry expires, post a reminder embed to the configured whitelist channel including the player's Discord ID and tier | 🟡 Med | S |
| ⚪ **Later** | **Group-based slot caps** — each group/tier tag can declare its own MaxSlots independent of the global cap (e.g. max 20 Builder tier entries regardless of total server limit) | 🟡 Med | S |
| ⚪ **Later** | **Priority login reservation** — reserve a configurable number of whitelist slots exclusively for staff and booster tiers that are never consumed by regular whitelist entries | 🟡 Med | S |
| ⚪ **Later** | **Emergency whitelist lockdown toggle** — `/whitelist lock` instantly switches the server to whitelist-only mode; `/whitelist unlock` reverts; the locked state survives restarts | 🔴 High | S |
| ⚪ **Later** | **Rule-based whitelist auto-approval** — config rules that automatically approve whitelist applications matching criteria (e.g. required Discord role + account age > N days) without manual review | 🟡 Med | M |
| ⚪ **Later** | **Whitelist expiry reminder pings** †— configurable number of days before expiry at which a reminder is posted; supports multiple reminder thresholds (e.g. 7 days and 1 day) | 🟡 Med | S |
| ⚪ **Later** | **Self-service whitelist status command** — `/wlstatus` in-game command lets a player check their own tier, expiry date, and group without contacting staff | ⚪ Low | S |
| ⚪ **Later** | **Whitelist probation tier** — a dedicated tier that allows server join but automatically applies stricter chat/spam thresholds for the duration of the probation window | 🟡 Med | M |
| ⚪ **Later** | **Startup whitelist validation report** — on server start, validate all whitelist entries for expired records, duplicate PUIDs, and missing display names; post a summary report to the admin channel | 🟡 Med | S |
| ⚪ **Later** | **Whitelist audit discrepancy scanner** — periodic background job that cross-checks the whitelist JSON against the player session registry for orphaned or conflicting records and posts discrepancies | 🟡 Med | S |
| ⚪ **Later** | **Tier migration tool** — `/whitelist migrate <player> <new_tier>` moves a player between tiers, updates all linked records, and creates a full audit-log entry for the change | 🟡 Med | S |

---

### Staff Operations

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Live incident command room** — `/incidentroom create <name>` creates a temporary private Discord channel with a pre-pinned incident playbook and auto-invites all currently online staff roles | 🔴 High | M |
| ⚪ **Later** | **Moderator reputation / trust score** — internal score incremented on clean ticket resolutions and decremented on overturned actions; visible in the private staff metrics dashboard and used to gate peer-review requirements | 🟡 Med | M |
| ⚪ **Later** | **Post-incident retrospective template** — `/retro <ticket_id|ban_id>` posts a structured retrospective embed to the staff channel pre-filled with case details (timeline, actions taken, outcome) | 🟡 Med | S |

---

## Extended Feature Backlog (Pass 5)

> Added 2026-04-16. All items are ⚪ **Later** (unscheduled). Server-side only — no client download required.

### Moderation / Enforcement

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Account age gate** — block or restrict joining accounts below a configurable minimum age; post an automated DM explaining the requirement | 🔴 High | S |
| ⚪ **Later** | **Progressive punishment ladder per offense type** — separate escalation paths for chat abuse, griefing, and exploiting that advance independently | 🔴 High | M |
| ⚪ **Later** | **Repeat-offender heat score** — decaying numeric score incremented on every recorded offense; gates harsher defaults once thresholds are crossed | 🔴 High | M |
| ⚪ **Later** | **Auto-silence for spam bursts** — if a player sends N messages within T seconds, automatically mute for a configurable short duration without staff intervention | 🔴 High | S |
| ⚪ **Later** | **Link / domain blacklist with categories** — configurable per-category domain deny list (ads, phishing, NSFW); violations produce a warn or auto-mute | 🔴 High | S |
| ⚪ **Later** | **Regex-based slur / abuse filters** — configurable regex patterns matched against chat; hits trigger warn, mute, or delete depending on severity tier | 🔴 High | S |
| ⚪ **Later** | **Similarity spam detection** — detect copy-paste variant spam using edit-distance or trigram similarity and auto-suppress within the burst window | 🟡 Med | M |
| ⚪ **Later** | **Mass mention / ping abuse detection** — flag or auto-mute players who mention N or more unique users in a short window | 🟡 Med | S |
| ⚪ **Later** | **Raid-mode toggle** — `/raidmode on/off` applies a stricter rule preset (e.g. no new joins, chat throttled) and posts an alert embed to the admin channel | 🔴 High | S |
| ⚪ **Later** | **Alt-account suspicion scoring** — cross-reference IP range, EOS PUID history, and join timing to surface probable alt accounts with a confidence score | 🔴 High | L |
| ⚪ **Later** | **Shared punishment notes across linked IDs** — when a ban or note is added to any linked account, automatically attach a reference note to all other accounts in the group | 🔴 High | M |
| ⚪ **Later** | **Auto-kick on rapid reconnects** — kick and temp-ban players who reconnect more than N times within T minutes (crash-loop exploit mitigation) | 🟡 Med | S |
| ⚪ **Later** | **Duplicate-name impersonation detection** — on join, compare the display name against all banned players using fuzzy matching; alert staff if similarity exceeds threshold | 🔴 High | M |
| ⚪ **Later** | **Caps-lock flood penalty** — auto-warn when a message exceeds a configurable all-caps ratio and length | ⚪ Low | S |
| ⚪ **Later** | **Cooldown per command category** — separate per-player cooldowns for report, appeal, and ticket commands to prevent abuse | 🟡 Med | S |
| ⚪ **Later** | **Mod action reason required enforcement** — reject ban/mute/warn commands that omit a reason; display an in-game or Discord error prompt | 🟡 Med | S |
| ⚪ **Later** | **Silent evidence-lock mode** — `/silentlock <player>` freezes all chat logs for a player in a read-only evidence state without notifying them | 🟡 Med | S |
| ⚪ **Later** | **Probation mode** — automatically apply a probation flag after ban expiry that enforces stricter chat rules and shorter kick-before-ban thresholds for a configurable window | 🔴 High | M |
| ⚪ **Later** | **Auto-escalate unresolved severe reports** — if a report tagged "severe" has no staff action after N hours, ping an escalation role and re-post in the priority queue | 🔴 High | S |
| ⚪ **Later** | **Moderator action rollback command** — `/rollback <action_id>` reverses a specific ban, mute, or warn with a full audit entry noting the rollback and the reason | 🟡 Med | M |

### Appeals / Cases

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Case timeline view** — `/case <id>` posts a chronological embed listing every action, note, and status change for that case | 🔴 High | M |
| ⚪ **Later** | **Evidence attachment index per case** — staff can attach Discord message links, image URLs, or log snippets to a case; indexed and retrievable via `/evidence <case_id>` | 🟡 Med | M |
| ⚪ **Later** | **Appeal cooldown after denial** — configurable minimum days a player must wait before reopening an appeal after a "Deny" decision | 🔴 High | S |
| ⚪ **Later** | **Appeal auto-close for inactivity** — if an open appeal receives no player response within N days, automatically deny and close with a templated reason | 🟡 Med | S |
| ⚪ **Later** | **Staff voting on appeals** — require a configurable minimum number of staff votes before an appeal is approved; `/vote <appeal_id> approve|deny` | 🟡 Med | M |
| ⚪ **Later** | **Appeal priority queue** — high-severity bans automatically land at the top of the appeal queue; visible in the appeal dashboard embed | 🟡 Med | S |
| ⚪ **Later** | **"Needs more info" appeal status** — staff can place an appeal in a NMI state that pauses the appeal timer and auto-DMs the player asking for details | 🟡 Med | S |
| ⚪ **Later** | **One-click precedent lookup** — `/precedent <case_id>` searches closed cases for similar offense types and returns the three most comparable resolutions | 🟡 Med | M |
| ⚪ **Later** | **Case tags** — staff-assignable category tags (toxicity, cheating, griefing, etc.) on each case; filterable in the appeal dashboard and audit export | 🟡 Med | S |
| ⚪ **Later** | **Auto-remind staff of aging open appeals** — daily digest embed listing all appeals open longer than N hours, grouped by severity | 🔴 High | S |

### DiscordBridge Enhancements

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Per-player incident thread templates** — configurable template for the opening message posted to each new per-player moderation thread | 🟡 Med | S |
| ⚪ **Later** | **Auto-thread archive + summary post** — when a per-player thread is archived, automatically post a summary embed (action count, outcome) to the parent channel | 🟡 Med | S |
| ⚪ **Later** | **Discord slash command parity** — expose every admin panel action as a native Discord `/` slash command for keyboard-first workflow | 🔴 High | L |
| ⚪ **Later** | **"Confirm dangerous action" button flows** — destructive actions (permanent ban, mass-kick) require a second confirmation button click before execution | 🔴 High | S |
| ⚪ **Later** | **Scheduled announcement campaigns** — define a sequence of announcements with relative offsets (T+0, T+30 min, T+60 min) that fire automatically | 🟡 Med | M |
| ⚪ **Later** | **Staff shift handoff embed** — `/handoff` posts a summary of all open tickets, active bans issued in the last 4 hours, and muted players to the staff channel | 🔴 High | S |
| ⚪ **Later** | **Moderator availability status sync** — `/oncall on/off` lets staff signal availability; the admin panel footer shows who is currently on-call | 🟡 Med | S |
| ⚪ **Later** | **Cross-channel escalation routing** — configurable rule table that routes specific ticket categories or offense types to a dedicated secondary Discord channel | 🟡 Med | M |
| ⚪ **Later** | **Message reaction shortcuts for mod actions** — reacting to a forwarded in-game message embed with a configured emoji (e.g. 🔇) triggers a quick mute on that player | 🟡 Med | M |
| ⚪ **Later** | **Auto-pin important moderation updates** — server restart, raid-mode toggle, and mass-ban events are automatically pinned in the admin channel | ⚪ Low | S |

### Logging / Audit / Forensics

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Immutable audit hash chain** — each audit entry stores the SHA-256 hash of the previous entry; tampering is detectable by re-validating the chain | 🔴 High | M |
| ⚪ **Later** | **Exportable case bundles** — `/export <case_id>` generates a JSON or CSV file containing all events, notes, and evidence for a case; posted as a Discord file attachment | 🟡 Med | M |
| ⚪ **Later** | **Searchable moderation transcript index** — full-text search across all stored audit notes via `/auditsearch <query>` | 🟡 Med | L |
| ⚪ **Later** | **Staff action diff view** — when a ban or mute is edited, the audit log stores a before/after diff and the admin panel shows the change inline | 🟡 Med | M |
| ⚪ **Later** | **Geolocation / event correlation in logs** — tag audit entries with the GeoIP region of the acting player; surface region-cluster anomalies in weekly digest | 🟡 Med | M |
| ⚪ **Later** | **"Who touched this case" audit view** — `/casetouches <case_id>` lists every staff member who viewed or modified the case with timestamps | 🟡 Med | S |
| ⚪ **Later** | **Deleted-message mirror log** — when a moderated player's chat message is deleted, a copy is stored in the audit log with original timestamp | 🟡 Med | S |
| ⚪ **Later** | **Suspicious pattern weekly digest** — automated Sunday report embed summarising top offense categories, most-active offenders, and detection accuracy | 🟡 Med | M |
| ⚪ **Later** | **Policy violation trend by category** — time-series chart data posted monthly showing which rule categories are trending up or down | ⚪ Low | M |
| ⚪ **Later** | **Red-flag alerts for abnormal mod activity** — alert if a staff account performs N ban/mute actions within T minutes (compromised account or error detection) | 🔴 High | M |

### Player Safety / Community Health

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Onboarding trust levels** — new players start at Trust Level 0 with limited chat rate; level increases automatically after N sessions without violations | 🔴 High | M |
| ⚪ **Later** | **New-player protection window** — accounts below N sessions are shielded from certain chat triggers (caps filter, spam detection) to reduce false positives | 🟡 Med | S |
| ⚪ **Later** | **Abuse report wizard** — guided `/report` command that prompts for category, evidence link, and description before submitting | 🔴 High | M |
| ⚪ **Later** | **Anonymous reporting mode** — `/reportanon` submits a report without revealing the reporter's identity to other players or in public channels | 🔴 High | S |
| ⚪ **Later** | **Harassment shield mode** — targeted player can request a temporary shield that escalates any message directed at them directly to staff review | 🔴 High | M |
| ⚪ **Later** | **Ignore-list moderation signals** — when multiple players ignore the same user, auto-flag that user for staff review | 🟡 Med | M |
| ⚪ **Later** | **Reputation decay / recovery system** — offense heat score decays over time; clean play for N days awards positive reputation that reduces auto-moderation sensitivity | 🟡 Med | M |
| ⚪ **Later** | **Positive-behavior rewards integration** — configurable milestones (e.g. 30 clean sessions) post a recognition embed to a community channel | ⚪ Low | S |
| ⚪ **Later** | **Auto-detect repeated target harassment** — if player A triggers violations involving player B multiple times, auto-flag as targeted harassment and escalate | 🔴 High | M |
| ⚪ **Later** | **Cool-down room state** — a limited-chat mute variant where a player can still read but not send messages for a configurable short duration before returning to normal | 🟡 Med | S |

### Staff Tools / Admin UX

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Unified moderation dashboard snapshot** — `/dashboard` command posts a live embed with: open tickets, active bans, muted players, recent incidents, and on-call staff | 🔴 High | M |
| ⚪ **Later** | **One-click player history card** — panel button that opens a compact embed with a player's full history: bans, mutes, warns, notes, appeal outcomes | 🔴 High | M |
| ⚪ **Later** | **Quick macros for common actions** — staff-defined macro slots in the admin panel for one-click execution of frequently repeated action+reason combinations | 🔴 High | S |
| ⚪ **Later** | **Bulk actions for raid cleanup** — `/bulkkick <criteria>` and `/bulkban <criteria>` act on all players matching a filter (join-time window, name pattern) | 🔴 High | M |
| ⚪ **Later** | **Mod notes templates** — configurable named templates for `/note` pre-filled with structured fields (incident type, severity, follow-up required) | 🟡 Med | S |
| ⚪ **Later** | **Staff performance metrics** — opt-in per-staff stats: tickets closed, average response time, appeals overturned; displayed in a private staff channel embed | 🟡 Med | M |
| ⚪ **Later** | **Incident queue assignment system** — staff can claim or assign open incident tickets; unclaimed tickets are highlighted after N minutes | 🔴 High | M |
| ⚪ **Later** | **"Take ownership" lock for active cases** — when a staff member opens a case, it is soft-locked to them for N minutes to prevent double-handling | 🟡 Med | S |
| ⚪ **Later** | **Command dry-run mode** — `/ban --dry-run` previews the exact embed and actions that would execute without applying them | 🟡 Med | S |
| ⚪ **Later** | **Admin panel per-action rate limits** — independent configurable rate limits per action type (ban, mute, kick) in addition to the existing global 60s panel cooldown | 🟡 Med | S |

### Whitelist / Access Control (Extended)

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Invite-code based whitelist grants** — generate single-use or N-use invite codes; redeeming a code adds the player automatically with a configurable tier | 🔴 High | M |
| ⚪ **Later** | **Temporary whitelist windows by schedule** — define time-windows in config during which the whitelist is relaxed (e.g. open-server events) | 🟡 Med | S |
| ⚪ **Later** | **Country / region allow / deny policy layers** — combine GeoIP blocking with whitelist overrides: deny a region globally but allow whitelisted players from that region | 🟡 Med | M |
| ⚪ **Later** | **Role-based whitelist tiers** — automatically assign or validate whitelist tier based on the player's Discord role at join time | 🟡 Med | M |
| ⚪ **Later** | **Auto-expiring VIP access** — whitelist entries granted via boost or purchase auto-expire and post a renewal prompt embed | 🟡 Med | S |
| ⚪ **Later** | **Whitelist anomaly detector** — alert staff if more than N entries are added within T minutes (bulk-add abuse detection) | 🟡 Med | S |
| ⚪ **Later** | **Access request + approval workflow** — players submit a `/whitelistrequest` in-game or via Discord; staff review and approve/deny from the admin panel | 🔴 High | M |
| ⚪ **Later** | **Account fingerprint consistency checks** — flag whitelist entries whose EOS PUID or display name changes significantly since their last approved session | 🟡 Med | M |
| ⚪ **Later** | **Grace-period reconnect exemptions** — whitelisted players who disconnect within a short window (crash/timeout) bypass re-join checks for N minutes | 🟡 Med | S |
| ⚪ **Later** | **Priority queue slots by tier** — reserve a configurable number of join slots per whitelist tier that are never consumed by lower tiers | 🟡 Med | S |

### Anti-Cheat / Integrity (Server-side Heuristics)

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Impossible movement / pathing checks** — flag players whose reported position changes exceed physically possible speeds over a configurable sample window | 🔴 High | L |
| ⚪ **Later** | **Build / place speed anomaly detection** — alert when a player's build-placement rate exceeds the configurable human-possible threshold per second | 🔴 High | M |
| ⚪ **Later** | **Resource gain outlier detection** — compare a player's resource-gain rate against the server-wide percentile; flag statistical outliers for staff review | 🔴 High | M |
| ⚪ **Later** | **Repeated suspicious kill-pattern alerts** — detect repeat rapid-kill sequences and alert staff with a confidence score and session snapshot | 🔴 High | M |
| ⚪ **Later** | **Shadow flag mode** — silently mark a suspicious player for elevated logging before any punitive action; staff are notified but the player sees no change | 🔴 High | M |
| ⚪ **Later** | **Confidence-score based enforcement** — auto-moderation only acts when a configurable confidence threshold is met; borderline cases are flagged for manual review | 🔴 High | M |
| ⚪ **Later** | **Auto-request manual review for edge cases** — when confidence score is between low and high thresholds, post a staff-review request embed and pause action | 🔴 High | S |
| ⚪ **Later** | **Session risk score updated in real time** — composite score updated each tick combining movement anomalies, resource outliers, and violation history | 🔴 High | L |
| ⚪ **Later** | **Toggleable anti-exploit rule packs by season** — config-driven rule bundles that can be enabled / disabled without a server restart for seasonal content changes | 🟡 Med | M |
| ⚪ **Later** | **Automated evidence metadata snapshots** — when a player is shadow-flagged, automatically capture and store session metadata (position, resource counts, action rates) as evidence | 🔴 High | M |

### Reliability / Ops / Config

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Hot-reloadable rule packs** — `/reloadrules` reloads all filter/escalation/exploit rule configs without a server restart | 🔴 High | S |
| ⚪ **Later** | **Per-feature kill-switches** — each major subsystem (AFK kick, chat filter, anti-exploit) has a config toggle that can disable it instantly without code changes | 🔴 High | S |
| ⚪ **Later** | **Canary mode for new moderation rules** — new rules run in log-only mode for a configurable trial period before enforcement is enabled | 🟡 Med | M |
| ⚪ **Later** | **Automatic config schema migration** — on startup, detect old config keys and migrate them to the current schema, preserving values and logging all changes | 🟡 Med | M |
| ⚪ **Later** | **Rule simulation against historical logs** — `/simrule <rule_id>` re-runs a rule against the last N audit-log entries and reports how many would have fired | 🟡 Med | L |
| ⚪ **Later** | **Backup integrity verification job** — periodic job that re-reads each backup file, validates JSON/checksum, and posts a pass/fail report to the admin channel | 🔴 High | S |
| ⚪ **Later** | **Alerting on failed webhook / Discord posts** — detect HTTP error responses from the Discord API, increment a failure counter, and page the admin channel after N consecutive failures | 🔴 High | S |
| ⚪ **Later** | **Fallback queue when Discord API is down** — buffer outgoing messages in memory; replay them in order once the API recovers | 🔴 High | M |
| ⚪ **Later** | **Health endpoint for mod subsystems** — REST endpoint (gated by RestApiKey) reporting the current state of each subsystem: OK / DEGRADED / DOWN | 🟡 Med | S |
| ⚪ **Later** | **Safe mode startup** — config flag that starts the server with only critical subsystems active (join/ban enforcement) while disabling non-essential features for diagnostics | 🔴 High | S |

---

## Extended Feature Backlog (Pass 6)

> Added 2026-04-16. All items are ⚪ **Later** (unscheduled). Server-side only — no client download required.  
> † = a related item already exists in the sections above.

---

### Moderation / Enforcement

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Delayed-ban scheduler** — `/scheduleban <player> <time> <reason>` queues a ban to activate at a future date/time; visible in the scheduled-ban registry and cancellable via `/cancelsban <id>` | 🔴 High | M |
| ⚪ **Later** | **Auto-unban expiry reminder** — N hours before a temp-ban expires, post an embed to the admin channel so staff can decide whether to extend or release early | 🟡 Med | S |
| ⚪ **Later** | **Anti-abuse guardrails for moderators** — config-level caps on the maximum ban duration, mute duration, and warn count a moderator tier can issue per rolling hour without senior-staff approval | 🔴 High | S |
| ⚪ **Later** | **Rejoin cooldown after kick** — configurable minimum seconds a kicked player must wait before the server accepts their next join attempt; enforced without a full ban record | 🟡 Med | S |
| ⚪ **Later** | **"First offense leniency" toggle per category** — config flag per offense category that converts the first-ever auto-action into a warning instead of a ban or mute | 🟡 Med | S |
| ⚪ **Later** | **Time-window rule strictness** — config schedule (e.g. 22:00–06:00) that automatically tightens chat-filter thresholds and auto-action cooldowns; reverts at the window end | 🔴 High | M |
| ⚪ **Later** | **Match / round-based punishment resets** — optional mode that clears active mutes at the end of each configured round or wipe cycle, with a full audit entry per reset | ⚪ Low | S |
| ⚪ **Later** | **Progressive captcha-style verification flow** — new or flagged players must complete a configured command sequence (e.g. `/verify <code>`) before their chat relay is enabled; code is DM'd via Discord | 🟡 Med | M |
| ⚪ **Later** | **Phrase severity tiers for chat filter** — configure filter phrases with a severity level (low/med/high/critical) so low-severity matches warn while critical matches immediately mute | 🔴 High | M |
| ⚪ **Later** | **Chat filter staff exemption list** — EOS PUIDs in a config exempt list bypass all chat-filter checks so staff can discuss rule-violating phrases without triggering auto-actions | 🟡 Med | S |
| ⚪ **Later** | **Message entropy / gibberish detection** — auto-warn messages whose character-entropy or consonant-cluster score falls outside a normal range (catches random gibberish spam) | 🟡 Med | M |
| ⚪ **Later** | **Bulk unmute / unban with filters** — `/bulkunmute` and `/bulkunban` accept a filter flag (e.g. `--reason-contains "event"` or `--expires-before <date>`) to lift a set of punishments at once | 🔴 High | M |
| ⚪ **Later** | **Auto-close stale low-priority warnings** — periodic job that marks warnings below a configured severity as "expired" once they exceed `WarnDecayDays` without further offenses, freeing escalation slots | ⚪ Low | S |

---

### Anti-Spam / Anti-Abuse

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Per-command-family global rate limit** — separate rate-limit buckets per command family (ban-family, mute-family, kick-family) across all staff to detect coordinated misuse | 🟡 Med | S |
| ⚪ **Later** | **Auto-lock Discord alert channels during raids** †— when raid mode activates, automatically set the configured admin alert channel to staff-only send permissions, reverting on raid-mode off | 🔴 High | S |
| ⚪ **Later** | **Join queue throttling under high load** — when the player count exceeds a configurable threshold, buffer incoming join requests in a FIFO queue and admit them at a fixed rate | 🟡 Med | M |
| ⚪ **Later** | **Link / domain allowlist** — a companion allowlist to the domain denylist so trusted domains (e.g. the server's own website) are always passed through without filter checks | 🟡 Med | S |

---

### Appeals & Cases

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Appeal auto-tagging by offense type** †— when an appeal ticket is opened, classify it automatically (spam / toxicity / exploit / other) based on the original ban reason and prefix the channel name | 🟡 Med | S |
| ⚪ **Later** | **Appeal duplicate detection** — when a player opens a new appeal ticket, check for any open appeal from the same Discord user and block creation with an informational message linking the existing ticket | 🔴 High | S |
| ⚪ **Later** | **Rich Discord incident embeds with action buttons** — moderation event webhooks (ban, mute, kick) include inline Discord buttons for "View History", "Undo", and "Add Note" so staff can act without switching channels | 🔴 High | M |
| ⚪ **Later** | **Staff acknowledgment buttons for critical alerts** — raid-mode and anomaly-detection embeds include an "Acknowledged" button; unacknowledged alerts re-ping the on-call role after N minutes | 🔴 High | S |
| ⚪ **Later** | **"Mark resolved" workflow in Discord** — alerts in the admin channel gain a "Mark Resolved" button that stamps the embed with the resolving admin's name and timestamp | 🟡 Med | S |

---

### Discord & Admin UX

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Discord role-based command scope** — config mapping that restricts specific admin-panel actions to Discord roles, independent of the in-game EOS PUID permission tiers | 🟡 Med | M |
| ⚪ **Later** | **Discord role ↔ in-game permission sync** — map Discord roles to `AdminEosPUIDs` / `ModeratorEosPUIDs` tiers so granting the Discord role is sufficient to activate in-game permissions | 🔴 High | L |
| ⚪ **Later** | **Webhook failover endpoints** — configure a secondary (and tertiary) Discord webhook URL per event channel; if the primary POST fails, automatically fall over to the next endpoint | 🔴 High | M |
| ⚪ **Later** | **Webhook delivery retry dashboard** — admin-channel embed posted when a webhook retry succeeds after failures, showing which events were delayed and by how long | 🟡 Med | S |
| ⚪ **Later** | **Queue depth monitoring alert** — if the outbound Discord API queue exceeds a configurable item count, post an alert embed and drop only the lowest-priority pending messages | 🟡 Med | S |
| ⚪ **Later** | **Quiet-hours event routing** — config time window during which only high-priority (ban / raid-mode / anomaly) events are posted; informational events are held and batch-posted at window end | 🟡 Med | M |
| ⚪ **Later** | **Admin panel widget customization** — config-driven layout for the admin panel embed: server admins can reorder, hide, or rename action rows per their workflow | ⚪ Low | M |
| ⚪ **Later** | **Localized message template packs** — separate INI files for each supported language containing all player-facing messages; server selects the active pack via a config key | ⚪ Low | L |
| ⚪ **Later** | **Pinned live case summary** — a bot-managed pinned message in each ticket channel that auto-refreshes with the current status, assigned staff, priority, and evidence count | 🟡 Med | M |

---

### Whitelist & Access Control

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Join-time terms-acceptance tracking** — first join by a new whitelisted player presents a `/accept` command requirement; join is deferred until accepted; acceptance is audit-logged | 🟡 Med | M |
| ⚪ **Later** | **Auto-deny incomplete whitelist applications** †— whitelist ticket applications missing required form fields are automatically denied with a templated rejection message prompting resubmission | 🔴 High | S |
| ⚪ **Later** | **Revoke-by-group bulk whitelist removal** — `/whitelist revokegroup <group>` removes all entries belonging to a named tier or group tag and logs each removal | 🔴 High | S |
| ⚪ **Later** | **Whitelist activity audit dashboard** — weekly embed to the admin channel showing adds, removes, tier changes, and expirations over the period | 🟡 Med | S |
| ⚪ **Later** | **Trust-based fast-track whitelist** — players with a trust score above a configurable threshold and zero prior violations are auto-approved via an expedited application path | 🟡 Med | M |
| ⚪ **Later** | **Ban-to-whitelist conflict auto-resolution** — on whitelist add, automatically check whether any linked UID or IP is currently banned; surface a warning and require admin override before proceeding | 🔴 High | S |
| ⚪ **Later** | **Appeal-approved auto-whitelist restore** — when a ban appeal is approved and the player was previously whitelisted, automatically restore their whitelist entry at the original tier | 🔴 High | S |
| ⚪ **Later** | **One-time emergency bypass tokens** — staff can generate a single-use join token for a specific player that bypasses whitelist checks for a configurable window; token use is audit-logged | 🟡 Med | M |
| ⚪ **Later** | **Whitelist sync integrity checker** — on startup and on demand, cross-validate the whitelist JSON against the ban database and session registry for conflicts; post discrepancies to the admin channel | 🟡 Med | S |
| ⚪ **Later** | **Scheduled whitelist maintenance window** — config-defined time window during which whitelist changes are applied in batch rather than immediately; notified in the admin channel at window open/close | ⚪ Low | S |
| ⚪ **Later** | **Dynamic max-slots by time / day** — config schedule that adjusts `MaxSlots` automatically (e.g. higher cap on weekends, lower during maintenance windows) | 🟡 Med | S |

---

### AFK / Session Management

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **AFK detection profiles by player role** — different AFK timeout thresholds for staff, builders, and regular players configured via per-role AFK profile entries | 🟡 Med | S |
| ⚪ **Later** | **AFK immunity window after legitimate activity burst** — after a player's build-count or movement spikes above a threshold, start a configurable immunity window during which AFK checks are suspended | 🟡 Med | S |
| ⚪ **Later** | **AFK analytics by session** — track and report average AFK-kick rate, session length before kick, and recurrence rate per player tier in the weekly digest | ⚪ Low | S |
| ⚪ **Later** | **Session duration safeguard** — optionally warn and kick players whose single session exceeds a configurable maximum duration, with a configurable grace-period warning first | ⚪ Low | S |
| ⚪ **Later** | **Auto-break reminders for marathon sessions** — at configurable session-length milestones (e.g. 4 h, 8 h), send an in-game notification suggesting a break (no enforcement, informational only) | ⚪ Low | S |
| ⚪ **Later** | **Join failure diagnostics report** — when a player is rejected at join (ban, whitelist, GeoIP, raid-mode), log the specific rejection reason and post a debug entry to the admin channel if a configured verbosity flag is set | 🟡 Med | S |

---

### Live Risk Intelligence & Staff UX

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Live player risk heatmap embed** — `/riskheatmap` posts an embed to the admin channel visualising current online players ranked by composite risk score with one-click action buttons | 🔴 High | M |
| ⚪ **Later** | **Real-time offense score overlay in panel** — the admin panel "Players" embed includes each player's current risk score and a coloured indicator (🟢/🟡/🔴) | 🔴 High | M |
| ⚪ **Later** | **"Nearest severe incident" shortcut** — `/nextalert` posts the single highest-priority unacknowledged incident embed so staff can immediately attend to the most critical active case | 🔴 High | S |
| ⚪ **Later** | **Abuse wave predictor from recent trends** — if violations per minute exceed twice the rolling 24 h average for three consecutive intervals, automatically post an early-warning embed and suggest activating raid mode | 🔴 High | M |
| ⚪ **Later** | **Event-driven alert severity scoring** — each auto-generated alert carries a computed severity score (based on offense type, player history, and current server load) used to triage the admin alert feed | 🟡 Med | M |
| ⚪ **Later** | **Adaptive rule-tuning suggestions** — weekly digest includes a "Tuning Tips" section: rules that never fired suggest raising thresholds; rules that fire constantly suggest lowering or splitting them | ⚪ Low | M |
| ⚪ **Later** | **High-latency offender correlation** — flag players whose join latency or reconnect frequency correlates with known abuse patterns (crash-exploit or reconnect-spam) and surface them in the admin channel | 🟡 Med | M |
| ⚪ **Later** | **Idle resource abuse detection** — server-side heuristic that alerts staff when a player's resource-acquisition rate is abnormally high relative to their pawn activity (movement + build counts) | 🔴 High | M |
| ⚪ **Later** | **Canary mode for selected admin IDs** — specific admin EOS PUIDs can be flagged as canary users; new config rules apply only to their sessions first before rolling out to all staff | ⚪ Low | S |

---

### Analytics & Reporting

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Storage quota alerts** — configurable max-size thresholds (MB) for the ban database, audit log, and ticket transcript store; breach triggers a Discord alert with current sizes and recommended pruning actions | 🟡 Med | S |
| ⚪ **Later** | **Corrupt data auto-repair** — on startup and on scheduled intervals, detect and remove malformed JSON objects from ban, warning, and whitelist data files; log each repair with a before-snapshot | 🔴 High | M |

---

### Game-Specific / Cross-Server

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Zone-based rule sets** — define in-game map zones in config; players in flagged zones have stricter anti-spam or anti-exploit thresholds applied automatically | 🟡 Med | L |
| ⚪ **Later** | **Event-based temporary rule boosts** — `/eventrules on <preset>` applies a named rule-override pack (e.g. "tournament", "wipe-day") for the duration of a server event; reverts automatically at event end | 🔴 High | M |
| ⚪ **Later** | **Economy exploit detection hooks** — configurable thresholds for resource-gain-per-tick; hits generate a shadow-flag and admin-channel alert with a session snapshot for manual review | 🔴 High | M |
| ⚪ **Later** | **Trade / transfer scam pattern detection** — detect rapid repeated resource-transfer sequences involving the same two players and flag to staff for review | 🔴 High | M |
| ⚪ **Later** | **Base griefing behaviour anomaly alerts** — alert when a player's demolition or interaction rate against other players' structures exceeds a configurable threshold per session | 🔴 High | M |
| ⚪ **Later** | **Shared trust network between partner servers** — federated trust-score sharing where a positive or negative reputation score from a partner server is factored into the local risk assessment on join | 🟡 Med | L |

---

## Extended Feature Backlog (Pass 7)

> Added 2026-04-17. All items are ⚪ **Later** (unscheduled). Server-side only — no client download required.  
> † = a related item already exists in the sections above.

---

### Chat & Messaging Controls

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **First-join chat cooldown** — new players cannot send in-game chat (or have messages relayed to Discord) until they have been connected for a configurable number of seconds; staff chat bypass available | 🔴 High | S |
| ⚪ **Later** | **Link-only channel enforcement** — configurable in-game chat "channel" (prefix) that accepts only messages containing a URL; non-URL messages are silently dropped with a private notice to the sender | 🟡 Med | S |
| ⚪ **Later** | **Profanity severity tiers per channel** — each named in-game chat channel (configured by prefix or tag) can declare its own profanity-tier threshold, allowing stricter rules in public chat and looser rules in staff channels | 🟡 Med | M |
| ⚪ **Later** | **Language-specific chat filter profiles** — loadable per-language filter phrase packs; the active pack is selected via a config key and merged with the base filter at startup | 🟡 Med | M |
| ⚪ **Later** | **Nickname policy enforcement** — reject joins (or auto-kick post-join) from players whose display name violates a configurable policy (min/max length, regex denylist, reserved prefixes); post a policy-violation embed to the admin channel | 🔴 High | M |
| ⚪ **Later** | **Staff name impersonation detection** — on join, fuzzy-compare the display name against a configured list of protected staff names; flag high-similarity matches and optionally block the join | 🔴 High | S |
| ⚪ **Later** | **Invite-link policy engine** — configurable rules (allow/deny/warn) per detected invite-link pattern (Discord invites, referral codes, competitor URLs) with per-pattern escalation actions, beyond a simple blocker | 🟡 Med | M |
| ⚪ **Later** | **Attachment / media scanning hooks** — when a Discord→game relay message contains an attachment or embed URL, run it through a configurable denylist of MIME types or domain patterns before relaying | 🟡 Med | M |
| ⚪ **Later** | **Scheduled rule reminder broadcasts** — configurable list of server-rule reminder messages that are broadcast in-game and posted to a Discord channel on a schedule (daily/weekly, configurable times) | ⚪ Low | S |
| ⚪ **Later** | **Announcement targeting by player segment** — `/announce` command accepts an optional `--segment` flag (e.g. `new`, `veteran`, `staff`) so broadcasts are delivered only to matching players in-game | 🟡 Med | M |

---

### Moderation / Enforcement

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Auto-tempban for repeated mute violations** — if a player accumulates N mutes within a rolling window, automatically escalate to a short temporary ban; configurable per window and ban duration | 🔴 High | M |
| ⚪ **Later** | **Mute appeal queue** — dedicated ticket category (or Discord channel) for mute appeals, separate from ban appeals; includes mute context (reason, admin, duration) in the welcome embed and Approve/Deny buttons | 🔴 High | M |
| ⚪ **Later** | **Warning forgiveness tokens** — `/warnpardon <player>` lets an authorized admin grant a one-time forgiveness that subtracts one warn from the escalation counter without using `/clearwarn`; logged with a pardon reason | 🟡 Med | S |
| ⚪ **Later** | **Staff-only hidden warning categories** — warn categories tagged `internal` are recorded in the database and visible in `/bancheck` to staff tiers only; they do not appear in player-facing ban-kick messages or public embeds | 🟡 Med | S |
| ⚪ **Later** | **Action taxonomy config** — declare a structured list of named offense categories in config (e.g. `toxicity`, `griefing`, `exploiting`, `spam`) that are shared across ban templates, escalation tiers, and chat-filter rules for consistent labeling | 🟡 Med | M |
| ⚪ **Later** | **Session anomaly detection (rapid account switching)** — flag when multiple different EOS PUIDs share the same IP address in a short time window; post an alert embed to the admin channel with all matching PUIDs and session timestamps | 🔴 High | M |
| ⚪ **Later** | **Per-world / per-mode moderation profiles** — named config profiles (e.g. `PvP`, `Creative`, `Event`) that override spam thresholds, AFK timeouts, and auto-action settings; activated via in-game or Discord command | 🔴 High | M |
| ⚪ **Later** | **Cross-server warning sync** — when a warning is issued on one server, replicate it to all peers via the existing `PeerWebSocketUrls` channel so escalation ladders advance consistently across the network | 🔴 High | M |
| ⚪ **Later** | **Shared moderator notes across servers** — notes added via `/note` are broadcast to all peer servers and stored locally so any server's staff can read the full note history for a player | 🟡 Med | M |

---

### Trust & Verification

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Gradual permission unlock by trust level** — as a player's trust level increases (from onboarding trust tiers), additional in-game capabilities (e.g. URL posting, faster chat rate) unlock automatically without staff intervention | 🔴 High | M |
| ⚪ **Later** | **Manual verification queue with SLA timers** — unverified players are held in a pending-join state; staff see a queue embed with waiting players and time-in-queue; SLA breach pings the on-call role | 🔴 High | M |

---

### Tickets & Appeals

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **"Known issue" auto-responder in tickets** — config-defined keyword patterns that, when matched in a new ticket's opening message, auto-post a pre-written response embed inside the ticket channel explaining the known issue and ETA | 🔴 High | S |
| ⚪ **Later** | **Appeal evidence checklist enforcement** — configurable checklist of required evidence fields (e.g. "ban reason acknowledged", "evidence URL provided") shown in the appeal ticket; appeal cannot be submitted as Approve until all boxes are checked by staff | 🟡 Med | M |
| ⚪ **Later** | **Appeal outcome consistency checker** — weekly automated scan that compares recent appeal decisions (Approve/Deny) for the same offense category and ban duration; posts a digest to a staff channel flagging statistically inconsistent outcomes | 🟡 Med | M |
| ⚪ **Later** | **Ban reason normalization tool** — `/normalizereason <ban_id>` suggests a standardized reason string based on the configured action taxonomy; staff confirm and the original reason is preserved in an edit-history entry | 🟡 Med | M |

---

### Server Rules & Onboarding

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Server rules versioning + acceptance log** — rules are stored as versioned entries in a config file; when the version increments, returning players are prompted in-game to re-accept; acceptance timestamps are audit-logged per player per version | 🔴 High | M |

---

### Whitelist & Access Control

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Whitelist capacity forecasting** — periodic job that projects days until `MaxSlots` is reached based on recent add-rate; posts a forecast embed to the admin channel when capacity is projected to be full within N days | 🟡 Med | S |
| ⚪ **Later** | **Join queue priority ordering** — when the join queue is active, players in higher whitelist tiers or with lower risk scores are admitted ahead of those in lower tiers, rather than strict FIFO | 🟡 Med | M |
| ⚪ **Later** | **Queue abuse detection** — flag players who join and immediately disconnect more than N times within T minutes while in the join queue; auto-remove them from the queue and post an alert embed | 🔴 High | S |
| ⚪ **Later** | **Live join queue status panel** — a Discord embed in a configured channel that auto-updates with current queue position count, estimated wait time, and next-to-admit player tier; refreshed on each queue change | 🟡 Med | M |

---

### REST API & Security

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Scoped API keys with rotation** — support multiple `RestApiKey` entries each scoped to a subset of endpoints (e.g. read-only key vs. full-write key); keys can be rotated via a config reload without a server restart | 🔴 High | M |
| ⚪ **Later** | **IP-based API rate limiting** — enforce a configurable request-per-minute cap per calling IP address on the REST API, independent of the existing per-admin ban rate limit; excess requests receive HTTP 429 | 🔴 High | M |
| ⚪ **Later** | **Command abuse detection** — track in-game and Discord panel command invocation rates per user; if any user exceeds a configurable threshold across any command family within a short window, auto-suspend their panel access and post an alert | 🔴 High | M |
| ⚪ **Later** | **Secret redaction in exported logs** — when any audit record or export bundle is generated, automatically scan for and redact strings matching configured secret patterns (API keys, tokens, IP addresses) before writing to file or posting to Discord | 🔴 High | M |
| ⚪ **Later** | **Signed moderation export bundles** — `/export` and `/caseexport` outputs include an HMAC signature computed from the RestApiKey so recipients can verify the bundle has not been tampered with after export | 🟡 Med | M |
| ⚪ **Later** | **Compliance export mode** — `/complianceexport <player>` generates a structured, audit-ready package (all bans, warns, notes, sessions, appeals, chat snippets) formatted for legal / compliance review requests; access gated to the admin tier | 🔴 High | M |

---

### Analytics & Reporting

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Incident timeline auto-generation** — when a raid-mode or mass-incident event ends, automatically compile a timestamped timeline embed (join spike, rule hits, actions taken, resolution) and post it to the admin channel | 🔴 High | M |
| ⚪ **Later** | **Predictive high-risk period alerts** — use historical hourly violation counts (stored in the audit log) to forecast the next likely high-violation window; post an advance-warning embed to the staff channel before it begins | 🟡 Med | L |

---

## Extended Feature Backlog (Pass 8)

> Added 2026-04-17. All items are ⚪ **Later** (unscheduled). Server-side only — no client download required.  
> † = a related item already exists in the sections above.

---

### Moderation / Enforcement

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Appeal reputation score** — track the ratio of denied to approved appeals per player; serial appeal abusers automatically receive an extended appeal cooldown and a staff-channel alert on their next submission | 🔴 High | S |
| ⚪ **Later** | **Moderator cooldown-bypass audit log** — when a senior admin overrides another admin's panel or command cooldown, create a dedicated audit-log entry recording the bypassing admin, the bypassed admin, and the action taken | 🟡 Med | S |
| ⚪ **Later** | **Auto-silence for new accounts after N reports** — if a player who has never been punished accumulates N unresolved player reports within a configurable window, apply a short automatic mute and post a review embed for staff | 🔴 High | S |
| ⚪ **Later** | **"Why this action?" rationale enforcement for high-risk commands** — `/ban permanent`, `/unlinkbans`, and ban-reason edits require an additional free-text rationale field (via modal); rationale is stored in the audit log alongside the action | 🟡 Med | S |
| ⚪ **Later** | **Emergency lockdown mode** — `/lockdown on/off` simultaneously freezes in-game chat relay, queues new joins, and sets all auto-action thresholds to their strictest configured values; reverts fully on off with a delay-release audit entry | �� High | M |

---

### Chat & Anti-Abuse

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Real-time profanity heatmap dashboard** — a bot-managed Discord embed updated on a configurable interval showing per-phrase filter-hit counts for the current server session; helps admins identify which rules fire most and tune thresholds without waiting for the weekly digest | 🟡 Med | M |
| ⚪ **Later** | **Scam phrase pattern packs (hot-loadable)** — loadable INI array sections per threat category (e.g. `+PhishingPhrases=`, `+CryptoScamPhrases=`) merged with the base chat filter at startup and reloadable via `/admin reloadconfig` without a restart | 🔴 High | S |
| ⚪ **Later** | **Slowmode auto-enable during raids** †— when raid mode activates, automatically apply a per-player chat rate limit (configurable messages per minute); reverts when raid mode turns off | 🔴 High | S |
| ⚪ **Later** | **"First offense warn, second mute" policy engine** — a dedicated multi-step policy table (separate from `WarnEscalationTiers`) that maps specific chat-filter rule IDs to a 2-step ladder: first match → warn, second match within a window → mute, third → ban; configurable per rule | 🔴 High | M |
| ⚪ **Later** | **Auto-redact PII from forwarded chat** — before relaying a player message to Discord, strip patterns matching configured regexes (IP addresses, email patterns, phone-number formats) and replace with `[REDACTED]`; log the original to the audit trail | 🔴 High | M |

---

### DiscordBridge & Tickets

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Auto-summarize long tickets** — when a ticket channel reaches a configurable message count, have the bridge bot post (and pin) a summary embed in the ticket channel listing the current status, key actions taken, and any open questions; refreshed each time the threshold is crossed again | 🟡 Med | M |
| ⚪ **Later** | **Closed-ticket quality scoring (resolution tags)** — on ticket close, staff must choose a resolution tag (Resolved / Unresolved / Escalated / Duplicate / No Action) via a dropdown; tag distribution per category and per staff member is surfaced in `/ticket stats` and the weekly digest | 🟡 Med | M |
| ⚪ **Later** | **Ticket category routing by keyword** — config rules (`+TicketRoute=keyword|category`) that automatically set the ticket type or priority when the opener's reason message contains a configured keyword; reduces misrouted tickets | 🔴 High | S |
| ⚪ **Later** | **Staff handoff notes inside ticket metadata** — `/ticketnote <text>` posts a styled staff-only embed inside the ticket channel and also stores the note in ticket state so it persists across restarts and appears in the transcript export | 🟡 Med | S |

---

### Security / Integrity

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Signed inbound webhook payload validation** — validate Discord interaction payloads using the `X-Signature-Ed25519` / `X-Signature-Timestamp` headers and the registered application public key; reject requests that fail signature verification before any processing occurs | 🔴 High | M |
| ⚪ **Later** | **"Impossible account travel" detection** — flag when two sessions sharing the same EOS PUID originate from GeoIP regions that are geographically implausible within the time delta between sessions; post an alert embed listing both sessions' IPs, regions, and timestamps | 🔴 High | M |
| ⚪ **Later** | **Privilege drift detector** — on each `/admin reloadconfig`, compare the current `AdminEosPUIDs` and `ModeratorEosPUIDs` lists against the snapshot from the previous load; post a Discord alert listing any IDs that were added or removed since the last reload | 🔴 High | S |
| ⚪ **Later** | **API key rotation reminders with grace window** — config key `ApiKeyExpiryDays` that causes a reminder embed to post to the admin channel N days before the configured expiry; a `ApiKeyGraceDays` window accepts both old and new keys simultaneously during rotation | 🟡 Med | S |

---

### Gameplay / Server Health

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Dynamic AFK timeout by player count** — automatically increase the AFK kick timeout when the online player count is below a configurable floor (fewer players → more lenient); reverts to the base timeout as the count recovers | 🟡 Med | S |
| ⚪ **Later** | **Automated restart recommendation** — track consecutive-uptime hours (and optionally server-side tick-rate degradation) against configurable thresholds; when both are breached, post a "consider restarting" embed to the admin channel with current metrics | 🟡 Med | M |
| ⚪ **Later** | **Scheduled maintenance warning pipeline** — `/schedulemaintenance <datetime> <duration_minutes> <message>` queues a series of progressively spaced in-game broadcasts and Discord announcements (60 min / 30 min / 10 min / now) counting down to a planned maintenance window; cancellable via `/cancelmaintenance` | 🔴 High | M |
| ⚪ **Later** | **Inactive-player structure decay helper** — periodic background job that cross-references the session registry for players absent more than a configurable number of days and posts a report embed to the admin channel listing each player's last-seen date; helps admins identify decay candidates | 🟡 Med | S |
| ⚪ **Later** | **Team-size compliance checks** — configurable maximum team or ally-group size (based on linked UIDs or IP-range grouping); when a group exceeds the limit, auto-alert the admin channel with the group members and risk score so staff can investigate | 🟡 Med | M |

---

### Analytics & Reporting

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **"Top recurring issues" topic clustering** — weekly digest section that groups warning and ban reasons by keyword similarity into thematic clusters and surfaces the most-repeated offense themes (e.g. "profanity — 38 cases", "griefing — 14 cases") to guide rule tuning | 🟡 Med | M |
| ⚪ **Later** | **Config effectiveness comparison report** — when a chat-filter rule or escalation tier is modified (tracked via config-reload diff), the next weekly digest includes a before/after auto-action count comparison for the affected rule to quantify the change's impact | 🟡 Med | S |
| ⚪ **Later** | **New vs. veteran violation rate comparison** — weekly digest side-by-side embed comparing violation and auto-action rates for players with fewer than a configurable session count vs. those above it; helps calibrate new-player leniency settings | ⚪ Low | S |

---

### Moderator UX / Quality of Life

| Status | Feature | Impact | Complexity |
|--------|---------|--------|------------|
| ⚪ **Later** | **Guided moderation command wizard** — `/wizard` opens an interactive Discord button flow that walks a staff member through selecting an action type, choosing a player from a dropdown of online players, picking a reason template, and confirming; produces the identical result as the full text command and is audit-logged the same way | 🔴 High | L |
| ⚪ **Later** | **Moderator follow-up reminders** — `/remind <case_id|player> <hours> [note]` schedules a personal reminder for the acting admin; when the timer fires, a DM or admin-channel ping is posted referencing the original case with the optional note | 🟡 Med | M |
| ⚪ **Later** | **Case linking** — `/linkcase <id_1> <id_2>` creates a bidirectional reference between two case IDs (bans, tickets, or audit incidents); linked cases are shown as cross-references in `/case` output and the ticket transcript so staff can follow related chains of events | 🔴 High | S |
| ⚪ **Later** | **Bulk action mode with safeguards** †— `/bulkaction <action> <player_list>` accepts a comma-separated list of player names or PUIDs and applies the same action to all; requires a modal confirmation listing every affected player and the total count before executing | 🔴 High | M |
