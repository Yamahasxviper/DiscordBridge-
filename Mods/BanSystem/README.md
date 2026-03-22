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
| `/steamban <Steam64Id> [minutes] [reason...]` | Ban by Steam 64-bit ID. Omit or use `0` minutes for permanent. |
| `/steamunban <Steam64Id>` | Remove a Steam ban. |
| `/steambanlist` | List all active Steam bans. |

### EOS Commands
| Command | Description |
|---|---|
| `/eosban <EOSProductUserId> [minutes] [reason...]` | Ban by EOS Product User ID (32-char hex). Omit or use `0` minutes for permanent. |
| `/eosunban <EOSProductUserId>` | Remove an EOS ban. |
| `/eosbanlist` | List all active EOS bans. |

Commands work from the **in-game chat** (admin players) and the **server console** (`bOnlyUsableByPlayer = false`).

### Examples
```
/steamban 76561198000000000 0 Cheating
/steamban 76561198000000000 1440 Toxic behaviour (24h)
/steamunban 76561198000000000
/steambanlist

/eosban 00020aed06f0a6958c3c067fb4b73d51 Evading previous ban
/eosunban 00020aed06f0a6958c3c067fb4b73d51
/eosbanlist
```

---

## Storage

Bans are persisted as JSON in the server's save directory:

```
<ProjectSaved>/BanSystem/SteamBans.json
<ProjectSaved>/BanSystem/EOSBans.json
```

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

## How Ban Enforcement Works

When a player successfully connects, the `AFGGameMode::PostLogin` hook fires.  
The hook checks the player's `UniqueNetId` string against each ban list **independently**:

1. **Steam subsystem** checks if the ID matches a Steam64 entry in `SteamBans.json`.
2. **EOS subsystem** checks if the ID matches an EOS PUID entry in `EOSBans.json`.

If a match is found the player is kicked immediately with the ban reason prefixed by `[Steam Ban]` or `[EOS Ban]`.

> **Note:** The effectiveness of the PostLogin hook depends on what the active online service populates in `APlayerState::GetUniqueNetId()`. On a dedicated server using the CSS custom online integration the raw player identity string should still be available through this standard UE API.

---

## C++ Integration API (for other mods)

Add `"BanSystem"` to your mod's `PublicDependencyModuleNames` in `Build.cs`:

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "BanSystem" });
```

Then use either subsystem anywhere you have a `UGameInstance*`:

```cpp
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"

// Get subsystems
USteamBanSubsystem* SteamBans = GetGameInstance()->GetSubsystem<USteamBanSubsystem>();
UEOSBanSubsystem*   EOSBans   = GetGameInstance()->GetSubsystem<UEOSBanSubsystem>();

// Ban a player (permanent)
SteamBans->BanPlayer(TEXT("76561198000000000"), TEXT("Cheating"), 0, TEXT("MyMod"));

// Ban for 60 minutes
EOSBans->BanPlayer(TEXT("00020aed06f0a6958c3c067fb4b73d51"), TEXT("Spam"), 60, TEXT("MyMod"));

// Check ban status
FString Reason;
if (SteamBans->IsPlayerBanned(TEXT("76561198000000000"), Reason))
{
    // player is banned, Reason is populated
}

// Listen for ban events
SteamBans->OnPlayerBanned.AddDynamic(this, &UMyClass::OnSteamPlayerBanned);
EOSBans->OnPlayerUnbanned.AddDynamic(this, &UMyClass::OnEOSPlayerUnbanned);
```

### Blueprint Integration

Both subsystems are fully Blueprint-accessible.  
Use the **Get Game Instance → Get Subsystem (Steam Ban Subsystem)** node chain.

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
