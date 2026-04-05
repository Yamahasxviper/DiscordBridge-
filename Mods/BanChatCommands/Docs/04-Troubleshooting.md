# BanChatCommands – Troubleshooting

← [Back to index](README.md)

---

## Commands not registered / not responding

**Symptom:** Typing `/ban` or `/whoami` in chat has no effect.

**Checks:**

1. Confirm the log line at server startup:
   ```
   LogBanChatCommands: BanChatCommands: Registered 9 commands (ban, tempban, unban, bancheck, banlist, ...)
   ```
   If this line is absent, BanChatCommands did not load.

2. Verify that both `BanChatCommands` and `BanSystem` are in the server's `Mods/` folder.

3. Check the SML version. BanChatCommands requires `SML ^3.11.3`.

4. Restart the server and check for error messages at the top of the log file.

---

## "You do not have permission to use this command"

**Symptom:** Typing `/ban` returns a permission error.

**Cause:** Your platform ID is not in the admin list in `BanChatCommands.ini`.

**Fix:**

1. Type `/whoami` in-game to find your EOS PUID (e.g. `00020aed06f0a6958c3c067fb4b73d51`).
2. Add the raw ID to `Saved/Config/<Platform>/BanChatCommands.ini`:
   ```ini
   [/Script/BanChatCommands.BanChatCommandsConfig]
   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
   ```
3. Restart the server.

See [Configuration](02-Configuration.md) for the full admin setup guide.

---

## /whoami returns nothing

**Symptom:** `/whoami` produces no response.

**Cause:** The mod may not have loaded, or the chat command subsystem is unavailable.

**Fix:** Check the startup log for `LogBanChatCommands` entries. If you see a warning like:
```
LogBanChatCommands: Warning: AChatCommandSubsystem not available — commands not registered.
```
this means SML's chat system was not ready when BanChatCommands initialised. Ensure your SML version is `^3.11.3` or newer.

---

## Ban not taking effect after /ban

**Symptom:** You ran `/ban` and received a success message, but the player was not kicked and can reconnect.

**Cause:** BanChatCommands only stores the ban record. Enforcement is handled by **BanSystem**. If BanSystem is missing or failed to start, bans are stored but not enforced.

**Fix:** Check that BanSystem is loaded and its log shows:
```
LogBanEnforcer: BanEnforcer: login enforcement active (PostLogin)
```
See the [BanSystem Troubleshooting guide](../../BanSystem/Docs/06-Troubleshooting.md) for further steps.

---

## Config changes not taking effect

**Symptom:** You edited `BanChatCommands.ini` but the change has no effect.

**Fix:** The config is read at server startup. **Restart the server** after every edit to the config file.

---

## Multiple players matched for a display name

**Symptom:** `/ban SomeName` replies with a list of matched players instead of banning.

**Cause:** More than one connected player's display name contains `SomeName` as a substring.

**Fix:** Use the full compound UID instead:
```
/ban EOS:00020aed06f0a6958c3c067fb4b73d51 Reason
```
Type `/whoami` to get your own EOS PUID, or use `/playerhistory <name>` to look up a past UID for a disconnected player.
