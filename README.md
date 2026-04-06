# SatisfactoryModLoader [![CI](https://github.com/satisfactorymodding/SatisfactoryModLoader/actions/workflows/build.yml/badge.svg)](https://github.com/satisfactorymodding/SatisfactoryModLoader/actions/workflows/build.yml)

A tool used to load mods for the game Satisfactory. After Coffee Stain releases a proper Unreal modding API the project will continue as a utilities library.

This repository contains the SatisfactoryModLoader source code,
an ExampleMod with demos of some of the SML utilities,
a collection of editor utilities,
and more.
It also serves as the Unreal project used for developing mods.

---

## Custom Mods

This fork includes several custom server-side mods built on top of SML:

### [BanSystem](Mods/BanSystem/README.md)

Persistent EOS-based ban system for Satisfactory dedicated servers.

- Login-time ban enforcement (EOS PUID, not display name)
- Permanent and temporary bans with auto-expiry
- UID linking — ban one identity, block all linked identities
- Player session registry (`player_sessions.json`) for audit and UID lookup
- REST management API (default port 3000)
- JSON file storage that survives restarts and mod updates

→ [Full documentation](Mods/BanSystem/Docs/README.md)

---

### [BanChatCommands](Mods/BanChatCommands/README.md)

In-game chat command interface for BanSystem. Requires BanSystem.

| Command | Description |
|---------|-------------|
| `/ban <player\|UID> [reason]` | Permanently ban a player |
| `/tempban <player\|UID> <minutes> [reason]` | Temporarily ban for N minutes |
| `/unban <UID>` | Remove a ban |
| `/bancheck <player\|UID>` | Query ban status |
| `/banlist [page]` | List active bans (10 per page) |
| `/linkbans <UID1> <UID2>` | Link two EOS UIDs for cross-identity enforcement |
| `/unlinkbans <UID1> <UID2>` | Remove a UID link |
| `/playerhistory <name\|UID>` | Look up session history |
| `/banname <name> [reason]` | Ban offline player by name + IP from session history |
| `/reloadconfig` | Hot-reload admin config without restarting the server |
| `/whoami` | Show your own EOS PUID (no admin required) |

→ [Full documentation](Mods/BanChatCommands/Docs/README.md)

---

### [SMLWebSocket](Mods/SMLWebSocket/README.md)

RFC 6455 WebSocket client with SSL/OpenSSL support for Satisfactory mods.

- `ws://` and `wss://` (plain TCP and TLS via OpenSSL)
- Auto-reconnect with exponential back-off
- Thread-safe delegate callbacks fired on the game thread
- Blueprint and C++ API
- Used by mods that need persistent WebSocket connections (e.g. DiscordBridge)

→ [README](Mods/SMLWebSocket/README.md)

---

## Documentation

Learn how to set up and use this repo on the [modding documentation](https://docs.ficsit.app/).

## Discord Server

Join our [discord server](https://discord.gg/QzcG9nX) to talk about SML and Satisfactory Modding in general.

## DISCLAIMER

This software is provided by the author "as is". In no event shall the author be liable for any direct, indirect, incidental, special, exemplary, or consequential damages (including, but not limited to procurement of substitute goods or services; loss of use, data, or profits; or business interruption) however caused and on any
theory of liability, whether in contract, strict liability, or tort (including negligence or otherwise) arising in any way out of the use of this software, even if advised of the possibility of such damage.
