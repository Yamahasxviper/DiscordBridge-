# SatisfactoryModLoader [![CI](https://github.com/satisfactorymodding/SatisfactoryModLoader/actions/workflows/build.yml/badge.svg)](https://github.com/satisfactorymodding/SatisfactoryModLoader/actions/workflows/build.yml)

A tool used to load mods for the game Satisfactory. After Coffee Stain releases a proper Unreal modding API the project will continue as a utilities library.

This repository contains the SatisfactoryModLoader source code,
an ExampleMod with demos of some of the SML utilities,
a collection of editor utilities,
and more.
It also serves as the Unreal project used for developing mods.

## Roadmap

View the project roadmap here: [ROADMAP.md](ROADMAP.md)

---

## Mods in this repository

### [DiscordBridge](Mods/DiscordBridge) — v1.1.0

A server-only mod that creates a live two-way bridge between your dedicated
server's in-game chat and a Discord text channel. Features include:

- Two-way chat relay with configurable format strings and find-and-replace blocklist
- Server status announcements (online/offline) and dedicated status channel
- Live player-count Discord presence
- Player join/leave/timeout notifications with embed mode and admin-log channel
- Whitelist management from Discord (`/whitelist`) or in-game (`/ingamewhitelist`) with group/tier support, search, and application/verification workflows
- Game phase and schematic-unlock announcements
- `/stats`, `/playerstats`, and `/players` Discord slash commands
- Reaction-based vote-kick on player join
- AFK auto-kick
- Ban events channel and 42 Discord slash commands for moderation (when used with BanSystem)
- TicketSystem with 23 slash commands — tags, notes, escalation, SLA tracking, reminders, blacklist, and merge
- Scheduled announcements
- Discord invite URL broadcast

Requires **SMLWebSocket** and optionally **BanSystem** + **BanChatCommands**.

→ [DiscordBridge Release Notes](Mods/DiscordBridge/RELEASE_NOTES.md)
→ [DiscordBridge Documentation](Mods/DiscordBridge/Docs/README.md)

---

### [BanSystem](Mods/BanSystem) — v1.1.0

A server-only mod providing a persistent, EOS-based ban system for Satisfactory
dedicated servers. Features include:

- Permanent and temporary EOS PUID bans and IP address bans
- Login-time enforcement via PostLogin/PreLogin hooks
- UID linking for cross-identity enforcement
- Persistent JSON storage (`bans.json`)
- Warning system with configurable auto-ban escalation tiers
- Player session registry with IP logging
- Scheduled bans for deferred enforcement
- Ban appeals system with Discord notifications
- Local HTTP REST management API (42 endpoints, default port 3000) with optional API key auth
- Prometheus metrics export (`/metrics/prometheus`)
- Self-service appeals portal (`/appeals/portal`) and admin dashboard (`/dashboard`)
- CSV export for bans, warnings, sessions, and audit logs
- Discord webhook notifications via `BanDiscordNotifier` (bans, warns, kicks, appeals, auto-escalation, geo-IP blocks)
- Multi-server ban synchronisation via WebSocket (`BanSyncClient`)
- Player reputation scoring
- Automatic scheduled backups

→ [BanSystem README](Mods/BanSystem/README.md)
→ [BanSystem Documentation](Mods/BanSystem/Docs/README.md)

---

### [BanChatCommands](Mods/BanChatCommands) — v1.1.0

A server-only mod that adds 43 in-game chat commands for ban and moderation
management. Requires **BanSystem**.

Commands split across three permission levels:

- **Admin:** `/ban`, `/tempban`, `/unban`, `/unbanname`, `/bancheck`, `/banlist`,
  `/linkbans`, `/unlinkbans`, `/playerhistory`, `/banname`, `/reloadconfig`,
  `/warn`, `/warnings`, `/clearwarns`, `/clearwarn`, `/reason`, `/banreason`,
  `/announce`, `/stafflist`, `/note`, `/notes`, `/duration`, `/extend`,
  `/appeal`, `/scheduleban`, `/qban`, `/reputation`, `/bulkban`, `/staffchat`
- **Moderator:** `/kick`, `/modban`, `/mute`, `/unmute`, `/tempmute`,
  `/tempunmute`, `/mutecheck`, `/mutelist`, `/mutereason`, `/freeze`,
  `/clearchat`, `/report`
- **All players:** `/history`, `/whoami`

→ [BanChatCommands README](Mods/BanChatCommands/README.md)
→ [BanChatCommands Documentation](Mods/BanChatCommands/Docs/README.md)

---

### [SMLWebSocket](Mods/SMLWebSocket) — v1.1.0

A standalone C++ mod providing a custom RFC 6455 WebSocket client and server with
SSL/OpenSSL support for Satisfactory dedicated server mods. Required by
DiscordBridge and any other mod that needs a persistent WebSocket connection
on a CSS Dedicated Server.

Features include:

- `ws://` and `wss://` connections with TLS via OpenSSL
- WebSocket server with subscription-filtered event broadcasting
- Auto-reconnect with exponential back-off and ±20 % jitter
- Named client registry for cross-mod sharing
- Connection statistics (bytes/messages sent/received)
- 9 delegates: `OnConnected`, `OnMessage`, `OnBinaryMessage`, `OnClosed`, `OnError`, `OnReconnecting`, `OnReconnected`, `OnStateChanged`, `OnJsonMessage`
- Full Blueprint and C++ API

→ [SMLWebSocket README](Mods/SMLWebSocket/README.md)

---

## Documentation

Learn how to set up and use this repo on the [modding documentation](https://docs.ficsit.app/).

## Discord Server

Join our [discord server](https://discord.gg/QzcG9nX) to talk about SML and Satisfactory Modding in general.

## DISCLAIMER

This software is provided by the author "as is". In no event shall the author be liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to procurement of substitute goods or services; loss of use, data, or profits; or business interruption) however caused and on any
theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.
