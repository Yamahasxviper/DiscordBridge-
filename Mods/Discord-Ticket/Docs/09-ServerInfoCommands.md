# DiscordBridge – Server Info Commands (`!server`)

← [Back to index](README.md)

The server info command group lets **any member** of the bridged Discord channel
query the game server without needing an admin role.  The same commands are also
available **in-game** (typed directly into the game chat by any connected player),
with responses posted back into game chat.

---

## Config file

All server info command settings live in the primary config file:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
```

This file ships with the mod and is created automatically on the first server start
if it is missing. Edit the file and restart the server to apply changes.

> **Backup:** the mod writes `<ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini` on every
> server start.  If a mod update resets `DefaultDiscordBridge.ini`, the backup is used to
> restore your settings automatically.

> **Other config files:** whitelist, ban, and ticket settings each have their own dedicated
> file in the same `Config/` folder.  See [Getting Started](01-GettingStarted.md) for the
> full list.

---

## Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `ServerInfoCommandPrefix` | string | `!server` | The prefix that triggers server info commands in the **bridged Discord channel**. Set to an **empty string** to disable Discord-side server info commands entirely. |
| `InGameServerInfoCommandPrefix` | string | `!server` | The prefix that triggers server info commands typed in **game chat**. Set to an **empty string** to disable in-game server info commands entirely. Both settings default to `!server` and can be changed independently. |

---

## Supported commands

These commands work both in the **bridged Discord channel** (using `ServerInfoCommandPrefix`)
and in **game chat** (using `InGameServerInfoCommandPrefix`).  Responses in Discord are
formatted with markdown; responses in game chat are plain text.

| Command | Effect |
|---------|--------|
| `!server players` | List the in-game names of all currently online players |
| `!server online` | Alias for `!server players` |
| `!server status` | Show the server name, online/offline state, and player count; includes a warning if the EOS platform is unavailable |
| `!server eos` | Run a full EOS platform diagnostic and display ban-ID enforcement state |
| `!server help` | List all available commands |

> **No role required.** These commands are intentionally open to all members of
> the bridged channel and to any player in game chat.  To prevent Discord-side abuse,
> restrict which Discord members can see or post in the bridged channel using Discord's
> own channel permissions.

---

## In-game usage

Any player connected to the server can type `!server <subcommand>` directly in game chat
and receive an immediate in-game reply.

```
!server players   → "3 player(s) online: Alice, Bob, Carol"
!server status    → "My Server is online | Players: 3"
!server eos       → (full EOS/platform diagnostic in plain text)
!server help      → list of available subcommands
```

> **Note:** the in-game `!server eos` output uses plain text without Discord markdown
> (no `:emoji:` codes, bold formatting, etc.) so it reads cleanly in the game chat window.

---

## EOS diagnostics (`!server eos`)

The `!server eos` command runs a live EOS platform diagnostic using the
**OnlineIntegration** plugin directly and posts the result to Discord (or game chat).  The report shows:

- Whether EOS credentials are present in the engine config
- Whether the EOS session interface initialised successfully at startup
- The number of platform IDs currently in the ban list
- Step-by-step fix instructions when EOS is unavailable

Use this command whenever `!server status` shows the `:warning: EOS platform
unavailable` notice, or whenever `!ban id` enforcement appears to have stopped
working.

---

## Example config

```ini
; Default prefix – all members can type !server status, !server players, etc.
ServerInfoCommandPrefix=!server

; Disable Discord server info commands entirely
ServerInfoCommandPrefix=

; In-game server info command prefix (defaults to !server, same as Discord)
InGameServerInfoCommandPrefix=!server

; Disable in-game server info commands entirely
InGameServerInfoCommandPrefix=
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
