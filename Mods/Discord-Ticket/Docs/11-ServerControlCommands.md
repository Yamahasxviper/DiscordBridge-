# DiscordBridge – Server Control Commands (`!admin`)

← [Back to index](README.md)

The server control command group lets **authorised** Discord members stop or
restart the dedicated server directly from the bridged channel, and post the
ticket button panel.
These commands are **deny-by-default**: they are disabled for everyone until
`ServerControlCommandRoleId` is set.

> **Warning:** Only grant the control role to fully trusted administrators.
> `!admin stop` and `!admin restart` immediately affect the live server.

---

## Config file

All server control command settings live in the primary config file:

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
| `ServerControlCommandPrefix` | string | `!admin` | The prefix that triggers server control commands in the bridged Discord channel. Set to an **empty string** to disable all control commands. |
| `ServerControlCommandRoleId` | string | *(empty)* | Discord role ID whose members are allowed to run control commands. **Required** — while empty, all control commands are disabled for everyone. |

---

## Supported commands

| Command | Exit code | Effect |
|---------|-----------|--------|
| `!admin start` | *(no exit)* | Replies that the server is already online |
| `!admin stop` | `0` | Gracefully shuts down the server process |
| `!admin restart` | `1` | Gracefully shuts down the server so the process supervisor restarts it |
| `!admin ticket-panel` | *(no exit)* | Posts the ticket button panel to `TicketPanelChannelId` (or the main channel) |

---

## Process supervisor configuration

DiscordBridge signals intent to the process supervisor through the server exit code:

- **`!admin stop`** → exit code **0** (clean, intentional stop — do **not** restart)
- **`!admin restart`** → exit code **1** (restart requested)

For the correct behaviour, configure your supervisor to restart only on a
non-zero exit code (`on-failure`), **not** `always`.

**systemd** (`/etc/systemd/system/satisfactory.service`):
```ini
[Service]
Restart=on-failure
```

**Docker**:
```
--restart on-failure
```

**Shell wrapper**:
```bash
while true; do
    ./FactoryServer.sh
    [ $? -eq 0 ] && break   # exit code 0 = !admin stop; do not restart
done
```

> If you use `Restart=always` or `--restart always`, the server will restart
> after **both** `!admin stop` and `!admin restart`.

---

## How to get the role ID

Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode),
then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

---

## Ticket panel command

`!admin ticket-panel` posts the ticket button panel to the Discord channel
configured by `TicketPanelChannelId` (or the main bridged channel if that setting
is empty).  Members can then click a button to open a private support ticket
without typing any command.

> **Note:** The bot requires the **Manage Channels** permission at the server (or
> category) level for this command to create and delete private ticket channels.

For full ticket system configuration see [10-TicketSystem.md](10-TicketSystem.md).

---

## Example config

```ini
; Only members of role 123456789012345678 can run !admin commands
ServerControlCommandPrefix=!admin
ServerControlCommandRoleId=123456789012345678
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
