# DiscordBridge – Player Notifications

← [Back to index](README.md)

These settings control whether DiscordBridge posts a message to Discord when a
player **joins**, **leaves**, or **times out** from the server.

> All three event types are **disabled by default**.
> Set `PlayerEventsEnabled=True` to opt in.

---

## `PlayerEventsEnabled`

Master on/off switch for all player join/leave/timeout notifications.

**Default:** `False`

```ini
; Enable player notifications
PlayerEventsEnabled=True
```

---

## `PlayerEventsChannelId`

The snowflake ID of a dedicated Discord channel for player join/leave/timeout
notifications.  Leave **empty** to post to the main bridged channel (`ChannelId`).

**Default:** *(empty — notifications go to `ChannelId`)*

```ini
; Route player events to a dedicated channel
PlayerEventsChannelId=111222333444555666777
```

---

## `PlayerJoinMessage`

Posted to Discord when a player joins the server.
Leave **empty** to disable join notifications.

**Default:** `:green_circle: **%PlayerName%** joined the server.`

### Placeholders

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | In-game display name of the joining player |
| `%SteamId%` | Steam64 ID (17-digit decimal, e.g. `76561198000000000`). Empty string when the player did not connect through Steam (e.g. Epic-direct / EOS-only). |
| `%EOSProductUserId%` | EOS Product User ID (32-char lowercase hex, e.g. `00020aed06f0a6958c3c067fb4b73d51`). Empty string when no EOS session is present. |

Both `%SteamId%` and `%EOSProductUserId%` may be populated simultaneously for a
Steam player whose account is linked to an Epic account.

**Examples:**

```ini
; Default
PlayerJoinMessage=:green_circle: **%PlayerName%** joined the server.

; Include EOS PUID for admin reference
PlayerJoinMessage=:green_circle: **%PlayerName%** joined. (EOS: %EOSProductUserId%)

; Disable join notifications
PlayerJoinMessage=
```

---

## `PlayerLeaveMessage`

Posted to Discord when a player **leaves** the server cleanly.
Also used as a fallback when `PlayerTimeoutMessage` is empty and a timeout is
detected.  Leave **empty** to disable leave notifications entirely.

**Default:** `:red_circle: **%PlayerName%** left the server.`

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | In-game display name of the player who left |

---

## `PlayerTimeoutMessage`

Posted to Discord when a player's connection is **lost without a clean disconnect**
(e.g. the process was killed or the network dropped).
Leave **empty** to use `PlayerLeaveMessage` for timeouts instead (or to suppress
timeout-specific notifications when `PlayerLeaveMessage` is also empty).

**Default:** `:yellow_circle: **%PlayerName%** timed out.`

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | In-game display name of the player who timed out |

---

## Full example

```ini
; Enable player notifications
PlayerEventsEnabled=True

; Route notifications to a dedicated channel (optional)
PlayerEventsChannelId=111222333444555666777

; Customise messages
PlayerJoinMessage=:green_circle: **%PlayerName%** joined the server.
PlayerLeaveMessage=:red_circle: **%PlayerName%** left the server.
PlayerTimeoutMessage=:yellow_circle: **%PlayerName%** timed out.
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
