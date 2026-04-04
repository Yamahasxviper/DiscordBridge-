# BanChatCommands – Configuration

← [Back to index](README.md)

---

## Config file location

Settings are stored in the mod's own config file, **not** the project `DefaultGame.ini`.  
The recommended way to configure without being overwritten by mod updates is to use the server override file:

```
<ServerRoot>/FactoryGame/Saved/Config/<Platform>/BanChatCommands.ini
```

Alternatively you can edit the mod's default template directly (note: this file is overwritten on mod updates):

```
<ServerRoot>/FactoryGame/Mods/BanChatCommands/Config/DefaultBanChatCommands.ini
```

BanChatCommands reads its settings from the `[/Script/BanChatCommands.BanChatCommandsConfig]` section.

---

## Admin list

Only players whose platform ID appears in the admin list can run `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist`, `/linkbans`, `/unlinkbans`, and `/playerhistory`. The `/whoami` command is always open to everyone.

Commands issued from the **server console** bypass the admin list entirely.

### Steam admins

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminSteam64Ids=76561198000000000
+AdminSteam64Ids=76561198111111111
```

Add one `+AdminSteam64Ids=` line per admin. The value is the player's **Steam 64-bit ID** (17-digit decimal number).

**How to find a Steam 64-bit ID:** Have the player type `/whoami` in-game. The reply will be `STEAM:76561198000000000` — use only the digits after the colon.

### EOS admins

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

Add one `+AdminEosPUIDs=` line per admin. The value is the player's **EOS Product User ID** (32-character hex string, case-insensitive).

**How to find an EOS PUID:** Have the player type `/whoami` in-game. The reply will be `EOS:00020aed...` — use only the hex characters after the colon.

### Combined example

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminSteam64Ids=76561198000000000
+AdminSteam64Ids=76561198111111111
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

---

## Empty admin list

If both lists are empty, **no player** can run admin commands from chat — only the server console can. This is the default state before you add any admins.

---

## Applying changes

Restart the dedicated server after editing the config file. Changes are not hot-reloaded at runtime.
