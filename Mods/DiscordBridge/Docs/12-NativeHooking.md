# Native Hooking — SML C++ Hook Reference

← [Back to index](README.md)

This document captures everything a mod developer needs to know about SML's
**native (C++) hooking** system so it can be consulted quickly when writing or
debugging hooks in DiscordBridge or any other Alpakit C++ mod.

The authoritative source file is:

```
Mods/SML/Source/SML/Public/Patching/NativeHookManager.h
```

---

## Background

Satisfactory ships **PDB files** alongside its executables.  PDBs let SML
resolve a C++ function symbol name to its run-time address without needing to
reverse-engineer the binary.

Coffee Stain also enabled **Modular Build** (as of Update 4), which means every
Unreal module is compiled to its own `.dll`.  All module-local functions are
therefore *exported*, which means:

* No function can be inlined away — every function listed in a header can be
  hooked.
* The linker can locate each function by name without extra PE-section tricks.

> For Blueprint-implemented functions see the **Blueprint Hooking System**
> instead (`BlueprintHookManager.h`).

---

## Core concepts

### Hook execution order

When multiple hooks are attached to the same function they fire **in the order
they were registered**.

Each hook has a **call order**:

| Order | Macro suffix | When called |
|-------|-------------|-------------|
| Before | *(no suffix)* | Before the original function |
| After | `_AFTER` | After the original function (or after the before-hook cancelled execution) |

### `TCallScope<…>` — the scope object

The first parameter of every **before** hook is a `TCallScope` reference.  It
lets you control whether the real function body (and subsequent hooks) actually
runs.

```cpp
// Signature for a void before-hook
void MyHook(TCallScope<void(*)(UMyClass*, int)>& Scope, UMyClass* Self, int Arg);

// Signature for a non-void before-hook
void MyHook(TCallScope<FString(*)(UMyClass*)>& Scope, UMyClass* Self);
```

#### `TCallScope` API

| Method | Effect |
|--------|--------|
| `Scope.Cancel()` | Prevents the original function **and** all subsequent hooks from running (void return only). |
| `Scope.Override(NewValue)` | Sets a replacement return value and cancels the original call (non-void). |
| `Scope.GetResult()` | Returns the current return value (valid after the original function has been called). |
| `Scope(args…)` | Manually calls the next hooks and the original function from inside your hook body. |

> **Note:** if you do not call `Scope.Cancel()` / `Scope.Override()` and do not
> call `Scope()` manually, the original function is called *implicitly* after
> your hook returns.

### `FDelegateHandle` — handle to a registered hook

Every `SUBSCRIBE_*` macro returns an `FDelegateHandle`.  Store it if you need
to unsubscribe later (e.g., on module shutdown).

```cpp
FDelegateHandle Handle = SUBSCRIBE_METHOD(AMyActor::BeginPlay, [](auto& Scope, AMyActor* Self) {
    // …
});

// Later, to remove the hook:
UNSUBSCRIBE_METHOD(AMyActor::BeginPlay, Handle);
```

---

## Macro reference

All macros expand to a call that installs the low-level trampoline (once) and
registers the supplied handler lambda or function pointer.

### Before-hooks — non-virtual member functions

```cpp
FDelegateHandle Handle = SUBSCRIBE_METHOD(AMyActor::SomeMethod, Handler);
```

* Installs a **before** hook on `AMyActor::SomeMethod`.
* Use when the method is **not** virtual (or when you know the concrete
  implementation address at compile time).

### After-hooks — non-virtual member functions

```cpp
FDelegateHandle Handle = SUBSCRIBE_METHOD_AFTER(AMyActor::SomeMethod, Handler);
```

* Fires **after** the original function runs.
* The handler signature does **not** include `TCallScope` — just the return
  value (if non-void) then the normal parameters:

```cpp
// After-hook for a method returning bool
[](bool ReturnValue, AMyActor* Self, int Arg) { … }

// After-hook for a void method
[](AMyActor* Self, int Arg) { … }
```

### Explicit signature variants

Use these when the compiler cannot deduce the correct overload:

```cpp
// Before
FDelegateHandle Handle = SUBSCRIBE_METHOD_EXPLICIT(
    void(AMyActor::*)(int),   // explicit method signature
    AMyActor::SomeMethod,
    Handler);

// After
FDelegateHandle Handle = SUBSCRIBE_METHOD_EXPLICIT_AFTER(
    void(AMyActor::*)(int),
    AMyActor::SomeMethod,
    Handler);
```

### Virtual method hooks

Virtual methods require a **sample object instance** so SML can look up the
correct vtable slot at run-time:

```cpp
// Before
FDelegateHandle Handle = SUBSCRIBE_METHOD_VIRTUAL(
    AMyActor::SomeVirtualMethod,
    GetMutableDefault<AMyActor>(),   // sample instance
    Handler);

// After
FDelegateHandle Handle = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
    AMyActor::SomeVirtualMethod,
    GetMutableDefault<AMyActor>(),
    Handler);

// Explicit overload selection + virtual
FDelegateHandle Handle = SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL(
    void(AMyActor::*)(int),
    AMyActor::SomeVirtualMethod,
    GetMutableDefault<AMyActor>(),
    Handler);
```

### `UObject` convenience macros

These macros call `GetDefault<ObjectClass>()` automatically for the sample
instance, so you do not have to supply one:

```cpp
// Before
FDelegateHandle Handle = SUBSCRIBE_UOBJECT_METHOD(AMyActor, SomeMethod, Handler);

// After
FDelegateHandle Handle = SUBSCRIBE_UOBJECT_METHOD_AFTER(AMyActor, SomeMethod, Handler);

// Explicit overload selection
FDelegateHandle Handle = SUBSCRIBE_UOBJECT_METHOD_EXPLICIT(
    void(AMyActor::*)(int),
    AMyActor, SomeMethod, Handler);

FDelegateHandle Handle = SUBSCRIBE_UOBJECT_METHOD_EXPLICIT_AFTER(
    void(AMyActor::*)(int),
    AMyActor, SomeMethod, Handler);
```

### Unsubscribing

```cpp
// Standard symbol
UNSUBSCRIBE_METHOD(AMyActor::SomeMethod, Handle);

// Explicit overload
UNSUBSCRIBE_METHOD_EXPLICIT(void(AMyActor::*)(int), AMyActor::SomeMethod, Handle);

// UObject helper
UNSUBSCRIBE_UOBJECT_METHOD(AMyActor, SomeMethod, Handle);

// UObject helper + explicit overload
UNSUBSCRIBE_UOBJECT_METHOD_EXPLICIT(void(AMyActor::*)(int), AMyActor, SomeMethod, Handle);
```

> When the last handler for a function is removed, SML automatically uninstalls
> the trampoline and frees its resources.

---

## Practical examples

### 1 — Intercept and cancel a void method

```cpp
#include "Patching/NativeHookManager.h"
#include "FGChatManager.h"

static FDelegateHandle GChatHookHandle;

void InstallChatHook()
{
    GChatHookHandle = SUBSCRIBE_UOBJECT_METHOD(AFGChatManager, AddChatMessage,
        [](TCallScope<void(*)(AFGChatManager*, const FChatMessageStruct&)>& Scope,
           AFGChatManager* Self,
           const FChatMessageStruct& Msg)
        {
            if (Msg.MessageString.Contains(TEXT("SPAM")))
            {
                Scope.Cancel();   // Drop the message entirely
                return;
            }
            // Otherwise let the original implementation run
        });
}

void RemoveChatHook()
{
    UNSUBSCRIBE_UOBJECT_METHOD(AFGChatManager, AddChatMessage, GChatHookHandle);
}
```

### 2 — Override a non-void return value

```cpp
#include "Patching/NativeHookManager.h"
#include "FGPlayerController.h"

static FDelegateHandle GCanInteractHandle;

void InstallInteractHook()
{
    GCanInteractHandle = SUBSCRIBE_UOBJECT_METHOD(AFGPlayerController, CanInteract,
        [](TCallScope<bool(*)(AFGPlayerController*)>& Scope,
           AFGPlayerController* Self)
        {
            // Always allow interaction regardless of the original logic
            Scope.Override(true);
        });
}
```

### 3 — Read the result in an after-hook

```cpp
#include "Patching/NativeHookManager.h"
#include "FGBuildGun.h"

static FDelegateHandle GBuildGunHandle;

void InstallBuildGunHook()
{
    GBuildGunHandle = SUBSCRIBE_UOBJECT_METHOD_AFTER(AFGBuildGun, GetBuildGunState,
        [](EBuildGunState ReturnValue, AFGBuildGun* Self)
        {
            UE_LOG(LogTemp, Log, TEXT("BuildGun state is now: %d"), (int32)ReturnValue);
        });
}
```

### 4 — Explicit overload selection (function is overloaded)

```cpp
#include "Patching/NativeHookManager.h"
#include "FGCharacterPlayer.h"

static FDelegateHandle GDamageHandle;

void InstallDamageHook()
{
    // TakeDamage is overloaded; pick the exact signature we want
    using Sig = float(ACharacter::*)(float, FDamageEvent const&, AController*, AActor*);

    GDamageHandle = SUBSCRIBE_METHOD_EXPLICIT(
        Sig,
        AFGCharacterPlayer::TakeDamage,
        [](TCallScope<float(*)(AFGCharacterPlayer*, float, FDamageEvent const&, AController*, AActor*)>& Scope,
           AFGCharacterPlayer* Self,
           float DamageAmount,
           FDamageEvent const& DamageEvent,
           AController* EventInstigator,
           AActor* DamageCauser)
        {
            // Halve all incoming damage
            Scope.Override(DamageAmount * 0.5f);
        });
}
```

---

## Quick-reference table

| Task | Macro |
|------|-------|
| Hook member fn (before) | `SUBSCRIBE_METHOD` |
| Hook member fn (after) | `SUBSCRIBE_METHOD_AFTER` |
| Hook member fn, explicit sig (before) | `SUBSCRIBE_METHOD_EXPLICIT` |
| Hook member fn, explicit sig (after) | `SUBSCRIBE_METHOD_EXPLICIT_AFTER` |
| Hook virtual fn (before) | `SUBSCRIBE_METHOD_VIRTUAL` |
| Hook virtual fn (after) | `SUBSCRIBE_METHOD_VIRTUAL_AFTER` |
| Hook virtual fn, explicit sig (before) | `SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL` |
| Hook virtual fn, explicit sig (after) | `SUBSCRIBE_METHOD_EXPLICIT_VIRTUAL_AFTER` |
| Hook UObject method (before) | `SUBSCRIBE_UOBJECT_METHOD` |
| Hook UObject method (after) | `SUBSCRIBE_UOBJECT_METHOD_AFTER` |
| Hook UObject method, explicit sig (before) | `SUBSCRIBE_UOBJECT_METHOD_EXPLICIT` |
| Hook UObject method, explicit sig (after) | `SUBSCRIBE_UOBJECT_METHOD_EXPLICIT_AFTER` |
| Remove any hook | `UNSUBSCRIBE_METHOD` / `UNSUBSCRIBE_UOBJECT_METHOD` (+ `_EXPLICIT` variants) |
| Cancel original call (void) | `Scope.Cancel()` |
| Replace return value + cancel | `Scope.Override(value)` |
| Read current return value | `Scope.GetResult()` |
| Call original from inside hook | `Scope(args…)` |

---

## Common pitfalls

| Problem | Cause | Fix |
|---------|-------|-----|
| Hook never fires | Function is inlined in a non-modular build | Satisfactory uses modular build — this should not happen in CSS UE; double-check the header declares the function `SML_API` or is in a separate module. |
| Crash on `Scope.Cancel()` | Called on a non-void hook | Use `Scope.Override(defaultValue)` for non-void functions instead. |
| Hook fires on the wrong overload | Method is overloaded | Use `SUBSCRIBE_METHOD_EXPLICIT` with the exact function signature. |
| Sample instance crash for virtual hook | `nullptr` passed as sample instance | Pass `GetDefault<MyClass>()` or `GetMutableDefault<MyClass>()`; use `SUBSCRIBE_UOBJECT_METHOD` which does this automatically. |
| Second hook never runs | An earlier hook called `Scope.Cancel()` | Hooks registered earlier in call order can block later ones — registration order matters. |
| Linker error: `HookInvoker` not found | Missing `#include "Patching/NativeHookManager.h"` | Add the include. |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
