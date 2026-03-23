# DiscordBridge – Troubleshooting

← [Back to index](README.md)

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

- This is expected behaviour for the primary config. Your credentials are saved
  automatically to `<ServerRoot>/FactoryGame/Saved/Config/DiscordBridge.ini` each
  session and will be restored automatically. If you want to be safe, keep a
  separate copy of your `BotToken` and `ChannelId` somewhere secure.

---

## Whitelist commands are not recognised / players are not being kicked

1. Make sure `WhitelistCommandPrefix` is set (default is `!whitelist`) and not empty.
2. Confirm the whitelist is **enabled** (`!whitelist status` in the Discord channel).
3. If using `WhitelistRoleId`, verify the bot has the **Manage Roles** permission on your Discord server.
4. Players are only kicked on **join** – the whitelist is checked when a player connects, not while they are already in the game.

---

## Ban commands are not recognised / banned players can still join

Ban management has been moved to the **BanSystem** mod. If you are looking for ban commands:

1. Install the **BanSystem** mod.
2. Configure it via `<ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini`.
3. Use `!steamban`, `!eosban`, `!banbyname` in the Discord channel configured in `DefaultBanSystem.ini`.
4. Banned players (by Steam64 ID or EOS PUID) are automatically kicked on join by BanSystem.

See the [Ban System](04-BanSystem.md) page for full details.

---

## In-game commands are not recognised

1. Make sure `InGameWhitelistCommandPrefix` is set (defaults to `!whitelist`) and not empty in the config file.
2. In-game commands can only be typed in the **Satisfactory in-game chat** by players who are already connected to the server.
3. In-game whitelist commands do not include `!whitelist role add/remove` — that is Discord-only.
4. For in-game ban commands, install the **BanSystem** mod which provides `/steamban`, `/eosban`, `/banbyname`, and related commands.
5. If the command appears to do nothing, check the server log (`FactoryGame.log`) for `LogDiscordBridge` or `LogWhitelistManager` lines that may explain why it was rejected.

---

## Log verbosity

Add the following to your server's `DefaultEngine.ini` to increase log detail:

```ini
[Core.Log]
LogDiscordBridge=Verbose
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
