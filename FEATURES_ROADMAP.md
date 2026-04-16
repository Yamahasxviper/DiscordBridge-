# Features Roadmap

> **Last updated:** 2026-04-16 (pass 3)  
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
