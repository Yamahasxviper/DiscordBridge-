# Third-Party C++ Libraries — Adding a Library as an Alpakit Module

← [Back to index](README.md)

This guide is for **mod developers** who need to ship a third-party C++ library
alongside their Alpakit mod.  The library must be compiled as a separate
**Unreal module** (its own entry in the `.uplugin` file) rather than being
linked directly into the main mod module.

The worked example throughout this page is **SMLWebSocket**, which wraps the
OpenSSL and Sockets APIs and is included in this repository at
`Mods/SMLWebSocket/`.

---

## Why a separate module?

UnrealBuildTool (UBT) resolves include paths and link dependencies **per
module**.  Embedding a third-party library directly inside your main mod module
works for simple header-only libraries, but causes problems when the library:

* requires platform-specific link flags or pre-compiled static archives (`.lib`
  / `.a`),
* defines symbols that would collide with other modules if exposed through
  the public link interface, or
* needs to be shared by more than one mod without duplicate symbol errors at
  link time.

Wrapping the library in its own module keeps its symbols private and lets any
number of mods take it as a module dependency.

---

## Module definition

### 1 — Directory structure

Create the following directory tree inside `Mods/` (replace `MyLibWrapper` with
your module name):

```
Mods/
└── MyLibWrapper/                    ← plugin root (same name as your module)
    ├── MyLibWrapper.uplugin         ← plugin descriptor
    ├── Config/
    │   └── Alpakit.ini              ← build targets
    └── Source/
        └── MyLibWrapper/            ← module source directory (same name as module)
            ├── MyLibWrapper.Build.cs
            ├── Public/
            │   └── MyLibWrapper.h   ← module interface header
            └── Private/
                └── MyLibWrapper.cpp ← IMPLEMENT_MODULE(...)
```

> **Naming convention:** the plugin directory, the `.uplugin` file, the source
> subdirectory, the `Build.cs` file, and the module name declared in
> `IMPLEMENT_MODULE` must all match exactly.

---

### 2 — Plugin descriptor (`.uplugin`)

Create `Mods/MyLibWrapper/MyLibWrapper.uplugin`:

```json
{
    "FileVersion": 3,
    "Version": 1,
    "VersionName": "1.0.0",
    "SemVersion": "1.0.0",
    "GameVersion": ">=416835",
    "FriendlyName": "My Library Wrapper",
    "Description": "Wraps libfoo for use by Satisfactory mods.",
    "Category": "Utility",
    "CreatedBy": "YourName",
    "CanContainContent": false,
    "IsBetaVersion": false,
    "IsExperimentalVersion": false,
    "Installed": false,
    "RequiredOnRemote": false,
    "SupportedTargetPlatforms": [
        "Win64",
        "Linux"
    ],
    "Modules": [
        {
            "Name": "MyLibWrapper",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        }
    ],
    "Plugins": [
        {
            "Name": "SML",
            "Enabled": true,
            "SemVersion": "^3.11.3"
        }
    ]
}
```

Key fields:

| Field | Notes |
|-------|-------|
| `"RequiredOnRemote": false` | Third-party wrapper modules are server-only; clients do not need them. |
| `"SupportedTargetPlatforms"` | Match the platforms your library supports (`Win64` and/or `Linux`). |
| `"Modules[].LoadingPhase"` | Use `"Default"` unless the library must be initialised before game objects are created (use `"PostConfigInit"` in that case). |

---

### 3 — Alpakit targets (`Config/Alpakit.ini`)

Create `Mods/MyLibWrapper/Config/Alpakit.ini`:

```ini
[Alpakit]
+Targets=Windows
+Targets=WindowsServer
+Targets=LinuxServer
```

All three entries are required so the module is compiled for every Satisfactory
dedicated-server platform.

---

### 4 — Module build file (`Build.cs`)

Create `Mods/MyLibWrapper/Source/MyLibWrapper/MyLibWrapper.Build.cs`:

```csharp
using UnrealBuildTool;

public class MyLibWrapper : ModuleRules
{
    public MyLibWrapper(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DummyHeaders",   // Required: CSS UE header stubs
            "SML",
        });

        // Link the third-party library privately so its symbols are not
        // leaked into the public link interface of this module.
        if (Target.Platform == UnrealTargetPlatform.Win64 ||
            Target.Platform == UnrealTargetPlatform.Linux)
        {
            PrivateDependencyModuleNames.AddRange(new string[]
            {
                "MyThirdPartyLib",   // UBT module name for the library
            });
        }
    }
}
```

> **Private vs. public dependency:** use `PrivateDependencyModuleNames` for the
> third-party library so that modules which depend on `MyLibWrapper` do not
> transitively inherit its link flags.  Expose only what downstream consumers
> genuinely need through `PublicDependencyModuleNames`.

---

### 5 — Module public header

Create `Mods/MyLibWrapper/Source/MyLibWrapper/Public/MyLibWrapper.h`:

```cpp
#pragma once

#include "Modules/ModuleManager.h"

/** Module that initialises and shuts down libfoo for Satisfactory mods. */
class FMyLibWrapperModule : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;
};
```

---

### 6 — Module implementation

Create `Mods/MyLibWrapper/Source/MyLibWrapper/Private/MyLibWrapper.cpp`:

```cpp
#include "MyLibWrapper.h"

// Suppress compiler warnings emitted by third-party headers under UBT's
// strict warning settings (/WX on MSVC, -Werror on GCC/Clang).
#if PLATFORM_WINDOWS || PLATFORM_LINUX
THIRD_PARTY_INCLUDES_START
#include "libfoo/foo.h"
THIRD_PARTY_INCLUDES_END
#endif

void FMyLibWrapperModule::StartupModule()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
    foo_global_init();
#endif
}

void FMyLibWrapperModule::ShutdownModule()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
    foo_global_cleanup();
#endif
}

IMPLEMENT_MODULE(FMyLibWrapperModule, MyLibWrapper)
```

> **`THIRD_PARTY_INCLUDES_START` / `THIRD_PARTY_INCLUDES_END`** suppress MSVC
> and GCC/Clang warnings produced by the library's own headers.  Always wrap
> third-party `#include` directives with these macros.

---

## Consuming the wrapper from your main mod

### `Build.cs` — declare the dependency

In your main mod's `Build.cs` add `MyLibWrapper` as a public dependency:

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine",
    "DummyHeaders",
    "SML",
    "MyLibWrapper",   // ← add this
});
```

### `.uplugin` — declare the plugin dependency

In your main mod's `.uplugin` add an entry to the `"Plugins"` array:

```json
{
    "Name": "MyLibWrapper",
    "Enabled": true,
    "SemVersion": "^1.0.0"
}
```

This tells UBT and Alpakit that your mod plugin requires `MyLibWrapper` to be
present and compiled before your mod.

---

## Worked example — SMLWebSocket and OpenSSL

SMLWebSocket (`Mods/SMLWebSocket/`) is the reference implementation in this
repository.  It wraps OpenSSL and Unreal's Sockets API for use by DiscordBridge.

| File | Purpose |
|------|---------|
| `Mods/SMLWebSocket/SMLWebSocket.uplugin` | Plugin descriptor; registers the `SMLWebSocket` module |
| `Mods/SMLWebSocket/Config/Alpakit.ini` | Declares `Windows`, `WindowsServer`, `LinuxServer` targets |
| `Mods/SMLWebSocket/Source/SMLWebSocket/SMLWebSocket.Build.cs` | Links `SSL` and `OpenSSL` as **private** dependencies; `Sockets` as private too |
| `Mods/SMLWebSocket/Source/SMLWebSocket/Public/SMLWebSocket.h` | `FSMLWebSocketModule` declaration |
| `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocket.cpp` | OpenSSL global init/cleanup; `IMPLEMENT_MODULE` |

**OpenSSL namespace collision workaround (Windows only)**

OpenSSL's `ossl_typ.h` declares `typedef struct ui_st UI` at global scope.
Unreal's Slate/InputCore also declares `namespace UI {}` globally.  On MSVC
this produces error C2365.  The fix is to rename the OpenSSL typedef before
including its headers:

```cpp
THIRD_PARTY_INCLUDES_START
#pragma push_macro("UI")
#define UI UI_OSSLRenamed
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/crypto.h"
#pragma pop_macro("UI")
THIRD_PARTY_INCLUDES_END
```

On Linux this sequence is a harmless no-op.

DiscordBridge declares the dependency in `DiscordBridge.uplugin`:

```json
{
    "Name": "SMLWebSocket",
    "Enabled": true,
    "SemVersion": "^1.0.0"
}
```

And in `DiscordBridge.Build.cs`:

```csharp
PublicDependencyModuleNames.Add("SMLWebSocket");
```

---

## Common issues

| Error | Likely cause | Fix |
|-------|-------------|-----|
| `cannot open input file 'libfoo.lib'` | Library static archive not found by UBT | Verify the UBT module name and that the library ships with the CSS UE installation |
| Include errors from third-party headers | Missing `THIRD_PARTY_INCLUDES_START` / `THIRD_PARTY_INCLUDES_END` | Wrap all third-party `#include` lines with these macros |
| `error C2365` (`UI` redefinition on Windows) | OpenSSL `UI` typedef collides with UE `namespace UI` | Use the `push_macro` / `pop_macro` trick shown in the SMLWebSocket example |
| Module not found at runtime | Missing `"Plugins"` entry in the consumer's `.uplugin` | Add `{ "Name": "MyLibWrapper", "Enabled": true }` to the `"Plugins"` array |
| Duplicate symbol linker errors | Library linked as a **public** dependency in two modules | Change the consumer's dependency to `PrivateDependencyModuleNames` |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
