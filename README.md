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

**Player targeting** — commands that accept `<player|UID>` resolve the target in this order:
1. Compound UID starting with `EOS:` — used directly.
2. Raw 32-character hex string — `EOS:` prefix added automatically.
3. Display name — case-insensitive substring match against **currently connected** players. Use a UID or `/banname` for offline players.

| Command | Admin | Description |
|---------|:-----:|-------------|
| `/ban <player\|UID\|IP:address> [reason]` | ✅ | Permanently ban a player, EOS UID, or IP address |
| `/tempban <player\|UID\|IP:address> <minutes> [reason]` | ✅ | Temporarily ban for N minutes (auto-lifted on expiry) |
| `/unban <UID\|IP:address>` | ✅ | Remove a ban by EOS UID or IP address (not display name) |
| `/unbanname <name_substring>` | ✅ | Remove ban for an offline player by display-name — also removes their IP ban if recorded |
| `/bancheck <player\|UID\|IP:address>` | ✅ | Show ban status, reason, expiry, and linked UIDs |
| `/banlist [page]` | ✅ | List all active bans, 10 per page |
| `/linkbans <UID1> <UID2>` | ✅ | Link two EOS UIDs so a ban on either blocks the player |
| `/unlinkbans <UID1> <UID2>` | ✅ | Remove a previously created UID link |
| `/playerhistory <name\|UID>` | ✅ | Search session history by display name or UID (up to 20 results) |
| `/banname <name_substring> [reason]` | ✅ | Ban offline player by name: bans EOS PUID + IP and links them |
| `/reloadconfig` | ✅ | Hot-reload admin config without restarting the server |
| `/whoami` | ❌ | Show your own EOS PUID — open to all players |

#### Common workflows

**Ban an online player**
```
/ban SomePlayer Griefing
```

**Ban an offline player by name (EOS + IP together)**
```
/banname SomePlayer Griefing
```

**Temporary ban (e.g. 24 hours = 1440 minutes)**
```
/tempban SomePlayer 1440 Toxic behaviour
```

**Ban directly by EOS UID or IP**
```
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban IP:1.2.3.4 VPN evader
/tempban IP:1.2.3.4 60 Suspicious traffic
```

**Unban a player**
```
/unban EOS:00020aed06f0a6958c3c067fb4b73d51
/unban IP:1.2.3.4
/unbanname SomePlayer
```

**Handle ban evasion (new EOS PUID)**
```
; 1. Find the new UID from session history
/playerhistory SomePlayer
; 2. Ban the new UID
/ban EOS:00020aed06f0a6958c3c067fb4b73d52 Cheating (ban evasion)
; 3. Link the two bans so either identity is blocked
/linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:00020aed06f0a6958c3c067fb4b73d52
```

**Find your own EOS PUID (to become an admin)**
```
/whoami
```

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
