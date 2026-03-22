# DiscordBridge – Getting Started

← [Back to index](README.md)

This guide covers how to install the mod, where the configuration file lives, and how to create the Discord bot that powers the bridge.

---

## Step 0 – Install the mod

> **Server-only mod — players do not need to install anything.**
> DiscordBridge and its dependency SMLWebSocket are both flagged as
> `RequiredOnRemote = false` in their mod descriptors.  This means the
> Satisfactory Mod Manager (SMM) will **never** prompt players who connect to
> your server to download or install either mod.  Only the **server operator**
> needs to install these mods; your players can join with a completely vanilla
> (unmodded) game client.

The easiest way to install DiscordBridge is through the **Satisfactory Mod Manager (SMM)**:

1. Download and install [SMM](https://smm.ficsit.app/) if you have not done so already.
2. Open SMM, select your **server** installation, then search for **DiscordBridge** in the mods list.
3. Click **Install**. SMM handles all dependencies (SML, SMLWebSocket) automatically.
4. Launch / restart your dedicated server. The mod loads automatically on startup.

> **Dedicated server without SMM**
> Copy the extracted mod folder (`DiscordBridge/`) into
> `<ServerRoot>/FactoryGame/Mods/` manually.
> Ensure SML and SMLWebSocket are present in that same `Mods/` directory.
>
> **Why do I need SMLWebSocket?**
> DiscordBridge communicates with Discord over a secure WebSocket connection (WSS /
> RFC 6455). Satisfactory uses a custom Coffee Stain Studios build of Unreal Engine
> (UnrealEngine-CSS, UE 5.3) and Unreal's built-in WebSocket module is **not
> available** in Alpakit-packaged mods targeting this engine. SMLWebSocket supplies
> a custom WebSocket + SSL/OpenSSL client that DiscordBridge depends on. The bridge
> cannot connect to Discord without it — you will see connection errors in
> `FactoryGame.log` if it is missing.

> **Developer note — building from source**
> If you are building this mod from source using Alpakit, see the
> [Build System guide](00-BuildSystem.md) for details on CSS UnrealEngine
> module constraints, why `OnlineSubsystem` headers are unavailable to mods,
> and the recommended `Build.cs` settings.

---

## Where is the config file?

After installing the mod the mod's `Config/` folder contains **four primary config files**:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultWhitelist.ini
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultBan.ini
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultTickets.ini
```

| File | What it controls |
|------|-----------------|
| `DefaultDiscordBridge.ini` | Connection, chat bridge, server status, presence, server info, server control |
| `DefaultWhitelist.ini` | Whitelist toggle, command role/prefix, kick messages |
| `DefaultBan.ini` | Ban system toggle, command role/prefix, kick messages |
| `DefaultTickets.ini` | Ticket system prefix, channels, per-type toggles, notify role |

Edit the relevant file(s), then restart the server. Your changes take effect on the next start.

> **Tip – surviving mod updates**
> The mod automatically writes a dedicated backup of each config file to
> `<ServerRoot>/FactoryGame/Saved/Config/` every time the server starts:
>
> | Backup file | Mirrors |
> |-------------|---------|
> | `Saved/Config/DiscordBridge.ini` | `DefaultDiscordBridge.ini` |
> | `Saved/Config/Whitelist.ini` | `DefaultWhitelist.ini` |
> | `Saved/Config/Ban.ini` | `DefaultBan.ini` |
> | `Saved/Config/Tickets.ini` | `DefaultTickets.ini` |
>
> If a mod update resets any primary config file, the mod restores your settings
> from the matching backup automatically on the next server start.

---

## Step 1 – Create a Discord Bot

1. Go to <https://discord.com/developers/applications> and click **New Application**.
2. Give it a name (e.g. *My Satisfactory Bot*), then open the **Bot** tab.
3. Click **Reset Token** and copy the token – paste it as `BotToken` in the config.
4. Under **Privileged Gateway Intents** enable:
   - **Server Members Intent**
   - **Message Content Intent**
5. Under **OAuth2 → URL Generator** tick `bot`, then tick the permissions
   **Send Messages** and **Read Message History**.
6. Open the generated URL in a browser and invite the bot to your server.
7. Enable **Developer Mode** in Discord (User Settings → Advanced), right-click
   the target text channel, and choose **Copy Channel ID**. Paste it as `ChannelId`.

> **Tip:** Throughout this documentation the term **"Discord ID"** means a Discord numeric ID (the 18-19 digit number you copy from Discord). Every Discord object — channel, role, user — has one. See [Connection Settings](02-ConnectionSettings.md) for more detail.

---

## Next steps

Once your bot is created, configure the connection in [Connection Settings](02-ConnectionSettings.md).

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
