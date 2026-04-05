# BanSystem – Troubleshooting

← [Back to index](README.md)

---

## Banned player was not kicked when they joined

1. Check the server log (`FactoryGame.log`) for `LogBanEnforcer` lines. They will show whether the player's UID was resolved and whether the ban check ran.
2. Verify the UID format in `bans.json` — UIDs must be in the compound format `EOS:xxx` (with the platform prefix). A raw EOS PUID without the `EOS:` prefix will not match.
3. If the player may have changed their EOS PUID (ban evasion), ban the new UID and link them with `/linkbans`.
4. CSS Satisfactory uses asynchronous EOS identity resolution. BanSystem polls for up to 20 s after PostLogin — check the log for `[BanEnforcer] Pending identity check` messages to confirm the poll is running.
5. Make sure BanSystem is actually loaded: look for `LogBanDatabase: Database loaded` near the start of the log.

---

## `bans.json` was not created

1. Ensure the server has write permission to the `Saved/` directory.
2. If `DatabasePath` is set to a custom path, verify the directory exists and is writable.
3. Check the log for `LogBanDatabase: Error` lines.

---

## REST API is not reachable

1. Confirm `RestApiPort` is not `0` in `DefaultBanSystem.ini`.
2. Check that nothing else is already listening on the configured port.
3. If you are accessing from another machine, verify your firewall allows the port.
4. Look in the log for `LogBanRestApi: REST API listening on port ...` to confirm the server started.

---

## In-game commands are not recognised

1. Make sure the **BanChatCommands** mod is installed alongside BanSystem.
2. Check the server log for `LogBanChatCommands` lines.
3. Verify your EOS PUID is in `BanChatCommands.ini` under `[/Script/BanChatCommands.BanChatCommandsConfig]`.
4. Use `/whoami` to confirm your UID. Then check it matches what is in the config file exactly (correct platform prefix, correct casing for EOS PUIDs).

---

## `/ban SomeName` says "player not found"

Display name resolution only works for **currently connected** players. If the player has disconnected, use their EOS PUID directly:

```
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Reason
```

Use `/playerhistory SomeName` to find a past UID from the session registry.

---

## Bans are lost after a mod update

Bans are stored in `Saved/BanSystem/bans.json`, which is outside the mod folder and is never touched by mod updates. If bans are missing after an update:

1. Check that `DatabasePath` in `DefaultBanSystem.ini` is either empty (uses the default Saved path) or points to the same path as before.
2. Verify the file still exists at the expected location.

---

## Log verbosity

Add the following to your server's `DefaultEngine.ini` to increase log detail:

```ini
[Core.Log]
LogBanDatabase=Verbose
LogBanEnforcer=Verbose
LogBanRestApi=Verbose
LogBanChatCommands=Verbose
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
