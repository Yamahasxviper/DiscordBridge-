# Yamahasxviper — Satisfactory Dedicated Server Mods

Server-side Satisfactory mods by **Yamahasxviper**.  All mods target Satisfactory build `>=416835` and require SML `^3.11.3`.  No client download is needed — install on your dedicated server only.

---

## Mods

| Mod | Version | Description |
|-----|---------|-------------|
| [DiscordBridge](Mods/DiscordBridge/) | 1.1.0 | Bridges Satisfactory game chat with a Discord channel.  Two-way chat relay, player join/leave notifications, whitelist management, 42 Discord slash commands for moderation, 23-command ticket system, game-event announcements, vote-kick, AFK kick, and more. |
| [BanSystem](Mods/BanSystem/) | 1.1.0 | Persistent EOS-based ban system.  Login-time enforcement, permanent and temporary bans, IP bans, UID linking, warning system with auto-escalation tiers, scheduled bans, ban appeals portal, 41-endpoint REST API, Prometheus metrics, CSV exports, admin dashboard, multi-server WebSocket sync, and Discord webhook notifications. |
| [BanChatCommands](Mods/BanChatCommands/) | 1.1.0 | Adds 42 in-game chat commands for ban management, moderation, warnings, mutes, notes, freeze, player reports, scheduled bans, and more.  Requires **BanSystem**. |
| [SMLWebSocket](Mods/SMLWebSocket/) | 1.1.0 | RFC 6455 WebSocket client and server with SSL/OpenSSL support.  Drop-in replacement for Unreal's missing WebSockets module in Alpakit-packaged mods.  Required by DiscordBridge and BanSystem. |

---

## Dependency graph

```
DiscordBridge  ──requires──►  SMLWebSocket
               ──optional──►  BanSystem
               ──optional──►  BanChatCommands

BanSystem      ──requires──►  SMLWebSocket

BanChatCommands ──requires──► BanSystem
```

---

## Requirements

| Requirement | Version |
|-------------|---------|
| Satisfactory (dedicated server) | `>=416835` |
| SML (SatisfactoryModLoader) | `^3.11.3` |

---

## Quick install

1. Copy the mod folders you want into `<ServerRoot>/FactoryGame/Mods/`:
   ```
   Mods/SMLWebSocket/
   Mods/BanSystem/
   Mods/BanChatCommands/    ← optional, requires BanSystem
   Mods/DiscordBridge/      ← optional, requires SMLWebSocket
   ```
2. Start (or restart) the dedicated server.  SML loads the mods automatically.

---

## Documentation

| Mod | Docs |
|-----|------|
| DiscordBridge | [Release Notes](Mods/DiscordBridge/RELEASE_NOTES.md) · [Docs index](Mods/DiscordBridge/Docs/README.md) |
| BanSystem | [README](Mods/BanSystem/README.md) · [Docs index](Mods/BanSystem/Docs/README.md) |
| BanChatCommands | [README](Mods/BanChatCommands/README.md) · [Docs index](Mods/BanChatCommands/Docs/README.md) |
| SMLWebSocket | [README](Mods/SMLWebSocket/README.md) |

---

## DISCLAIMER

This software is provided by the author "as is". In no event shall the author be liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to procurement of substitute goods or services; loss of use, data, or profits; or business interruption) however caused and on any theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.
