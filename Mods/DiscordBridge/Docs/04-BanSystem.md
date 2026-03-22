# DiscordBridge – Ban System

← [Back to index](README.md)

> **The ban system has been moved to the dedicated BanSystem mod.**
>
> DiscordBridge no longer manages bans. All ban functionality — Discord commands,
> in-game commands, join enforcement, and kick notifications — is now handled
> entirely by the **BanSystem** mod.

---

## Using the BanSystem mod

Install the **BanSystem** mod alongside DiscordBridge (or standalone) and configure it via:

```
<ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
```

### What BanSystem provides

| Feature | Description |
|---------|-------------|
| **Join enforcement** | Banned players (by Steam64 ID or EOS PUID) are automatically kicked when they connect |
| **Discord commands** | `!steamban`, `!steamunban`, `!steambanlist`, `!eosban`, `!eosunban`, `!eosbanlist`, `!banbyname`, `!playerids` |
| **In-game commands** | `/steamban`, `/steamunban`, `/eosban`, `/eosunban`, `/banbyname`, `/playerids` |
| **Kick notification** | Posts to Discord (`BanKickDiscordMessage`) when a banned player is kicked on join |
| **Configurable kick reason** | `SteamBanKickReason` / `EOSBanKickReason` control what banned players see |
| **Standalone mode** | Set `BotToken` in `DefaultBanSystem.ini` to run without DiscordBridge |
| **Shared mode** | Leave `BotToken` empty — BanSystem uses DiscordBridge's connection automatically |

### Why it was moved

BanSystem uses **platform IDs** (Steam 64-bit IDs and EOS Product User IDs) rather than
player names, which is far more reliable — players cannot evade bans by changing their
display name.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
