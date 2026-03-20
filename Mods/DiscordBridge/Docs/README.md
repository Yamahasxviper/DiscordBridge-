# DiscordBridge – Documentation Index

Welcome to the DiscordBridge documentation. Use the links below to jump straight to the guide you need.

| Guide | What it covers |
|-------|---------------|
| [Build System](00-BuildSystem.md) | Alpakit & CSS UnrealEngine build environment, module constraints, and packaging (developers) |
| [Getting Started](01-GettingStarted.md) | How to install the mod, where the four config files live, and how to create your Discord bot |
| [Connection Settings](02-ConnectionSettings.md) | `BotToken`, `ChannelId` (including multi-channel), `ServerName` |
| [Chat Bridge](03-ChatBridge.md) | Customise how messages look on both sides of the bridge |
| [Ban System](04-BanSystem.md) | Discord-managed player bans and kick messages |
| [Whitelist](05-Whitelist.md) | Restrict who can join, managed from Discord |
| [Server Status & Behaviour](06-ServerStatus.md) | Online/offline announcements, `ServerStatusNotifyRoleId`, and bot-message filtering |
| [Player Count Presence](07-PlayerCountPresence.md) | Bot status showing live player count |
| [Troubleshooting](08-Troubleshooting.md) | Fixes for common problems |
| [Server Info Commands](09-ServerInfoCommands.md) | `!server players`, `!server status` and related read-only queries |
| [Ticket System](10-TicketSystem.md) | Button-based ticket panel, custom ticket reasons, private ticket channels |
| [Server Control Commands](11-ServerControlCommands.md) | Admin-only `!admin stop` / `!admin restart` / `!admin ticket-panel` commands |
| [Discord Commands Reference](discord-commands.md) | **All commands in one place** — every Discord and in-game command with descriptions, parameters, and usage notes |
| [Native Hooking Reference](12-NativeHooking.md) | SML C++ native hook macros, `TCallScope` API, virtual-function hooks, and worked examples (developers) |
| [Third-Party Libraries](13-ThirdPartyLibraries.md) | Wrapping a third-party C++ library as an Alpakit module — directory layout, `.uplugin`, `Build.cs`, and OpenSSL worked example (developers) |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
