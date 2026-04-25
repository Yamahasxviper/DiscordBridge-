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

Only players whose EOS Product User ID appears in the admin list can run `/ban`, `/tempban`, `/unban`, `/bancheck`, `/banlist`, `/linkbans`, `/unlinkbans`, `/playerhistory`, `/warn`, `/warnings`, `/clearwarns`, `/announce`, `/stafflist`, and `/reason`. The `/whoami` and `/history` commands are always open to everyone.

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

## Moderator list

Moderators can run `/kick`, `/modban`, `/mute`, and `/unmute` but **cannot** run full admin commands. Admins automatically qualify as moderators.

```ini
+ModeratorEosPUIDs=aabbccdd11223344aabbccdd11223344
```

Add one `+ModeratorEosPUIDs=` line per moderator.

---

## `BanListPageSize`

Number of bans shown per page in `/banlist`.

**Default:** `10`

```ini
BanListPageSize=20
```

---

## `bCreateWarnOnKick`

When `True`, `/kick` automatically creates a warning entry for the kicked player
so that kick reasons are preserved in the warning history.

**Default:** `False`

```ini
bCreateWarnOnKick=True
```

---

## `ReloadConfigWebhookUrl`

Optional URL that `/reloadconfig` POSTs a notification to after reloading the
configuration. Useful for notifying an external dashboard. Leave empty to disable.

```ini
ReloadConfigWebhookUrl=https://dashboard.example.com/api/config-changed
```

---

## Empty admin list

If both lists are empty, **no player** can run admin or moderator commands from chat — only the server console can. This is the default state before you add any admins.

---

## Applying changes

Restart the dedicated server after editing the config file, or use the `/reloadconfig` chat command to hot-reload the admin/moderator lists without a restart. Changes applied via `/reloadconfig` take effect immediately — no server restart required.
