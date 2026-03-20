# Third-Party C++ Libraries — Integration Guide

← [Back to index](README.md)

This guide is for **mod developers** who need to bundle an external C++ library
(static `.lib` / `.a`, shared `.dll` / `.so`, or header-only) inside an Alpakit
mod plugin.  If you are adding a library that is already shipped with the CSS
UnrealEngine build (e.g. `SSL`, `OpenSSL`, `Sockets`) you only need to list it
in `PrivateDependencyModuleNames` — no extra setup is required.  See
[00-BuildSystem.md](00-BuildSystem.md) for those engine-provided modules.

---

## Overview

Unreal Build Tool (UBT) does not allow a module to add arbitrary linker flags
from inside `Build.cs`.  Every binary must be described as a **UBT module** with
its own `Build.cs`.  When you bring in a third-party C++ library you therefore
need to:

1. Create a **ThirdParty module** in your plugin — a `Build.cs`-only module that
   tells UBT where the library headers and pre-compiled binaries live.
2. Reference that ThirdParty module from your main runtime module's `Build.cs`.
3. (Optionally) register the module in your plugin's `.uplugin` descriptor so
   that UBT can locate it as part of your plugin.

---

## Directory structure

Lay out your plugin folder following this pattern:

```
Mods/
└── YourMod/
    ├── YourMod.uplugin
    └── Source/
        ├── YourMod/                       ← your main runtime module
        │   ├── YourMod.Build.cs
        │   ├── Public/
        │   └── Private/
        └── ThirdParty/
            └── LibraryName/               ← ThirdParty UBT module
                ├── LibraryName.Build.cs
                ├── include/               ← public headers
                │   └── library.h
                └── lib/
                    ├── Win64/
                    │   ├── library.lib    ← import lib (MSVC)
                    │   └── library.dll    ← runtime DLL (if shared)
                    └── Linux/
                        └── liblibrary.a   ← static lib (GCC/Clang)
```

> **Header-only libraries** still need a `Build.cs` to expose the include path;
> the `lib/` directory is not required in that case.

---

## ThirdParty module — `Build.cs`

Create `Source/ThirdParty/LibraryName/LibraryName.Build.cs`:

```csharp
using System.IO;
using UnrealBuildTool;

public class LibraryName : ModuleRules
{
    public LibraryName(ReadOnlyTargetRules Target) : base(Target)
    {
        // This is a ThirdParty module — no UE source files, only path setup.
        Type = ModuleType.External;

        // Expose headers to any module that lists us as a dependency.
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));

        string LibDir = Path.Combine(ModuleDirectory, "lib");

        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            // Link the import library at compile time.
            PublicAdditionalLibraries.Add(
                Path.Combine(LibDir, "Win64", "library.lib"));

            // Stage the runtime DLL alongside the game binary.
            // Remove this block for static libraries.
            RuntimeDependencies.Add(
                Path.Combine(LibDir, "Win64", "library.dll"));
        }
        else if (Target.Platform == UnrealTargetPlatform.Linux)
        {
            PublicAdditionalLibraries.Add(
                Path.Combine(LibDir, "Linux", "liblibrary.a"));
        }
    }
}
```

Key points:

| Setting | Purpose |
|---------|---------|
| `Type = ModuleType.External` | Tells UBT this module has no C++ source of its own — only path/linker configuration. |
| `PublicIncludePaths` | Exposes headers to any module that depends on this one. |
| `PublicAdditionalLibraries` | Passes the library to the linker. Use absolute paths (built with `Path.Combine`). |
| `RuntimeDependencies` | Copies a shared library (`.dll` / `.so`) to the staging directory so the OS can load it at runtime. Omit for static libraries. |

---

## Main module — referencing the ThirdParty module

In `Source/YourMod/YourMod.Build.cs`, add the ThirdParty module as a
**private** dependency (unless other mods need to include your library's headers
directly, in which case use `PublicDependencyModuleNames`):

```csharp
using UnrealBuildTool;

public class YourMod : ModuleRules
{
    public YourMod(ReadOnlyTargetRules Target) : base(Target)
    {
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core", "CoreUObject", "Engine",
            "DummyHeaders",    // required by all Alpakit C++ mods
            "SML",
        });

        // Add the ThirdParty module — private because callers
        // of YourMod do not need the library's headers directly.
        PrivateDependencyModuleNames.Add("LibraryName");
    }
}
```

---

## Plugin descriptor — `.uplugin`

Add the module to the `Modules` array in `YourMod.uplugin` with
`"Type": "External"`:

```json
{
    "Modules": [
        {
            "Name": "YourMod",
            "Type": "Runtime",
            "LoadingPhase": "Default"
        },
        {
            "Name": "LibraryName",
            "Type": "External"
        }
    ]
}
```

> `"Type": "External"` matches `ModuleType.External` in the `Build.cs` and
> tells Alpakit / UBT that this is a configuration-only module with no compiled
> C++ output of its own.

---

## Platform guards

Satisfactory's dedicated server targets are **Win64** and **Linux** only.  If
your pre-compiled library is not available for all targets you must guard the
linker flags so that UBT does not fail on unsupported platforms:

```csharp
if (Target.Platform == UnrealTargetPlatform.Win64 ||
    Target.Platform == UnrealTargetPlatform.Linux)
{
    PublicAdditionalLibraries.Add(/* … */);
}
else
{
    // Optionally warn developers on unsupported platforms.
    System.Console.WriteLine(
        $"LibraryName: unsupported platform {Target.Platform}");
}
```

This is the same pattern used by `SMLWebSocket.Build.cs` for its `SSL` and
`OpenSSL` dependencies.

---

## Static vs. shared libraries

| Library type | `.lib` / `.a` | `.dll` / `.so` | `RuntimeDependencies` entry |
|-------------|:---:|:---:|:---:|
| **Static** | ✔ required | — | not needed |
| **Shared / DLL** | ✔ (import lib on Win64) | ✔ required | ✔ required |

For Satisfactory mods, **static libraries are strongly preferred** because:

- No separate DLL needs to be staged or deployed alongside the `.pak`.
- There are no symbol-visibility conflicts with other mods.
- Alpakit packages the `.pak` only; extra DLLs require manual deployment steps.

---

## Example — bundling a header-only library

For a header-only library no pre-compiled binaries are needed:

```csharp
// Source/ThirdParty/nlohmann_json/nlohmann_json.Build.cs

using System.IO;
using UnrealBuildTool;

public class nlohmann_json : ModuleRules
{
    public nlohmann_json(ReadOnlyTargetRules Target) : base(Target)
    {
        Type = ModuleType.External;
        PublicIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
    }
}
```

Directory layout:

```
Source/
└── ThirdParty/
    └── nlohmann_json/
        ├── nlohmann_json.Build.cs
        └── include/
            └── nlohmann/
                └── json.hpp
```

---

## Common errors

| Error | Likely cause | Fix |
|-------|-------------|-----|
| `Cannot open input file 'library.lib'` | Path passed to `PublicAdditionalLibraries` is wrong | Use `Path.Combine(ModuleDirectory, …)` with the correct relative path from the `.Build.cs` file |
| `undefined reference to 'LibFunction'` (Linux) | Static library not listed in `PublicAdditionalLibraries` | Add the `.a` path inside the Linux platform guard |
| `DLL not found at runtime` (Win64) | `RuntimeDependencies` entry missing for a shared library | Add `RuntimeDependencies.Add(Path.Combine(LibDir, "Win64", "library.dll"))` |
| Include `<library.h>` not found | `PublicIncludePaths` path is wrong, or `LibraryName` not in `DependencyModuleNames` | Check both the path and that the ThirdParty module is listed as a dependency |
| UBT: `No modules found for plugin YourMod` | Module name in `.uplugin` doesn't match the folder / `.Build.cs` class name | Ensure `"Name"` in `Modules[]` matches the `Build.cs` class name exactly |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
