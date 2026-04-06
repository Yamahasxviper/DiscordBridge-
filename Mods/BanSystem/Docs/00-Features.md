# BanSystem – Features Overview

← [Back to index](README.md)

BanSystem is a server-only Satisfactory mod that enforces player bans on dedicated servers. It uses EOS Product User IDs rather than display names, so bans cannot be evaded by renaming.

---

## Login-time enforcement

Banned players are checked at the moment they connect — before they enter the game world. The enforcer hooks `PostLogin` and kicks the player synchronously when a matching ban is found. If EOS identity resolution is still in progress (CSS async auth), the enforcer polls every 0.5 s for up to 20 s before giving up.

Both **EOS PUID bans** and **IP address bans** are enforced at login time. The player's remote IP is captured via the `PreLogin` hook and checked against any `IP:` ban records in the database — so a banned player cannot evade by creating a new EOS account while their IP ban is still active.

---

## Permanent and temporary bans

Both permanent and timed bans are supported. Temporary bans carry an expiry timestamp. On server startup, all expired bans are pruned automatically so players whose ban elapsed while the server was offline are never incorrectly kicked.

---

## Cross-platform UID linking

A single player may connect and later change their EOS identity (e.g. after an account migration or evasion). `/linkbans` associates two compound UIDs so that a ban on either identity blocks the player regardless of which EOS PUID they log in with.

→ See [Cross-Platform Linking](05-CrossPlatform.md)

---

## Persistent JSON storage

All bans are stored in a single JSON file (`bans.json`) that survives server restarts and mod updates. The file is written immediately after every change.

---

## REST management API

A local HTTP server (default port 3000) provides the same management endpoints as the original Node.js Tools/BanSystem. Use it to integrate with external tools, dashboards, or scripts without needing in-game access.

→ See [REST API](04-RestApi.md)

---

## Player session registry

Every player UID, display name, and **remote IP address** seen at join time is written to `player_sessions.json`. Admins can query the registry with `/playerhistory` to look up past UIDs and IPs for a player — useful for IP banning, UID linking, or tracking evasion attempts.

---

## IP address banning

In addition to EOS PUID bans, BanSystem supports **IP address bans**. IP UIDs use the format `IP:<address>` (e.g. `IP:1.2.3.4`).

- The player's remote IP is captured via the `PreLogin` hook at every connection.
- If the player's IP matches an active `IP:` ban record, they are kicked — even if they log in under a completely new EOS account.
- IP bans can be created from in-game chat (`/ban IP:1.2.3.4 Reason`) or via the REST API (`"platform": "IP"`).
- Use `/banname` to ban an offline player by name and automatically add an IP ban if their IP was recorded in the session registry.

→ See [In-Game Commands](03-ChatCommands.md) for chat command IP ban examples.
→ See [REST API](04-RestApi.md) for REST IP ban examples.

---

## In-game chat commands (BanChatCommands mod)

The optional **BanChatCommands** mod provides `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist`, `/linkbans`, `/unlinkbans`, `/playerhistory`, `/banname`, `/reloadconfig`, and `/whoami` directly from the Satisfactory in-game chat.

→ See [In-Game Commands](03-ChatCommands.md)

---

## Requirements

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
