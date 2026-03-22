# DiscordBridge – Ban System

← [Back to index](README.md)

The built-in ban system lets you manage player bans directly from the bridged Discord channel.
Bans are stored in `<ServerRoot>/FactoryGame/Saved/ServerBanlist.json` and persist across
server restarts automatically.

Players can be banned in two ways:

| Method | Command | Notes |
|--------|---------|-------|
| **By in-game name** | `!ban add <name>` | Case-insensitive; works even if the player is offline. Also auto-bans by platform ID if they are connected. |
| **By platform ID** | `!ban id add <id>`, `!ban steam add <id>`, `!ban epic add <id>` | Steam64 ID or EOS Product User ID; bypasses name changes |

Platform ID banning is more robust because a player's Steam or Epic Games account ID never
changes, even if they change their display name or create a new character. Both methods can be
combined — a player is kicked if they match **either** their name **or** their platform ID.

> **`!ban add <name>` now auto-bans by platform ID:**
> When the named player is currently connected, `!ban add <name>` automatically also bans
> their platform ID (Steam64 or EOS PUID) in the same step — no separate
> `!ban id lookup` + `!ban id add` steps needed.  The bot replies with a confirmation
> line showing the platform ID that was added, or an informational note if the player
> is offline or their PUID is still resolving.

> **How to find a player's platform ID:**
> Use `!ban players` in the bridged Discord channel to see every connected player together with
> their platform name and platform ban ID (EOS PUID on EOS-mode servers, Steam64 ID on
> Steam-mode servers). Alternatively, use `!ban id lookup <name>` to query a specific player's
> platform ban ID by in-game name. The server log also prints the platform ID alongside the
> player name when they join or are kicked (look for lines containing `DiscordBridge BanSystem`).
> Once you have the ID, ban it with `!ban id add <id>` (EOS PUID) or `!ban steam add <id>`
> (Steam64) so the player cannot rejoin under a different name.

> **The ban system and the whitelist are completely independent.**
> You can use either one, both, or neither — enabling or disabling one never affects the other.
>
> | Goal | Config |
> |------|--------|
> | Ban system only | `WhitelistEnabled=False`, `BanSystemEnabled=True` *(default)* |
> | Whitelist only | `WhitelistEnabled=True`, `BanSystemEnabled=False` |
> | Both | `WhitelistEnabled=True`, `BanSystemEnabled=True` |
> | Neither | `WhitelistEnabled=False`, `BanSystemEnabled=False` |

---

## Steam vs Epic — which platform ID type to use?

The correct platform ID format depends on how your server's `DefaultEngine.ini` is configured
under `[OnlineSubsystem]`:

> **CSS dedicated server users — read this first:**
>
> Satisfactory's custom Unreal Engine build (UnrealEngine-CSS) uses
> `DefaultPlatformService=EOS` as the primary online platform for dedicated servers.
> `OnlineSubsystemSteam` (the v1 Steam OSS plugin) is **explicitly disabled on
> CSS dedicated server builds** — it has `TargetDenyList: Server` in CSS's
> `OnlineIntegration.uplugin`.
>
> **You do NOT need to enable `OnlineSubsystemSteam` for Steam ban enforcement on a
> CSS server.** Steam players are bridged to EOS Product User IDs (PUIDs) automatically
> via `NativePlatformService=Steam`.  Ban Steam players by their **EOS PUID**, not their
> Steam64 ID, on a CSS server.
>
> The correct `DefaultEngine.ini` for any CSS dedicated server is:
> ```ini
> [OnlineSubsystem]
> DefaultPlatformService=EOS
> NativePlatformService=Steam
> ```
>
> `DefaultPlatformService=Steam` on a CSS server is a **misconfiguration** — the Steam
> OSS will not load and player IDs will likely not resolve correctly.
> Run `!server eos` in game chat or `!ban status` in Discord to verify your platform state.

### `DefaultPlatformService=Steam` (Steam-mode server — non-CSS servers only)

Players connect with their **Steam64 ID** — a 17-digit decimal number that starts with `765`.

> **This section applies only to non-CSS server builds where `OnlineSubsystemSteam` (v1)
> is loaded.**  On CSS dedicated servers `OnlineSubsystemSteam` is disabled — use
> `DefaultPlatformService=EOS` + `NativePlatformService=Steam` instead (see note above).

```
Example Steam64 ID:  76561198123456789
```

Use the dedicated Steam commands to ban with format validation:

```
!ban steam add 76561198123456789       ← bans by Steam64 ID
!ban steam remove 76561198123456789    ← unbans
!ban steam list                         ← lists all Steam64 ID bans
```

You can also use `!ban id add 76561198123456789` — the bot auto-detects the Steam64 format
and labels it `(Steam)` in the confirmation and in `!ban id list`.

> **How to find a Steam64 ID:**
> Look at the player's Steam profile URL (`https://steamcommunity.com/profiles/<id>`), use
> the SteamID lookup tool at [steamid.io](https://steamid.io), or run `!ban players` in
> Discord while the player is connected — the server will show their Steam64 ID in the
> platform ID column.

### `DefaultPlatformService=EOS` (EOS-mode server — the correct setting for all CSS dedicated servers)

Both Steam **and** Epic Games players receive an **EOS Product User ID (PUID)** — a hex
alphanumeric string assigned by Epic Online Services.

```
Example EOS PUID:  0002abcdef1234567890abcdef123456
```

Use the dedicated Epic/EOS commands:

```
!ban epic add 0002abcdef1234567890abcdef123456       ← bans by EOS PUID
!ban epic remove 0002abcdef1234567890abcdef123456    ← unbans
!ban epic list                                        ← lists all EOS PUID bans
```

You can also use `!ban id add <puid>` — the bot auto-detects the EOS PUID format and labels
it `(EOS PUID)` in the confirmation and in `!ban id list`.

> **How to find an EOS PUID:**
> Use `!ban players` to list all connected players with their platform and PUID.
> Or use `!ban id lookup <PlayerName>` to query a specific player's platform ban ID by name
> (returns an EOS PUID on EOS-mode servers).
> You cannot determine a player's PUID from outside the server — the player must connect first.

### ID format validation

The `!ban steam add` command validates that the supplied value is actually a Steam64 ID
(17 digits starting with `765`) and rejects invalid input with a helpful error. Similarly,
`!ban epic add` warns you if you accidentally supply a Steam64 ID instead of an EOS PUID.

The generic `!ban id add` command accepts both formats and auto-labels them in output:

```
!ban id list
→  `76561198123456789` (Steam)
   `0002abcdef1234567890abcdef123456` (EOS PUID)
```

### EOS PUID followup check and cross-platform ID linking

On an EOS-configured server, Steam players may briefly present a **Steam64 ID** at the
moment they connect — before Epic Online Services finishes processing their login and
upgrades the identifier to an EOS Product User ID (PUID).  This commonly occurs on custom
UnrealEngine-CSS builds where EOS components initialise more slowly.

The mod handles this automatically:

1. At join time, the Steam64 ID is checked against the ban list.
2. A **60-second background timer** polls for the player's EOS PUID.
3. As soon as the EOS PUID resolves (the engine replaces the Steam handle), **both** the
   EOS PUID **and** the original Steam64 ID are checked against the ban list.
4. **Cross-platform ID linking:** if either ID is found on the ban list, the partner ID is
   automatically added to the ban list.  This ensures the ban applies regardless of which
   identifier the player presents on their next login:

| Situation | What happens |
|-----------|-------------|
| Steam64 is banned, EOS PUID is not | EOS PUID is automatically added; player is kicked; ban channel notified |
| EOS PUID is banned, Steam64 is not | Steam64 is automatically added; player is kicked |
| Both already banned | Player is kicked |
| Neither banned | Timer stops; player stays |

This cross-platform linking means that once the server has observed a player's
Steam64 ↔ EOS PUID mapping, a ban added via **either** identifier will be enforced on
all future logins — no manual `!ban id add` for the second ID is needed.

> **Note:** The followup timer is only active when `DefaultPlatformService=EOS` is set in
> `DefaultEngine.ini`.  On Steam-only servers the ID format never changes, so no followup
> is needed.

> **Tip:** To guarantee immediate enforcement regardless of EOS initialisation timing,
> you can still ban both IDs explicitly:
> ```
> !ban steam add 76561198123456789              ← catches the player at primary check
> !ban epic add 0002abcdef1234567890abcdef123456 ← catches them once EOS resolves their PUID
> ```
> The cross-platform linking will also add these automatically once a player's
> Steam64 ↔ EOS PUID pair has been observed on the server.

---

## Config file

All ban settings live in their own dedicated file:

```
<ServerRoot>/FactoryGame/Mods/DiscordBridge/Config/DefaultBan.ini
```

This file ships with the mod and is created automatically on the first server start
if it is missing. Edit the file and restart the server to apply changes.

> **Backup:** the mod writes `<ServerRoot>/FactoryGame/Saved/Config/Ban.ini` on every
> server start.  If a mod update resets `DefaultBan.ini`, the backup is used to
> restore your settings automatically.

---

## Settings

### `BanSystemEnabled`

Controls whether the ban system is active when the server starts.

**Default:** `True` (banned players are kicked on join)

Set to `False` and restart the server to completely disable ban enforcement. Set back to `True` and restart to re-enable it. This INI value is applied on **every** server restart, so it is the authoritative on/off toggle for the ban system.

> **Note:** `!ban on` / `!ban off` Discord commands update the in-memory state for the current session but do not override this config setting — the next server restart will always reset to whatever `BanSystemEnabled` is set to in the config file.

---

### `BanCommandRoleId`

The Discord role ID whose members are allowed to run `!ban` management commands.

**Default:** *(empty — !ban commands are disabled for all Discord users)*

When set, **only members who hold this role** can run `!ban` commands in the bridged Discord channel. When left empty, `!ban` commands are fully disabled (deny-by-default) — no one can run them until a role ID is provided.

This role is also the one granted or revoked by `!ban role add/remove <discord_id>`, so existing ban admins can promote or demote other Discord members without needing access to Discord server settings. The bot must have the **Manage Roles** permission for those commands to work.

> **IMPORTANT:** Holding this role does **not** bypass the ban check when joining the game. If a role-holder's in-game name is on the ban list they will still be kicked.

**How to get the role ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the role in Server Settings → Roles and choose **Copy Role ID**.

**Example:**
```ini
BanCommandRoleId=987654321098765432
```

The ban admin role and the whitelist admin role are **completely independent** — you can assign different roles to each, or use the same role for both.

---

### `BanCommandPrefix`

The prefix that triggers ban management commands when typed in the bridged Discord channel.
Set to an **empty string** to disable Discord-based ban management entirely.

**Default:** `!ban`

**Supported commands** (type these in the bridged Discord channel — all require `BanCommandRoleId`):

| Command | Effect |
|---------|--------|
| `!ban on` | Enable the ban system; currently-connected banned players are kicked immediately |
| `!ban off` | Disable the ban system — banned players can join freely |
| `!ban players` | List all currently connected players with their platform name and EOS PUID, flagging any who are already banned |
| `!ban check <name>` | Check if the named player is banned (by name and, if connected, by platform ID) |
| `!ban add <name>` | Ban a player by in-game name; also auto-bans their platform ID (Steam/Epic) if connected; kicks immediately if ban system is enabled |
| `!ban remove <name>` | Unban a player by in-game name |
| `!ban id add <platform_id>` | Ban by Steam64 ID **or** EOS PUID — auto-detects type and labels confirmation as `(Steam)` or `(EOS PUID)` |
| `!ban id remove <platform_id>` | Unban a player by platform ID |
| `!ban id lookup <name>` | Look up the platform ban ID (EOS PUID or Steam64 ID) of a currently connected player by in-game name |
| `!ban id list` | List all banned platform IDs with type labels `(Steam)` / `(EOS PUID)` |
| `!ban steam add <steam64_id>` | Ban by **Steam64 ID** with format validation (17-digit, starts with 765) |
| `!ban steam remove <steam64_id>` | Unban by Steam64 ID |
| `!ban steam list` | List only banned Steam64 IDs |
| `!ban epic add <eos_puid>` | Ban by **EOS PUID** (works for Steam & Epic on EOS servers); warns if you accidentally supply a Steam64 ID |
| `!ban epic remove <eos_puid>` | Unban by EOS PUID |
| `!ban epic list` | List only banned EOS PUIDs |
| `!ban list` | List all banned player names and current enabled/disabled state |
| `!ban status` | Show the current enabled/disabled state of **both** the ban system and the whitelist, plus platform state (Steam or EOS) |
| `!ban role add <discord_id>` | Grant the `BanCommandRoleId` role to a Discord user |
| `!ban role remove <discord_id>` | Revoke the `BanCommandRoleId` role from a Discord user |

---

### `BanCommandsEnabled`

Master on/off switch for the **ban command interface** — controls whether `!ban` Discord and in-game commands are accepted at all.

**Default:** `True`

| Value | Effect |
|-------|--------|
| `True` | `!ban` commands work normally (still gated by `BanCommandRoleId`) |
| `False` | All `!ban` commands are silently ignored; existing bans are **still enforced** on join; `BanSystemEnabled` is unaffected |

This gives a clean "on/off from config" for the command interface, independently of whether ban enforcement itself is active. Unlike `BanCommandPrefix=` (which disables commands by removing the trigger word), `BanCommandsEnabled=False` provides an explicit config toggle that's easy to find and understand.

**Example — disable ban commands, keep enforcement:**
```ini
BanSystemEnabled=True
BanCommandsEnabled=False
```

---

### `BanChannelId`

The Discord channel ID of the dedicated channel for ban management.
Leave **empty** to disable the ban-only channel.

**Default:** *(empty)*

When set:
- `!ban` commands issued from this channel are accepted. The sender must still hold `BanCommandRoleId`. Responses are sent back to this channel (not the main channel).
- Ban-kick notifications are **also** posted here in addition to the main `ChannelId`, giving admins a focused audit log of all ban events.
- Non-ban-command messages in this channel are silently ignored (the channel is admin-only and not bridged to game chat).

**How to get the channel ID:**
Enable Developer Mode in Discord (User Settings → Advanced → Developer Mode), then right-click the channel and choose **Copy Channel ID**.

**Example:**
```ini
BanChannelId=567890123456789012
```

> **Tip:** You can use `BanChannelId` together with `BanCommandRoleId` for a fully locked-down ban management workflow: create a private admin channel, add the bot to it, set `BanChannelId` to its ID and `BanCommandRoleId` to your admin role — only admins can see the channel and only they can issue ban commands from it.

---

### `BanKickDiscordMessage`

The message posted to the **main** Discord channel whenever a banned player attempts to join and is kicked.
Leave **empty** to disable this notification.

**Default:** `:hammer: **%PlayerName%** is banned from this server and was kicked.`

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlayerName%` | The in-game name of the banned player who was kicked |

**Example:**

```ini
BanKickDiscordMessage=:no_entry: **%PlayerName%** is banned and was removed.
```

> **This fires after the player has joined and is then kicked.**  For a notification that
> fires at the very start of the connection handshake — before the player has a display
> name — see [`BanLoginRejectDiscordMessage`](#banloginrejectdiscordmessage) below.

---

### `BanLoginRejectDiscordMessage`

The message posted to the Discord channel(s) whenever a banned player is **rejected at
login time** — at the very start of the connection handshake, before the player has
fully joined the server.  Leave **empty** to disable this notification.

Unlike `BanKickDiscordMessage`, this fires before the player's in-game display name is
known, so the placeholders use the platform ID instead of the player name:

| Placeholder | Replaced with |
|-------------|---------------|
| `%PlatformId%` | The banned Steam64 ID or EOS Product User ID |
| `%PlatformType%` | `Steam` or `EOS PUID`, depending on the ID format |

**Default:** `:no_entry: A banned player (%PlatformType% \`%PlatformId%\`) tried to connect and was rejected.`

**Example:**

```ini
BanLoginRejectDiscordMessage=:no_entry: Banned player (%PlatformType% `%PlatformId%`) was blocked at connection.
```

**Comparison with `BanKickDiscordMessage`:**

| Event | Config key | When it fires | Player name available? |
|-------|-----------|---------------|------------------------|
| Login-time block | `BanLoginRejectDiscordMessage` | Very first connection handshake | ✗ |
| Post-join kick | `BanKickDiscordMessage` | After player has joined | ✓ |

> **Rate limiting:** this notification is limited to **one message per platform ID per
> 60 seconds** to prevent Discord spam when a banned player retries rapidly.

---

### `BanKickReason`

The reason shown **in-game** to the player when they are kicked for being banned.
This is the text the player sees in the disconnected / kicked screen.

**Default:** `You are banned from this server.`

**Example:**

```ini
BanKickReason=You have been banned. Contact the server admin to appeal.
```

---

### `InGameBanCommandPrefix`

The prefix that triggers ban management commands when typed in the **Satisfactory in-game chat**.
This lets server admins manage bans directly from inside the game without needing Discord.
Set to an **empty string** to disable in-game ban commands.

**Default:** `!ban`

**Supported commands** (type these in the Satisfactory in-game chat):

| Command | Effect |
|---------|--------|
| `!ban on` | Enable the ban system; currently-connected banned players are kicked immediately |
| `!ban off` | Disable the ban system |
| `!ban players` | List all currently connected players with their platform name and EOS PUID |
| `!ban check <name>` | Check if the named player is banned (by name and, if connected, by platform ID) |
| `!ban add <name>` | Ban a player by in-game name; also auto-bans their platform ID (Steam/Epic) if connected; kicks immediately if ban system is enabled |
| `!ban remove <name>` | Unban a player by in-game name |
| `!ban id add <platform_id>` | Ban by Steam64 ID or EOS PUID — auto-detects type and labels output as `(Steam)` or `(EOS PUID)` |
| `!ban id remove <platform_id>` | Unban a player by platform ID |
| `!ban id lookup <name>` | Look up the platform ban ID (EOS PUID or Steam64 ID) of a currently connected player by in-game name |
| `!ban id list` | List all banned platform IDs with type labels |
| `!ban steam add <steam64_id>` | Ban by Steam64 ID with format validation |
| `!ban steam remove <steam64_id>` | Unban by Steam64 ID |
| `!ban steam list` | List only banned Steam64 IDs |
| `!ban epic add <eos_puid>` | Ban by EOS PUID (works for Steam & Epic on EOS servers) |
| `!ban epic remove <eos_puid>` | Unban by EOS PUID |
| `!ban epic list` | List only banned EOS PUIDs |
| `!ban list` | List all banned player names and current enabled/disabled state |
| `!ban status` | Show the current enabled/disabled state of **both** the ban system and the whitelist, plus platform state (Steam or EOS) |

> **Note:** In-game ban commands support the same operations as the Discord commands,
> except for role management which is Discord-only.

---

## Player lookup commands

### `!ban players`

Lists every currently connected player with their **platform name** (Steam or Epic) and their
**EOS Product User ID (PUID)** in a single Discord message.  Any player whose name or PUID is
already on the ban list is flagged with a `:hammer: BANNED` notice.

This is the quickest way to identify who is online and grab a PUID before running
`!ban id add`.  Example output:

```
:busts_in_silhouette: 2 players connected:
• PlayerOne | Steam | `0002abc123def456` :hammer: BANNED (ID)
• PlayerTwo | Epic | `0001xyz789uvw012`
```

---

### `!ban check <PlayerName>`

Checks whether a specific player is banned **by name**, and—if they are currently
connected—also checks their PUID.  Responds with one of:

| Result | Response |
|--------|---------|
| Banned by name and ID | `:hammer: BANNED by both name and platform ID.` |
| Banned by name only | `:hammer: BANNED by name.` |
| Banned by ID only | `:hammer: BANNED by platform ID (…).` |
| Not banned | `:white_check_mark: Not banned.` |

If the player is online, their platform name and PUID are shown alongside the ban status.

---

### `!ban id lookup <PlayerName>`

Looks up the **platform ban ID** of a **currently connected** player by in-game name and
returns a ready-to-copy ban command string.  The ID type and suggested command depend on
your server's platform configuration:

- **EOS-mode servers** (`DefaultPlatformService=EOS`, the correct setting for CSS dedicated
  servers): returns an **EOS PUID** and suggests `!ban id add <puid>`.  This covers both
  Steam and Epic Games players — all players on an EOS-mode server receive an EOS PUID via
  the Steam→EOS bridge.
- **Steam-mode servers** (`DefaultPlatformService=Steam`, non-CSS servers only): returns a
  **Steam64 ID** and suggests `!ban steam add <id>`.

If the player's platform ban ID has not yet resolved (the EOS session is still initialising),
the command asks you to wait a few seconds and try again.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
