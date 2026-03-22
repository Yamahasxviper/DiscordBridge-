# DiscordBridge – Connection Settings

← [Back to index](README.md)

These settings identify **which bot** to run and **which channel** to bridge.
They are required — the bridge will not start without `BotToken` and `ChannelId`.

---

## Settings

| Setting | Type | Default | Description |
|---------|------|---------|-------------|
| `BotToken` | string | *(empty)* | Your Discord bot token. Treat this like a password – never share it. The bridge will not start if this is empty. |
| `ChannelId` | string | *(empty)* | The snowflake ID of the Discord text channel to bridge. Must be a numeric ID (e.g. `123456789012345678`). |
| `ServerName` | string | *(empty)* | A display name for this server. Used as the `%ServerName%` placeholder in message formats and the player-count presence. Example: `My Satisfactory Server`. |

---

## How to find your Channel ID

Enable **Developer Mode** in Discord (User Settings → Advanced → Developer Mode),
then right-click the target text channel and choose **Copy Channel ID**.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
