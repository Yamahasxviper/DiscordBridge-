# SMLWebSocket

**Version:** 1.0.0 | **Satisfactory build:** ≥ 416835 | **SML:** ≥ 3.11.3

SMLWebSocket is a **server-only** dependency plugin that provides a custom WebSocket + SSL/OpenSSL client for Satisfactory mods built with Alpakit. Players connecting to your server do **not** need to install it — `RequiredOnRemote` is `false`.

It implements the WebSocket protocol (RFC 6455) over TCP with optional TLS encryption using OpenSSL, and supports automatic reconnection with exponential back-off. It exists because Unreal Engine's built-in `WebSocketsModule` is **not available** in Alpakit-packaged mods targeting Coffee Stain Studios' custom Unreal Engine 5.3 build (**UnrealEngine-CSS**).

---

## Why is this needed?

Coffee Stain Studios ships Satisfactory with a **custom Unreal Engine 5.3 fork**. Unreal's built-in `WebSocketsModule` is not present in Alpakit packages for this engine. Any mod that needs WebSocket connectivity (e.g. DiscordBridge connecting to the Discord Gateway) must use SMLWebSocket instead.

---

## Usage

### C++ — include the public header

```cpp
#include "SMLWebSocketClient.h"
```

Add `"SMLWebSocket"` to `PublicDependencyModuleNames` (or `PrivateDependencyModuleNames`) in your `Build.cs`.

### Create and connect

```cpp
USMLWebSocketClient* WS = NewObject<USMLWebSocketClient>(this);
WS->bAutoReconnect = true;
WS->ReconnectInitialDelaySeconds = 2.0f;
WS->MaxReconnectDelaySeconds = 60.0f;

WS->OnConnected.AddDynamic(this, &UMyClass::HandleConnected);
WS->OnMessage.AddDynamic(this, &UMyClass::HandleMessage);
WS->OnClosed.AddDynamic(this, &UMyClass::HandleClosed);
WS->OnError.AddDynamic(this, &UMyClass::HandleError);

WS->Connect(TEXT("wss://gateway.discord.gg/?v=10&encoding=json"));
```

### Send a message

```cpp
WS->SendText(TEXT("{\"op\":2,...}"));
```

### Disconnect cleanly (no reconnect)

```cpp
WS->Close();
```

---

## `USMLWebSocketClient` API

| Property / Method | Type | Description |
|-------------------|------|-------------|
| `bAutoReconnect` | `bool` | Automatically reconnect after a drop (default: `false`) |
| `ReconnectInitialDelaySeconds` | `float` | Delay before the first reconnect attempt |
| `MaxReconnectDelaySeconds` | `float` | Maximum delay between reconnect attempts (exponential back-off) |
| `Connect(URL)` | — | Open a `ws://` or `wss://` connection |
| `SendText(Message)` | — | Send a UTF-8 text frame |
| `SendBinary(Data, bIsFinal)` | — | Send a binary frame or fragment |
| `Close()` | — | Close the connection and cancel any pending reconnect |
| `IsConnected()` | `bool` | `true` when the handshake is complete and the socket is open |
| `OnConnected` | delegate | Fired on the game thread when the handshake succeeds |
| `OnMessage` | delegate | Fired on the game thread when a UTF-8 text message arrives |
| `OnBinaryMessage` | delegate | Fired on the game thread when a binary message (or fragment) arrives |
| `OnClosed` | delegate | Fired on the game thread when the connection closes (status code + reason) |
| `OnError` | delegate | Fired on the game thread when an error occurs |
| `OnReconnecting` | delegate | Fired on the game thread before each reconnect attempt |

All delegate callbacks are delivered on the **game thread** — it is safe to update game state directly from them.

---

## Building from Source (Alpakit / UnrealEngine-CSS)

SMLWebSocket is built as an **Alpakit C++ mod** targeting **UnrealEngine-CSS** (UE 5.3).

**`Build.cs` module dependencies:**

```csharp
PublicDependencyModuleNames.AddRange(new string[]
{
    "Core", "CoreUObject", "Engine",
    "DummyHeaders", // stub headers required by all Alpakit C++ mods
    "SML",
});

PrivateDependencyModuleNames.AddRange(new string[]
{
    "Sockets",  // FSocket / ISocketSubsystem (private — only used in the runnable)
});

// SSL and OpenSSL — only available on the two supported dedicated-server platforms
if (Target.Platform == UnrealTargetPlatform.Win64 ||
    Target.Platform == UnrealTargetPlatform.Linux)
{
    PrivateDependencyModuleNames.AddRange(new string[] { "SSL", "OpenSSL" });
}
```

Key Alpakit / CSS build constraints:

- Always add `"DummyHeaders"` to `PublicDependencyModuleNames`.
- Do **not** add `"WebSockets"` or `"WebSocketsModule"` — not present in Alpakit packages.
- SSL/OpenSSL dependencies are guarded to Win64 and Linux only (the two supported dedicated-server platforms).

Recommended `Build.cs` settings:

```csharp
CppStandard = CppStandardVersion.Cpp20;
DefaultBuildSettings = BuildSettingsVersion.Latest;
PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
bLegacyPublicIncludePaths = false;
```

---

## Dependencies

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |

For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>
