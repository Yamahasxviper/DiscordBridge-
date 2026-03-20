# Build Reference — DiscordBridge & Companion Mods

This document describes the toolchain, engine, and mod structure used to build
every mod in this repository.  It is intended as a quick reference so that
developers and AI assistants working on this codebase immediately understand
the build environment without having to inspect individual files.

---

## Toolchain at a glance

| Tool | Role |
|------|------|
| **Alpakit** | Official Satisfactory mod build & packaging tool (included as `Mods/Alpakit/`) |
| **UnrealBuildTool (UBT)** | C++ compiler driver invoked by Alpakit to compile mod DLLs |
| **UnrealEngine-CSS** | Coffee Stain Studios' custom fork of Unreal Engine 5.3, used as the compile target |
| **Visual Studio / Rider** | IDE for editing and building; target is **Development Editor** |

The **FactoryGame Unreal project** (`FactoryGame.uproject`) is the host project.
All mods live under `Mods/` and are loaded by Alpakit as UE plugins.

---

## Engine — UnrealEngine-CSS (UE 5.3)

Coffee Stain Studios ships a **custom fork** of Unreal Engine 5.3 alongside
Satisfactory.  This fork (`UnrealEngine-CSS`) differs from the public UE source
tree in ways that directly affect mod compilation:

- Several standard engine plugins are **absent or stripped** from the
  dedicated-server binaries (e.g. `EOSShared`, `EOSSDK`, `OnlineServicesEOS`,
  `OnlineServicesEOSGS`).
- The v1 `OnlineSubsystem` stack is **not reliably available** to mods.
  Use the CSS-native `OnlineIntegration` plugin instead.
- The built-in `WebSocketsModule` is **not available** to mods.
  Use the `SMLWebSocket` mod (see below).
- A `DummyHeaders` module provides stub headers for engine APIs that CSS UE
  omits.  **Every C++ mod must declare `"DummyHeaders"` as a dependency.**

The engine version association is recorded in `FactoryGame.uproject`:

```json
"EngineAssociation": "5.3.2-CSS"
```

---

## Mods in this repository

### `Mods/DiscordBridge/` — the main mod

**What it does:** Bridges the Satisfactory dedicated-server chat with a Discord
channel via a bot token.  Server-only (`RequiredOnRemote: false`).

**Version:** see `DiscordBridge.uplugin` (`SemVersion` / `VersionName`)

**Build targets (`Config/Alpakit.ini`):**
```
Windows, WindowsServer, LinuxServer
```

**Direct runtime dependencies (declared in `DiscordBridge.Build.cs`):**

| Module | Source | Purpose |
|--------|--------|---------|
| `Core`, `CoreUObject`, `Engine` | UE built-in | Standard UE base |
| `DummyHeaders` | CSS UE | Required stub headers (see above) |
| `FactoryGame` | Game source | `AFGChatManager`, `AFGPlayerController`, etc. |
| `SML` | `Mods/SML/` | SML runtime, mod-loading infrastructure |
| `SMLWebSocket` | `Mods/SMLWebSocket/` | WebSocket + SSL client (custom, see below) |
| `HTTP` | UE built-in | HTTP requests (Discord REST API calls) |
| `Json` | UE built-in | JSON serialisation (`FJsonObject`, `TJsonReader`) |
| `GameplayTags` | UE built-in | `FGameplayTag` declarations and lookups |
| `GameplayEvents` | CSS plugin (`Plugins/GameplayEvents/`) | Tag-based in-game event bus |
| `ReliableMessaging` | CSS plugin (`Plugins/ReliableMessaging/`) | Client→server relay channel |
| `OnlineIntegration` | CSS plugin (`Plugins/Online/OnlineIntegration/`) | CSS-native online services |
| `OnlineServicesInterface` | UE built-in (v2 OSS) | `FOnlineIdRegistryRegistry` declaration |
| `OnlineServicesCommon` | UE built-in (v2 OSS) | `FOnlineIdRegistryRegistry::Get()` definition |

**Plugin dependencies (declared in `DiscordBridge.uplugin`):**

| Plugin | SemVersion | Role |
|--------|-----------|------|
| `SML` | `^3.11.3` | Satisfactory Mod Loader |
| `SMLWebSocket` | `^1.0.0` | WebSocket library |
| `OnlineIntegration` | — | CSS online services layer |
| `GameplayEvents` | — | Event bus |
| `ReliableMessaging` | — | Client-to-server messaging |
| `CSSCompatStubs` | `^1.0.0` | Server-build compile stubs |

---

### `Mods/SMLWebSocket/` — custom WebSocket + SSL client

**What it does:** Implements the RFC 6455 WebSocket protocol over TCP with
optional TLS via OpenSSL.  Provides the `SMLWebSocket` UE module used by
DiscordBridge to connect to Discord's gateway.  Server-only
(`RequiredOnRemote: false`).  Required because UnrealEngine-CSS does not
expose the built-in `WebSocketsModule` to Alpakit mods.

**Build targets (`Config/Alpakit.ini`):** `Windows, WindowsServer, LinuxServer`

**Key Build.cs dependencies:**

| Module | Notes |
|--------|-------|
| `Core`, `CoreUObject`, `Engine` | UE base |
| `DummyHeaders` | CSS UE stub headers |
| `SML` | SML runtime |
| `Sockets` | `FSocket` / `ISocketSubsystem` (private dep) |
| `SSL`, `OpenSSL` | TLS support (Win64 and Linux only, private dep) |

---

### `Mods/CSSCompatStubs/` — server-build compile-time stubs

**What it does:** Provides empty stub implementations of EOS-related engine
modules that CSS custom UnrealEngine omits from its dedicated-server binaries.
Without these stubs, Alpakit fails to build DiscordBridge for
`WindowsServer` / `LinuxServer` targets because `OnlineIntegration`'s
transitive dependencies reference these modules.

**Build targets (`Config/Alpakit.ini`):** `Windows, WindowsServer, LinuxServer`

**Stub modules** (all have `TargetDenyList: ["Game"]` — compiled for Server
targets only; on Game targets the real CSS UE modules are used):

| Stub module | Replaces |
|-------------|---------|
| `EOSShared` | CSS UE engine plugin absent on dedicated server |
| `EOSSDK` | Epic Online Services SDK headers/libs absent on server |
| `OnlineServicesEOS` | UE v2 OSS EOS backend absent on server |
| `OnlineServicesEOSGS` | UE v2 OSS EOS Game Services backend absent on server |

---

### `Mods/SML/` — Satisfactory Mod Loader (upstream)

**What it does:** The foundational mod-loading runtime for Satisfactory.  All
mods in this repository depend on SML.  This is the upstream SML source; mods
in this repo are built *on top of* it.

---

### `Mods/Alpakit/` — the build & packaging tool (upstream)

**What it does:** An Unreal Editor plugin that adds the **Alpakit** panel to
the editor.  Provides the `Package` / `Package & Upload` workflow that compiles
and zips mods for distribution on [ficsit.app](https://ficsit.app).

---

## Build targets — what each Alpakit target means

| Alpakit target | UBT platform | UBT target type | Used for |
|---------------|-------------|-----------------|---------|
| `Windows` | Win64 | Game | Windows game client / listen-server host |
| `WindowsServer` | Win64 | Server | Windows dedicated server (`FactoryServer.exe`) |
| `LinuxServer` | Linux | Server | Linux dedicated server (`FactoryServer`) |

All three targets are listed in every mod's `Config/Alpakit.ini` so that the
packaged `.zip` contains binaries for every deployment type Satisfactory supports.

---

## `Config/Alpakit.ini` — per-mod target configuration

Each mod that needs to be packaged for specific targets carries a
`Config/Alpakit.ini` file.  Alpakit reads the `[ModTargets]` section to decide
which platforms to build.

```ini
[ModTargets]
+Targets=Windows
+Targets=WindowsServer
+Targets=LinuxServer
```

The `+Targets=` prefix is the array-append syntax used by Alpakit's
`FModTargetsConfig` (see `Mods/Alpakit/Source/Alpakit/Private/ModTargetsConfig.cpp`).

---

## `Build.cs` — required settings for every C++ mod

Every C++ mod in this repository uses the following UBT settings:

```csharp
CppStandard = CppStandardVersion.Cpp20;          // CSS UE 5.3 requires C++20
DefaultBuildSettings = BuildSettingsVersion.Latest;
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
bLegacyPublicIncludePaths = false;               // strict include hygiene
```

And **every** C++ mod must list `"DummyHeaders"` in its public dependencies:

```csharp
PublicDependencyModuleNames.Add("DummyHeaders");
```

---

## Where to find more detail

| Topic | File |
|-------|------|
| Full Alpakit project setup walkthrough | `Mods/DiscordBridge/Docs/00-BuildSystem.md` |
| Getting started (installing & configuring the mod) | `Mods/DiscordBridge/Docs/01-GettingStarted.md` |
| All DiscordBridge configuration options | `Mods/DiscordBridge/Docs/` (index at `README.md`) |
| CI/CD release workflow | `ReleaseProcess.md` |
