# BanSystem – Getting Started

← [Back to index](README.md)

This guide covers how to install BanSystem, where the config and data files live, and how to verify the mod is working.

---

## Step 1 – Install the mod

The easiest way is through the **Satisfactory Mod Manager (SMM)**:

1. Download and install [SMM](https://smm.ficsit.app/) if you have not done so already.
2. Open SMM, select your Satisfactory installation, then search for **BanSystem**.
3. Click **Install**. SMM handles SML automatically.
4. Launch / restart your dedicated server. BanSystem loads automatically.

> **Manual install**
> Copy the extracted `BanSystem/` folder into
> `<ServerRoot>/FactoryGame/Mods/` alongside SML.
> Restart the server afterwards.

---

## Step 2 – (Optional) Install BanChatCommands

BanChatCommands adds in-game chat commands (`/ban`, `/tempban`, `/whoami`, etc.) so you can manage bans from inside the game without needing the REST API.

1. Search for **BanChatCommands** in SMM and install it alongside BanSystem.
2. Open `<ServerRoot>/FactoryGame/Config/DefaultGame.ini` and add your admin player IDs:

```ini
[/Script/BanChatCommands.BanChatCommandsConfig]
+AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
```

Use `/whoami` in-game to find your own compound UID and identify the correct format for your account.

→ See [In-Game Commands](03-ChatCommands.md) for the full command reference.

---

## Where are the data files?

After the first server start, BanSystem creates the following files:

```
<ProjectSaved>/BanSystem/bans.json            ← ban records
<ProjectSaved>/BanSystem/player_sessions.json ← player session audit log
```

| OS | `<ProjectSaved>` resolves to |
|----|------------------------------|
| Windows | `C:\SatisfactoryServer\FactoryGame\Saved\` |
| Linux | `~/.config/Epic/FactoryGame/Saved/` |

These directories are **not** cleared on mod updates or server restarts — bans persist automatically.

---

## Where is the config file?

```
<ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
```

Edit this file, then restart the server. The defaults are sensible for most setups — you only need to change `RestApiPort` if port 3000 is already in use, or `DatabasePath` if you want the ban file stored somewhere specific.

→ See [Configuration](02-Configuration.md) for every available setting.

---

## Verifying BanSystem is running

Check the server log (`FactoryGame.log`) for lines starting with `LogBanDatabase` and `LogBanEnforcer`:

```
LogBanDatabase: Database loaded. Active bans: 0
LogBanEnforcer: PostLogin hook registered.
LogBanRestApi: REST API listening on port 3000
```

If you see these lines, BanSystem is running correctly. You can also call `GET /health` on the REST API to confirm:

```sh
curl http://localhost:3000/health
# → {"status":"ok"}
```

---

## Banning your first player

From the in-game chat (requires BanChatCommands and admin rights):

```
/ban 00020aed06f0a6958c3c067fb4b73d51 Cheating
/ban SomePlayer Griefing
/tempban SomePlayer 60 Spamming (1-hour ban)
```

From the REST API:

```sh
curl -X POST http://localhost:3000/bans \
  -H "Content-Type: application/json" \
  -d '{"playerUID":"00020aed06f0a6958c3c067fb4b73d51","platform":"EOS","reason":"Cheating","bannedBy":"admin"}'
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
