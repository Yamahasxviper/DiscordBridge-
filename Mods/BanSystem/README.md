# BanSystem — Standalone Satisfactory Server Ban Mod

A fully standalone Alpakit C++ mod that provides **two completely independent** ban systems — one for **Steam 64-bit IDs** and one for **EOS Product User IDs** — without any dependency on the custom CSS UnrealEngine fork.

---

## Features

| Feature | Steam | EOS |
|---|---|---|
| Permanent bans | ✅ | ✅ |
| Timed bans (minutes) | ✅ | ✅ |
| Auto-expiry pruning | ✅ | ✅ |
| Persistent JSON storage | ✅ | ✅ |
| Admin chat commands | ✅ | ✅ |
| Blueprint-accessible API | ✅ | ✅ |
| C++ linkable API | ✅ | ✅ |
| Ban/Unban delegates | ✅ | ✅ |

The two systems **share no state, no logic, and no storage** — they are completely separate.

---

## Admin Chat Commands

### Steam Commands
| Command | Description |
|---|---|
| `/steamban <Steam64Id\|PlayerName> [minutes] [reason...]` | Ban by Steam 64-bit ID **or player name**. Omit or use `0` minutes for permanent. |
| `/steamunban <Steam64Id>` | Remove a Steam ban. |
| `/steambanlist` | List all active Steam bans. |

### EOS Commands
| Command | Description |
|---|---|
| `/eosban <EOSProductUserId\|PlayerName> [minutes] [reason...]` | Ban by EOS Product User ID (32-char hex) **or player name**. Omit or use `0` minutes for permanent. |
| `/eosunban <EOSProductUserId>` | Remove an EOS ban. |
| `/eosbanlist` | List all active EOS bans. |

### Name-Based Commands (recommended for live moderation)
| Command | Description |
|---|---|
| `/banbyname <PlayerName> [minutes] [reason...]` | Ban a connected player by name across **all** available platforms (Steam + EOS simultaneously). |
| `/playerids [PlayerName]` | Show the platform IDs of all connected players, or a specific player, so you can record them for offline unban. |

Commands work from the **in-game chat** (admin players) and the **server console** (`bOnlyUsableByPlayer = false`).

### Player Name Lookup

When a player **name** is supplied to `/steamban`, `/eosban`, or `/banbyname`, the system:

1. Iterates all currently-connected `PlayerController`s on the server.
2. Finds the player whose `GetPlayerName()` **contains** the query (case-insensitive).
3. Resolves their Steam64 ID and/or EOS PUID via `FBanIdResolver`.
4. Applies the ban using the resolved ID.

If more than one online player's name matches the query, the command lists all ambiguous matches and asks you to be more specific.

### Examples
```
# Ban by raw ID (as before)
/steamban 76561198000000000 0 Cheating
/steamban 76561198000000000 1440 Toxic behaviour (24h)
/steamunban 76561198000000000
/steambanlist

/eosban 00020aed06f0a6958c3c067fb4b73d51 Evading previous ban
/eosunban 00020aed06f0a6958c3c067fb4b73d51
/eosbanlist

# Ban by player name — /steamban and /eosban accept names as fallback
/steamban SomePlayer 60 Spamming
/eosban SomePlayer 0 Cheating

# Preferred: /banbyname bans on ALL platforms at once
/banbyname SomePlayer Cheating
/banbyname SomePlayer 1440 Temporary mute

# Show all connected player IDs (useful before the player disconnects)
/playerids
/playerids SomePlayer
```

---

## Storage & Server-Restart Persistence

**Yes — bans survive server restarts.**

Bans are persisted as JSON in the server's save directory immediately after
every ban or unban action.  On the next startup the files are loaded back
automatically, so all active bans are enforced before the first player can
connect.

```
<ProjectSaved>/BanSystem/SteamBans.json
<ProjectSaved>/BanSystem/EOSBans.json
```

On a typical dedicated-server installation `<ProjectSaved>` resolves to:

| OS | Path |
|---|---|
| Windows | `C:\SatisfactoryServer\FactoryGame\Saved\` |
| Linux | `~/.config/Epic/FactoryGame/Saved/` (or the server's working directory) |

These directories are **not** cleared on server restart, so the JSON files
persist across updates and reboots.

### Startup behaviour

When the subsystems initialise on server start they:

1. Load all records from the JSON file.
2. **Immediately prune any timed bans whose expiry has already passed** — so
   players whose ban expired while the server was offline are never incorrectly
   kicked.
3. Save the pruned list back to disk.
4. Log the number of **active** bans.

### Example `SteamBans.json`
```json
{
  "Bans": [
    {
      "Id": "76561198000000000",
      "Reason": "Cheating",
      "BannedAt": "2025-01-01T00:00:00.000Z",
      "ExpiresAt": "0001-01-01T00:00:00.000Z",
      "BannedBy": "AdminPlayer",
      "IsPermanent": true
    }
  ]
}
```

---

## How Satisfactory Handles ID Lookups for Steam and Epic Games

Satisfactory uses the CSS custom UE5 engine with the `OnlineServices v2` layer.
Each player's identity is represented by a `FUniqueNetIdRepl` passed to the
server during login. The **BanIdResolver** decodes this into usable IDs.

### Steam players

When a player connects through Steam:

1. The CSS Steam online service authenticates the player.
2. `FUniqueNetIdRepl::GetType()` returns the `FName` **"Steam"**.
3. `FUniqueNetIdRepl::ToString()` returns the raw **17-digit Steam 64-bit
   decimal ID** (e.g. `76561198000000000`).
4. This is the ID stored in `SteamBans.json` and checked against
   `USteamBanSubsystem`.

### EOS / Epic Games players

When a player connects through Epic Online Services (EOS):

1. The CSS EOS online service assigns each player an **EOS Product User ID
   (PUID)** — a 32-character lowercase hex string (e.g.
   `00020aed06f0a6958c3c067fb4b73d51`).
2. The PUID is **not** directly available from `ToString()` on the
   `FUniqueNetId`. Instead it is extracted using the CSS FactoryGame helper
   `EOSId::GetProductUserId(const FUniqueNetId&, FString&)`, which understands
   the internal `UE::Online::FAccountId` representation used by the v2 API.
3. This PUID is the ID stored in `EOSBans.json` and checked against
   `UEOSBanSubsystem`.

### Dual-platform players (Steam + linked Epic)

CSS Satisfactory supports crossplay by linking a Steam account to an Epic
account. Such players have **both** a Steam64 ID **and** an EOS PUID active
simultaneously. The `BanIdResolver` extracts both:

```
Player connects via Steam with linked Epic account
 │
 ├─ GetType() == "Steam"  →  Steam64Id = "76561198000000000"
 └─ EOSId::GetProductUserId() succeeds  →  PUID = "00020aed06f0a6958c3c067fb4b73d51"
```

Both IDs are checked independently. A player can be banned by their Steam ID,
their EOS PUID, or both.

### LAN / Offline players

Players on the "Null" online service (LAN, offline sessions) have neither a
valid Steam type nor a resolvable EOS PUID. The resolver returns an invalid
`FResolvedBanId` and ban enforcement is skipped — which is the correct
behaviour for offline play.

---

## C++ Integration API (for other mods)

Add `"BanSystem"` to your mod's `PublicDependencyModuleNames` in `Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "BanSystem" });
```

### ID Resolution

```cpp
#include "BanIdResolver.h"

// Resolve all platform IDs for a connecting player
FResolvedBanId Ids = FBanIdResolver::Resolve(PlayerState->GetUniqueNetId());

if (Ids.HasSteamId())
    UE_LOG(MyLog, Log, TEXT("Steam64: %s"), *Ids.Steam64Id);

if (Ids.HasEOSPuid())
    UE_LOG(MyLog, Log, TEXT("PUID: %s"), *Ids.EOSProductUserId);

// Or extract one platform at a time:
FString SteamId;
if (FBanIdResolver::TryGetSteam64Id(UniqueNetId, SteamId))
    // ...

FString PUID;
if (FBanIdResolver::TryGetEOSProductUserId(UniqueNetId, PUID))
    // ...
```

### Player Name Lookup

```cpp
#include "BanPlayerLookup.h"

// Resolve IDs for a connected player by name
FResolvedBanId   Ids;
FString          FoundName;
TArray<FString>  Ambiguous;

if (FBanPlayerLookup::FindPlayerByName(World, TEXT("SomePlayer"), Ids, FoundName, Ambiguous))
{
    // Ids.Steam64Id / Ids.EOSProductUserId are now populated
    UE_LOG(MyLog, Log, TEXT("Found '%s' — Steam=%s  EOS=%s"),
        *FoundName, *Ids.Steam64Id, *Ids.EOSProductUserId);
}
else if (Ambiguous.Num() > 1)
{
    // Multiple players matched — show the list to the admin
    UE_LOG(MyLog, Warning, TEXT("Ambiguous: %s"),
        *FString::Join(Ambiguous, TEXT(", ")));
}

// Get all connected players and their IDs:
for (const auto& Pair : FBanPlayerLookup::GetAllConnectedPlayers(World))
{
    UE_LOG(MyLog, Log, TEXT("%s → Steam=%s  EOS=%s"),
        *Pair.Key, *Pair.Value.Steam64Id, *Pair.Value.EOSProductUserId);
}
```

### Ban Management

```cpp
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"

USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
UEOSBanSubsystem*   EOSBans   = GI->GetSubsystem<UEOSBanSubsystem>();

// Ban permanently
SteamBans->BanPlayer(TEXT("76561198000000000"), TEXT("Cheating"), 0, TEXT("MyMod"));

// Ban for 60 minutes
EOSBans->BanPlayer(TEXT("00020aed06f0a6958c3c067fb4b73d51"), TEXT("Spam"), 60, TEXT("MyMod"));

// Check
FString Reason;
if (SteamBans->IsPlayerBanned(TEXT("76561198000000000"), Reason)) { ... }

// React to ban events
SteamBans->OnPlayerBanned.AddDynamic(this, &UMyClass::HandleSteamBan);
EOSBans->OnPlayerUnbanned.AddDynamic(this, &UMyClass::HandleEOSUnban);
```

### Blueprint Integration

Both subsystems and `FResolvedBanId` are fully Blueprint-accessible.
Use **Get Game Instance → Get Subsystem** nodes.

---

## ID Format Reference

| Platform | Format | Example |
|---|---|---|
| Steam | 17-digit decimal starting with `7656119` | `76561198000000000` |
| EOS PUID | 32 lowercase hex characters | `00020aed06f0a6958c3c067fb4b73d51` |

Both subsystems include static validation helpers:
- `USteamBanSubsystem::IsValidSteam64Id(FString)`
- `UEOSBanSubsystem::IsValidEOSProductUserId(FString)`

---

## Build Targets

This mod is built for all Satisfactory server targets as required by Alpakit:

```
Windows
WindowsServer
LinuxServer
```

---

## Dependencies

| Dependency | Version | Purpose |
|---|---|---|
| SML | `^3.11.3` | Native hooks, chat commands |
| FactoryGame | bundled | Game mode hook targets |

No dependency on any CSS-custom engine modules or online integration APIs.
