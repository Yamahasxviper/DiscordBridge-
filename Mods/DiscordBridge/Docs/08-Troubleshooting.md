# DiscordBridge – Troubleshooting

← [Back to index](README.md)

---

## `!server` / `!admin` commands are not recognised

1. Check that the relevant prefix setting (`ServerInfoCommandPrefix`,
   `ServerControlCommandPrefix`) is set and not empty.
2. For `!admin` commands, verify that `ServerControlCommandRoleId` is set and
   that the Discord user holds that role.  All control commands are disabled for
   everyone while `ServerControlCommandRoleId` is empty.
3. Look in the server log (`FactoryGame.log`) for `LogDiscordBridge` lines that
   may explain why a command was rejected.

---

## The server restarts after `!admin stop`

Your process supervisor is configured to restart the server regardless of exit
code.  `!admin stop` exits with code **0** (clean stop) and `!admin restart`
exits with code **1** (restart requested).

**Fix:** Change the restart policy from `always` to `on-failure`:

- **systemd**: set `Restart=on-failure` in the `[Service]` section.
- **Docker**: use `--restart on-failure`.
- **Shell wrapper**: check `$?` after the server exits and break the loop if it
  is `0`.

---

## The bridge does not start / no messages are relayed

1. Check that `BotToken` and `ChannelId` are both set in the config file.
2. Confirm the bot is **in your Discord server** and has **Send Messages** and
   **Read Message History** permissions in the target channel.
3. Make sure both **Privileged Gateway Intents** are enabled in the Discord
   Developer Portal (Server Members, Message Content).
4. Look in the server log (`FactoryGame.log`) for lines starting with `LogDiscordBridge`
   – they will contain details about any connection errors.

---

## EOS platform not available / `!ban id` or `!server eos` does not work

DiscordBridge uses the CSS-native **OnlineIntegration** plugin (`Plugins/Online/OnlineIntegration`)
for all EOS platform detection and player platform-ID resolution.  This plugin ships
with the Satisfactory dedicated server and does **not** need to be installed separately.

If EOS platform state cannot be resolved you will see a warning in `FactoryGame.log`
and `!server eos` will report the reason.  Common causes:

- `DefaultPlatformService` is set to `None` in `DefaultEngine.ini` — EOS PUID ban
  enforcement is unavailable, but Steam64 bans are still enforced via direct ID
  resolution.
- The EOS session has not yet fully initialised at join time.  The 60-second PUID
  followup check will retry automatically.

Run `!server eos` in Discord or game chat to see a full platform diagnostic with
step-by-step fix instructions.

---

## Build errors when compiling from source with Alpakit

DiscordBridge is built against Coffee Stain Studios' custom Unreal Engine
build (**UnrealEngine-CSS**, UE 5.3) using the **Alpakit** mod packaging
pipeline.  Some standard UE modules are unavailable or behave differently in
this environment.

| Symptom | Cause | Fix |
|---------|-------|-----|
| `fatal error: 'OnlineSubsystem.h' file not found` | `"OnlineSubsystem"` added to `Build.cs` | Remove it; use `GConfig->GetString()` on `GEngineIni` instead (see [Build System guide](00-BuildSystem.md)) |
| `fatal error: <engine header> not found` | Missing `DummyHeaders` dependency | Add `"DummyHeaders"` to `PublicDependencyModuleNames` in `Build.cs` |
| Discord bridge refuses to link / WebSocket symbols missing | `SMLWebSocket` not listed as a dependency | Add `"SMLWebSocket"` to `PublicDependencyModuleNames` |
| Runtime `SIGSEGV` at `0x0000000006000001` | EOS SDK called before `mIsOnline=true` or `GetNumLocalPlayers()>0` | Use `OnlineIntegration` helpers with the required guards (see [Build System guide](00-BuildSystem.md)) |

See the [Build System guide](00-BuildSystem.md) for a full explanation of CSS
UE module constraints and recommended `Build.cs` settings.

---

## SMLWebSocket is missing / the bridge fails to connect to Discord

DiscordBridge uses SMLWebSocket to establish the secure WebSocket connection (WSS)
to Discord's gateway. If SMLWebSocket is not installed the bridge cannot connect and
you will see errors in `FactoryGame.log` similar to:

```
LogDiscordBridge: Error: Failed to create WebSocket – SMLWebSocket module not found
```

**Fix:**

- **Using SMM:** re-install DiscordBridge through the Satisfactory Mod Manager;
  it will detect and install the missing SMLWebSocket dependency automatically.
- **Manual install:** download SMLWebSocket from ficsit.app and copy the
  `SMLWebSocket/` folder into `<ServerRoot>/FactoryGame/Mods/` alongside
  `DiscordBridge/`. Restart the server afterwards.

---

## Messages go one way only (game → Discord works, Discord → game doesn't)

- Verify **Message Content Intent** is enabled. Without it Discord does not send
  message content to bots, so the bridge cannot read Discord messages.

---

## The bot shows "offline" in Discord even while the server is running

- Discord caches presence state. Wait up to a minute or try restarting your Discord client.
- Make sure `ShowPlayerCountInPresence=True` and `PlayerCountPresenceFormat` is
  not empty.

---

## Messages from other bots are relayed into the game (echo loop)

- Set `IgnoreBotMessages=True` (this is the default). This drops messages from
  any Discord account that has the `bot` flag set.

---

## The config gets reset after a mod update

- This is expected behaviour for the primary config files after a mod update resets
  them. The mod automatically backs up each config file separately on every server
  start and restores your settings from the matching backup on the next start:

  | Backup file | Mirrors |
  |-------------|---------|
  | `Saved/Config/DiscordBridge.ini` | `DefaultDiscordBridge.ini` |
  | `Saved/Config/Whitelist.ini` | `DefaultWhitelist.ini` |
  | `Saved/Config/Ban.ini` | `DefaultBan.ini` |
  | `Saved/Config/Tickets.ini` | `DefaultTickets.ini` |

  If you want an extra safety net, keep a separate copy of your `BotToken` and
  `ChannelId` somewhere secure.

---

## Whitelist commands are not recognised / players are not being kicked

1. Make sure `WhitelistCommandPrefix` is set (default is `!whitelist`) and not empty.
2. Confirm the whitelist is **enabled** (`!whitelist status` in the Discord channel).
3. If using `WhitelistRoleId`, verify the bot has the **Manage Roles** permission on your Discord server.
4. Players are only kicked on **join** – the whitelist is checked when a player connects, not while they are already in the game.

---

## Ban commands are not recognised / banned players can still join

1. Make sure `BanCommandPrefix` is set (default is `!ban`) and not empty.
2. Run `!ban status` in the Discord channel to confirm the ban system is **enabled**. If it shows disabled, run `!ban on` or set `BanSystemEnabled=True` in `DefaultBan.ini` and restart.
3. `BanSystemEnabled` in `DefaultBan.ini` is applied on **every** server restart — set it to `True` or `False` and restart to change the ban system state.
4. When the ban system is **enabled**, `!ban add <name>` kicks the player immediately if they are already connected. `!ban on` also immediately kicks any currently-connected banned players. Players who are not yet connected are kicked the next time they attempt to join.

---

## Platform ID bans are not working — `!server eos` shows "Platform operational: NO"

If `!server eos` or `!ban status` shows output similar to:

```
Platform service (config):  None (explicit no-platform configuration)
Detected type:              None
Platform operational (live): NO

Runtime (live)
OnlineIntegrationSubsystem: present
Session manager:            present
Local EOS users:            0

 No online platform configured (DefaultPlatformService=None).
```

This means `DefaultPlatformService=None` is set in your server's `DefaultEngine.ini`, which
disables EOS platform services and prevents the ban system from resolving player platform IDs
(EOS PUIDs) at join time.

**Fix:** Set the following in your server's `DefaultEngine.ini`:

```ini
[OnlineSubsystem]
DefaultPlatformService=EOS
NativePlatformService=Steam
```

- `DefaultPlatformService=EOS` — tells the CSS engine to use Epic Online Services as the
  primary online platform.  Every player receives an EOS Product User ID (PUID) that the ban
  system uses to enforce bans at join time.
- `NativePlatformService=Steam` — enables the Steam→EOS bridge so Steam players are
  transparently mapped to an EOS PUID.  Without this, Steam players may not receive a PUID.

Restart the server after making this change, then run `!server eos` or `!ban status` in
Discord and confirm the output shows `Platform operational (live): YES`.

> **Why not `DefaultPlatformService=Steam`?**
> Satisfactory's custom Unreal Engine build (UnrealEngine-CSS) explicitly disables
> `OnlineSubsystemSteam` on dedicated server builds — it is listed in
> `OnlineIntegration.uplugin` as `TargetDenyList: Server`.  `DefaultPlatformService=Steam`
> therefore does **not** work on a CSS dedicated server and is a misconfiguration.  Always
> use `DefaultPlatformService=EOS` + `NativePlatformService=Steam` on CSS servers.

---

## In-game commands are not recognised

1. Make sure `InGameWhitelistCommandPrefix` (in `DefaultWhitelist.ini`) and `InGameBanCommandPrefix` (in `DefaultBan.ini`) are set (both default to `!whitelist` and `!ban` respectively) and not empty.
2. In-game commands can only be typed in the **Satisfactory in-game chat** by players who are already connected to the server.
3. In-game whitelist commands do not include `!whitelist role add/remove` — that is Discord-only.
4. If the command appears to do nothing, check the server log (`FactoryGame.log`) for `LogDiscordBridge` or `LogWhitelistManager` / `LogBanManager` lines that may explain why it was rejected.

---

## SML chat commands (starting with `/`) do not work from Discord

SML chat commands such as `/save`, `/tp`, or any command prefaced with a forward
slash must be **typed directly in the Satisfactory in-game chat window** by a real
player.  They cannot be sent through the Discord bridge.

- Sending a `/`-prefixed message in Discord will produce an informational reply
  from the bot and the message will **not** be forwarded to the game.
- Similarly, if a player types an SML command in-game, it is **not** forwarded
  to Discord — it is treated as a private command, not a chat message.
- SML chat commands also do **not** work when typed in the Unreal Engine server
  console (the terminal or RCON interface). They must be entered in the game's
  chat UI.

**Bot commands are not affected** — `!whitelist`, `!ban`, `!server`, and `!admin`
(and any custom prefix you configure) are matched and handled before the `/` filter
runs. Even if you configure a custom bot-command prefix that starts with `/` (for
example `WhitelistCommandPrefix=/wl`), those commands continue to work normally.

For a full list of available SML chat commands see:
<https://docs.ficsit.app/satisfactory-modding/latest/SMLChatCommands.html>

---

## Log verbosity

DiscordBridge writes a **dedicated log file** on every server session:

```
<ServerRoot>/FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log
```

This file captures all output from the `LogDiscordBridge`, `LogSMLWebSocket`,
`LogBanManager`, and `LogWhitelistManager` categories in one place, with UTC
timestamps to the millisecond.  The file is opened in append mode so multiple
server restarts accumulate in a single file — each session boundary is marked:

```
[2026.03.11-12.00.00 UTC] ===== DiscordBot session started =====
...
[2026.03.11-12.30.00 UTC] ===== DiscordBot session ended =====
```

Use `tail -f <ServerRoot>/FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log` to
monitor events in real time.  The full server log (`FactoryGame.log`) also
continues to receive these lines, so both sources are available.

To increase log detail, add the following to your server's `DefaultEngine.ini`:

```ini
[Core.Log]
LogDiscordBridge=Verbose
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
