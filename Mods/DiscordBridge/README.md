# DiscordBridge

**Version:** 1.11.0 | **Satisfactory build:** ≥ 416835 | **SML:** ≥ 3.11.3

DiscordBridge is a **server-only** Satisfactory mod that creates a live two-way bridge between your dedicated server's in-game chat and a Discord text channel via a Discord bot token. Players connecting to your server do **not** need to install anything — `RequiredOnRemote` is `false`.

---

## Features

- **Two-way chat bridge** — in-game messages appear in Discord and Discord messages appear in-game, fully customisable with format-string placeholders.
- **Server status announcements** — configurable online/offline messages posted to a dedicated status channel.
- **Live player-count presence** — the bot's Discord status shows the current player count in real time.
- **Ban system** — manage a server-side ban list from Discord or in-game using `!ban` commands. `!ban add <name>` automatically bans the player's platform ID (Steam/Epic) in the same step when they are connected. When a Steam player's Steam64 ↔ EOS PUID mapping is first observed, bans on either ID are automatically mirrored to the other so future logins are always caught.
- **Whitelist** — restrict which players can join, managed via `!whitelist` commands or a Discord role.
- **Ticket system** — button-based support tickets that create private Discord channels automatically.
- **Server info commands** — any Discord member can query server status and online players with `!server`.
- **Server control commands** — admin-only `!admin stop` / `!admin restart` for remote management.
- **Dedicated log file** — all mod log output is written to `<ServerRoot>/FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log` for easy troubleshooting.

---

## Quick Start

### Installation (recommended — Satisfactory Mod Manager)

1. Install [SMM](https://smm.ficsit.app/) and select your **server** installation.
2. Search for **DiscordBridge** and click **Install**. SMM installs SML and SMLWebSocket automatically.
3. Edit `<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini` and set:
   ```ini
   [/Script/DiscordBridge.DiscordBridgeSubsystem]
   BotToken=YOUR_DISCORD_BOT_TOKEN
   ChannelId=YOUR_CHANNEL_ID
   ServerName=My Satisfactory Server
   ```
4. Restart the server.

### Installation (manual)

Copy the `DiscordBridge/` and `SMLWebSocket/` mod folders into `<ServerRoot>/FactoryGame/Mods/`. SML must already be installed.

---

## Configuration Files

All settings live in the mod's `Config/` folder:

| File | What it controls |
|------|-----------------|
| `DefaultDiscordBridge.ini` | Connection, chat bridge, server status, presence, server info, server control |
| `DefaultWhitelist.ini` | Whitelist toggle, command prefix, role, kick messages |
| `DefaultBan.ini` | Ban system toggle, command prefix, role, kick messages |
| `DefaultTickets.ini` | Ticket panel channel, category, notify role, custom ticket reasons |

The mod automatically backs up each config file to `<ServerRoot>/FactoryGame/Saved/Config/` on every start and restores from the backup if a mod update resets the primary file.

---

## Discord Bot Requirements

In the [Discord Developer Portal](https://discord.com/developers/applications) enable the following **Privileged Gateway Intents** for your bot:

- **Server Members Intent**
- **Message Content Intent**

The bot also needs **Send Messages** and **Read Message History** permissions in the bridge channel. **Manage Roles** is required for `!ban role` / `!whitelist role`. **Manage Channels** is required for `!admin ticket-panel`.

---

## Command Reference

### Chat / Server Info (`!server`)

| Command | Effect |
|---------|--------|
| `!server players` | List currently online players |
| `!server online` | Alias for `!server players` |
| `!server status` | Show server name, online state, and player count |
| `!server eos` | Run a full EOS platform diagnostic |
| `!server help` | List all available bot commands |

### Ban System (`!ban`)

| Command | Effect |
|---------|--------|
| `!ban on` / `!ban off` | Enable / disable the ban system |
| `!ban players` | List connected players with platform name and EOS PUID |
| `!ban check <name>` | Check if a player is banned by name and PUID |
| `!ban add <name>` | Ban a player by in-game name; also auto-bans their platform ID (Steam/Epic) if connected |
| `!ban remove <name>` | Unban a player by in-game name |
| `!ban id add <id>` | Ban by Steam64 ID or EOS Product User ID |
| `!ban id remove <id>` | Unban by platform ID |
| `!ban id lookup <name>` | Look up the EOS PUID of a connected player |
| `!ban id list` | List all banned platform IDs |
| `!ban steam add <steam64_id>` | Ban by Steam64 ID with format validation |
| `!ban steam remove <steam64_id>` | Unban by Steam64 ID |
| `!ban steam list` | List all banned Steam64 IDs |
| `!ban epic add <eos_puid>` | Ban by EOS PUID (Steam & Epic on EOS servers) |
| `!ban epic remove <eos_puid>` | Unban by EOS PUID |
| `!ban epic list` | List all banned EOS PUIDs |
| `!ban list` | List all banned names and enabled/disabled state |
| `!ban status` | Show ban, whitelist, and platform (Steam/EOS) state |
| `!ban role add <discord_id>` | Grant the ban admin role |
| `!ban role remove <discord_id>` | Revoke the ban admin role |

### Whitelist (`!whitelist`)

| Command | Effect |
|---------|--------|
| `!whitelist on` / `!whitelist off` | Enable / disable the whitelist |
| `!whitelist add <name>` | Add a player |
| `!whitelist remove <name>` | Remove a player |
| `!whitelist list` | List whitelisted players and enabled/disabled state |
| `!whitelist status` | Show whitelist and ban system state |
| `!whitelist role add/remove <id>` | Grant / revoke the whitelist role |

### Admin Control (`!admin`)

| Command | Effect |
|---------|--------|
| `!admin stop` | Gracefully shut the server down (exit code 0) |
| `!admin restart` | Shut down so the process supervisor restarts it (exit code 1) |
| `!admin ticket-panel` | Post the ticket button panel to the configured channel |

---

## Dependencies

| Dependency | Minimum version | Notes |
|------------|----------------|-------|
| Satisfactory (dedicated server) | build ≥ 416835 | Server only |
| SML | ≥ 3.11.3 | Server only |
| SMLWebSocket | ≥ 1.0.0 | Custom WebSocket + SSL client (included in this repo) |

---

## Building from Source (Alpakit / UnrealEngine-CSS)

This mod is built as an **Alpakit C++ mod** targeting Coffee Stain Studios' custom Unreal Engine 5.3 build (**UnrealEngine-CSS**). Key constraints:

- Add `"DummyHeaders"` to `PublicDependencyModuleNames` in your `Build.cs`.
- Do **not** use `"OnlineSubsystem"`, `"OnlineSubsystemEOS"`, or `"OnlineSubsystemUtils"` — not reliably available to Alpakit mods.
- Do **not** use Unreal's built-in `WebSocketsModule` — not present in Alpakit packages. Use `SMLWebSocket` instead.
- Use `"OnlineIntegration"` (CSS-native) instead of v1 OnlineSubsystem headers.
- Use `"GameplayEvents"` for the structured in-game event bus (bridge lifecycle events and ToDiscord relay).
- Use `"ReliableMessaging"` for ordered client-to-server message delivery (forward-to-Discord channel).
- Use `GConfig->GetString()` on `GEngineIni` for config reads that normally need `IOnlineSubsystem::Get()`.

See [Docs/00-BuildSystem.md](Docs/00-BuildSystem.md) for the full developer guide and [Docs/01-GettingStarted.md](Docs/01-GettingStarted.md) for the server installation walkthrough.

---

## Documentation Index

| Guide | What it covers |
|-------|---------------|
| [Build System](Docs/00-BuildSystem.md) | Alpakit & CSS UnrealEngine build environment (developers) |
| [Getting Started](Docs/01-GettingStarted.md) | Installation, config files, Discord bot creation |
| [Connection Settings](Docs/02-ConnectionSettings.md) | `BotToken`, `ChannelId`, multi-channel setup |
| [Chat Bridge](Docs/03-ChatBridge.md) | Message format customisation |
| [Ban System](Docs/04-BanSystem.md) | Discord-managed player bans |
| [Whitelist](Docs/05-Whitelist.md) | Join restriction management |
| [Server Status](Docs/06-ServerStatus.md) | Online/offline announcements |
| [Player Count Presence](Docs/07-PlayerCountPresence.md) | Bot status line |
| [Troubleshooting](Docs/08-Troubleshooting.md) | Common problems and fixes |
| [Server Info Commands](Docs/09-ServerInfoCommands.md) | `!server` read-only queries |
| [Ticket System](Docs/10-TicketSystem.md) | Button-based support tickets |
| [Server Control Commands](Docs/11-ServerControlCommands.md) | `!admin` remote management |

---

For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>
