# DiscordBridge – Building with Alpakit & CSS UnrealEngine

← [Back to index](README.md)

This guide is for **mod developers** who want to build, modify, or extend
DiscordBridge from source.  If you just want to install and run the mod on a
dedicated server you do not need this page — see the [Getting Started guide](01-GettingStarted.md).

---

## Overview

DiscordBridge (and its companion plugin SMLWebSocket) is
built as an **Alpakit C++ mod** targeting Coffee Stain Studios' custom
Unreal Engine build (**UnrealEngine-CSS**, based on UE 5.3).

Alpakit is the official Satisfactory modding build pipeline.  It compiles mods
against the CSS engine headers, packages the output into a `.pak` file (and
optionally a `.zip` for SMM distribution), and can copy the result directly
to your local Satisfactory installation for testing.

```
Source files (.cpp / .h)
        │
        ▼  Alpakit / UnrealBuildTool (UBT)
Compiled mod (.pak / .zip)
        │
        ▼  Satisfactory Mod Manager (SMM) or manual copy
Server Mods/ directory
```

---

## Alpakit project setup

1. Follow the official modding documentation to set up the UnrealEngine-CSS
   project: <https://docs.ficsit.app/satisfactory-modding/latest/Development/BeginnersGuide/project_setup.html>

2. Clone this repository into your Unreal project's `Mods/` directory.

3. To get Alpakit's C# files to show up in the Visual Studio project, pass the
   `-dotnet` argument when generating project files (command-line approach only):

   ```powershell
   UnrealBuildTool.exe -projectfiles -project="FactoryGame.uproject" -dotnet
   ```

4. Open the generated `.sln` in Visual Studio or Rider, set the build target to
   **Development Editor**, and build.

---

## CSS UnrealEngine-CSS — what is different from stock UE?

Coffee Stain Studios ships a **custom fork** of Unreal Engine 5.3 together with
Satisfactory.  Several engine modules that exist in the public UE source tree
are either absent, stripped, or replaced with CSS-specific equivalents.

The table below summarises the most important differences that affect mod
compilation:

| Standard UE module | Status in CSS UE | CSS alternative |
|--------------------|-----------------|-----------------|
| `WebSocketsModule` (built-in WSS) | **Not available** in Alpakit packages | `SMLWebSocket` (custom implementation included in this repo) |
| `OnlineSubsystem` (v1 OSS) | Header availability **not guaranteed** for mods; commented out in the Alpakit blank template | `GConfig->GetString()` reads on `GEngineIni` (Core, always present) |
| `OnlineSubsystemEOS` | **Not available** to mods | `OnlineIntegration` (CSS-native; see below) |
| `OnlineSubsystemUtils` | **Not available** to mods | `OnlineIntegration` |

> **Rule of thumb:** if the Alpakit blank template (`PLUGIN_NAME.Build.cs`)
> has a module commented out, treat it as unavailable and find the CSS-native
> equivalent before adding it to your own `Build.cs`.

---

## Module reference for Alpakit C++ mods

### `DummyHeaders` — required by every Alpakit C++ mod

CSS UE ships without certain engine header files that standard UE mods assume
are present.  The `DummyHeaders` module provides stub implementations of those
headers so that UBT (UnrealBuildTool) can resolve include paths at compile time
without errors.

**Every** Alpakit C++ mod must declare this module as a dependency:

```csharp
PublicDependencyModuleNames.Add("DummyHeaders");
```

Without it you will see include-not-found errors for standard engine headers
during mod compilation.

---

### `OnlineIntegration` — CSS-native online services layer

CSS UE replaces the v1 OnlineSubsystem stack with its own integration layer
located at `Plugins/Online/OnlineIntegration`.  This layer exposes:

| CSS type | Purpose |
|----------|---------|
| `UOnlineIntegrationSubsystem` | Session manager, user manager, and platform state |
| `UCommonUserSubsystem` | Local user count (`GetNumLocalPlayers()`) for runtime EOS guards |
| `UCommonSessionSubsystem` | Session lifecycle helpers |

**To depend on it**, add to `Build.cs`:

```csharp
PublicDependencyModuleNames.Add("OnlineIntegration");
```

Do **not** use `OnlineSubsystem.h` or `IOnlineSubsystem::Get()`.  These v1 OSS
calls may crash or produce linker errors because the v1 OSS headers are
intentionally absent (or unreliable) in the Alpakit build environment.

**Reading EOS config without OnlineSubsystem:**

```cpp
#include "Misc/ConfigCacheIni.h"

FString DefaultPlatformService;
GConfig->GetString(
    TEXT("OnlineSubsystem"),
    TEXT("DefaultPlatformService"),
    DefaultPlatformService,
    GEngineIni);
```

`GConfig` and `GEngineIni` are part of UE Core and are always available.

---

### `SMLWebSocket` — custom WebSocket + SSL client

Unreal Engine's built-in `WebSocketsModule` is **not available** in
Alpakit-packaged mods.  `SMLWebSocket` is a custom implementation (included
in this repository as a standalone mod/plugin) that provides:

- RFC 6455 WebSocket protocol over TCP
- TLS encryption via OpenSSL (Win64 and Linux only, matching the two supported
  dedicated-server platforms)

DiscordBridge depends on `SMLWebSocket` for its connection to Discord's gateway.
Both mods must be installed together — SMM installs `SMLWebSocket` automatically
as a dependency.

**To depend on it**, add to `Build.cs`:

```csharp
PublicDependencyModuleNames.Add("SMLWebSocket");
```

**Build.cs note:** `SMLWebSocket` itself links OpenSSL as a **private**
dependency so that OpenSSL symbols are not leaked into the public link
interface:

```csharp
// In SMLWebSocket.Build.cs — excerpt
PrivateDependencyModuleNames.AddRange(new string[] { "SSL", "OpenSSL" });
```

---

### `GameplayEvents` — structured in-game event bus

CSS UE ships with a **GameplayEvents** plugin (`Plugins/GameplayEvents`) that
provides a lightweight tag-based event bus for mods to broadcast and subscribe
to named game events.

DiscordBridge uses it to expose bridge lifecycle events (`DiscordBridge.Connected`,
`.Disconnected`, `.Player.Joined`, `.Player.Left`, `.Message.FromDiscord`) and to
subscribe to `DiscordBridge.Message.ToDiscord` so other mods can post Discord
messages without directly calling DiscordBridge subsystem methods.

**To depend on it**, add to `Build.cs`:

```csharp
PublicDependencyModuleNames.Add("GameplayEvents");
```

---

### `ReliableMessaging` — client-to-server relay channel

CSS UE ships with a **ReliableMessaging** plugin that provides ordered,
acknowledged message delivery between client and server.  DiscordBridge
registers the `EDiscordRelayChannel::ForwardToDiscord` (value `200`) channel
so that client-side mods can forward messages to Discord without needing a
direct reference to DiscordBridge.

**To depend on it**, add to `Build.cs`:

```csharp
PublicDependencyModuleNames.Add("ReliableMessaging");
```

---

## Recommended `Build.cs` settings for CSS UE mods

```csharp
public class MyMod : ModuleRules
{
    public MyMod(ReadOnlyTargetRules Target) : base(Target)
    {
        // C++ 20 — required for CSS UE 5.3 mod compatibility
        CppStandard = CppStandardVersion.Cpp20;

        // Use the latest build settings supported by this UBT version
        DefaultBuildSettings = BuildSettingsVersion.Latest;

        // Explicit or shared PCH — avoids "include what you use" linker noise
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        // Strict include paths — catches missing includes early
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "DummyHeaders",    // Required: CSS UE header stubs
            "FactoryGame",     // Game code (AFGPlayerState, AFGChatManager, …)
            "SML",             // SML runtime
        });
    }
}
```

---

## Packaging and releasing

Alpakit produces the distributable artifact.  In the Unreal Editor:

1. Open **Edit → Plugins → Alpakit** (or the Alpakit panel).
2. Select the mod plugin(s) to package.
3. Click **Package** (or **Package & Upload** for ficsit.app).

The output is a `.zip` file containing the `.pak` and metadata, ready for
upload to [ficsit.app](https://ficsit.app) or manual installation.

For the full CI/CD release workflow used by this repository see
[ReleaseProcess.md](../../../../ReleaseProcess.md).

---

## Common build errors

| Error | Likely cause | Fix |
|-------|-------------|-----|
| `fatal error: 'OnlineSubsystem.h' file not found` | `"OnlineSubsystem"` added to `Build.cs` | Remove it; use `GConfig->GetString()` on `GEngineIni` instead |
| `error: use of undeclared identifier 'IOnlineSubsystem'` | Same as above | Same fix |
| `fatal error: <some engine header> not found` | Missing `DummyHeaders` dependency | Add `"DummyHeaders"` to `PublicDependencyModuleNames` |
| `SIGSEGV at 0x0000000006000001` at runtime | EOS SDK call before `mIsOnline=true` or `GetNumLocalPlayers()>0` | Use `OnlineIntegration` helpers (`UCommonUserSubsystem::GetNumLocalPlayers()`, `UOnlineIntegrationControllerComponent`) with the required guards |
| Discord bridge fails to connect, WebSocket errors in log | `SMLWebSocket` not installed | Install `SMLWebSocket` alongside the mod |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
