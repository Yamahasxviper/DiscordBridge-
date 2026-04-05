# BanChatCommands – Configuration

← [Back to index](README.md)

---

## Config file location

```
<ServerRoot>/FactoryGame/Config/DefaultGame.ini
```

BanChatCommands reads its settings from the `[/Script/BanChatCommands.BanChatCommandsConfig]` section of `DefaultGame.ini`.

---

## Admin list

Only players whose EOS Product User ID appears in the admin list can run `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist`, `/linkbans`, `/unlinkbans`, and `/playerhistory`. The `/whoami` command is always open to everyone.

Commands issued from the **server console** bypass the admin list entirely.

> **Note:** On CSS Dedicated Server, all players are identified by their EOS Product User ID regardless of their launch platform (Steam, Epic, etc.). Use `/whoami` in-game to find the PUID for any player.

### Adding admins

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

Add one `+AdminEosPUIDs=` line per admin. The value is the player's **EOS Product User ID** (32-character hex string, case-insensitive).

**How to find an EOS PUID:** Have the player type `/whoami` in-game. The reply will be `[BanChatCommands] Your EOS PUID: 00020aed...` — use only the hex characters.

---

## Empty admin list

If the list is empty, **no player** can run admin commands from chat — only the server console can. This is the default state before you add any admins.

---

## Applying changes

Restart the dedicated server after editing `DefaultGame.ini`. Changes are not hot-reloaded at runtime.
