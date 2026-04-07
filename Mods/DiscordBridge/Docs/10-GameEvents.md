# DiscordBridge – Game Events

← [Back to index](README.md)

DiscordBridge can post Discord messages whenever the server's **game phase**
advances or a player purchases a **schematic (milestone)**. Both event types
are enabled as soon as a non-empty channel or the main `ChannelId` is configured
— there is no separate on/off switch.

---

## Game phase change announcements

Satisfactory tracks the overall server progression through a series of game
phases (e.g. Pioneer → Ficsit Pioneer Candidate). DiscordBridge hooks
`AFGGamePhaseManager::mOnCurrentGamePhaseUpdated` and posts a message to
Discord whenever the phase changes.

### `PhaseEventsChannelId`

The snowflake ID of the Discord channel for game-phase announcements.
Leave **empty** to route phase events to the main `ChannelId`.

**Default:** *(empty — falls back to `ChannelId`)*

```ini
PhaseEventsChannelId=111222333444555666777
```

---

## Schematic (milestone) unlock announcements

Every time a player purchases a schematic through the HUB or MAM,
DiscordBridge hooks `AFGSchematicManager::PurchasedSchematicDelegate` and
posts a message with the schematic name to Discord.

### `SchematicEventsChannelId`

The snowflake ID of the Discord channel for schematic-unlock announcements.

Fallback order when empty:

1. `SchematicEventsChannelId` (if set)
2. `PhaseEventsChannelId` (if set)
3. `ChannelId`

**Default:** *(empty — falls back through chain above)*

```ini
SchematicEventsChannelId=111222333444555666888
```

---

## Full example

```ini
; Route game events to a dedicated "progress" channel
PhaseEventsChannelId=111222333444555666777

; Route schematic unlocks to the same channel (leave empty to share PhaseEventsChannelId)
SchematicEventsChannelId=
```

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
