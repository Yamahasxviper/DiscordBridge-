# SMLWebSocket – RFC 6455 WebSocket Client for Satisfactory Mods

A standalone C++ mod that provides a **custom WebSocket client with SSL/OpenSSL support** for Satisfactory dedicated server mods built with Alpakit. It implements the full WebSocket protocol (RFC 6455) over TCP with optional TLS encryption, without requiring Unreal Engine's built-in WebSocket module (which is unavailable in Alpakit-packaged mods).

---

## Why is this needed?

Satisfactory uses a **custom Coffee Stain Studios Unreal Engine build** that strips out several engine modules not relevant to the game, including `WebSockets`. Any mod that needs to open a persistent WebSocket connection (for example, to the Discord Gateway at `wss://gateway.discord.gg/`) would normally rely on `FWebSocketsModule` – but that module is not present.

SMLWebSocket provides a **drop-in replacement** built directly on:

- **FSocket / ISocketSubsystem** – Unreal's low-level TCP socket layer (always available)
- **OpenSSL** – for TLS on `wss://` URLs (available on Win64 and Linux server targets)

Mods that require a live WebSocket connection (such as DiscordBridge and TicketSystem in standalone mode) list SMLWebSocket as a dependency and use `USMLWebSocketClient` instead of the engine's built-in client.

---

## Features

| Feature | Details |
|---------|---------|
| `ws://` and `wss://` | Plain TCP and TLS (OpenSSL) connections |
| RFC 6455 framing | Text frames, binary frames, ping/pong, close handshake |
| Auto-reconnect | Exponential back-off with configurable initial delay, max delay, and max attempts |
| Blueprint API | Full Blueprint exposure – `CreateWebSocketClient`, `Connect`, `SendText`, `SendBinary`, `Close`, `IsConnected` |
| C++ API | Inherit from or hold a `USMLWebSocketClient*`; bind delegates in C++ |
| Thread-safe callbacks | All delegate callbacks (`OnConnected`, `OnMessage`, `OnBinaryMessage`, `OnClosed`, `OnError`, `OnReconnecting`) are fired on the **game thread** |
| Alpakit-compatible | No dependency on engine modules absent from Satisfactory's custom UE build |

---

## Delegates

| Delegate | Signature | When it fires |
|----------|-----------|---------------|
| `OnConnected` | `()` | WebSocket handshake succeeded; connection is ready |
| `OnMessage` | `(FString Message)` | A UTF-8 text message was received |
| `OnBinaryMessage` | `(TArray<uint8> Data, bool bIsFinal)` | A binary frame (or fragment) was received |
| `OnClosed` | `(int32 StatusCode, FString Reason)` | Connection closed (either side) |
| `OnError` | `(FString ErrorMessage)` | Connection or protocol error |
| `OnReconnecting` | `(int32 AttemptNumber, float DelaySeconds)` | About to retry after a non-user-initiated disconnect |

---

## Reconnect behaviour

When `bAutoReconnect = true` (the default) and the server drops the connection, the client:

1. Fires `OnClosed`.
2. Waits `ReconnectInitialDelaySeconds` (default: 2 s).
3. Fires `OnReconnecting(1, delay)` then attempts to reconnect.
4. On each subsequent failure, doubles the delay up to `MaxReconnectDelaySeconds` (default: 30 s).
5. Stops retrying after `MaxReconnectAttempts` failed attempts (0 = unlimited).

Calling `Close()` always suppresses the reconnect loop regardless of `bAutoReconnect`.

---

## C++ usage example

```cpp
#include "SMLWebSocketClient.h"

// Create (usually in your subsystem's Initialize())
WebSocket = USMLWebSocketClient::CreateWebSocketClient(this);
WebSocket->bAutoReconnect = true;
WebSocket->ReconnectInitialDelaySeconds = 2.0f;
WebSocket->MaxReconnectDelaySeconds = 30.0f;

// Bind delegates
WebSocket->OnConnected.AddDynamic(this, &UMySubsystem::HandleConnected);
WebSocket->OnMessage.AddDynamic(this, &UMySubsystem::HandleMessage);
WebSocket->OnClosed.AddDynamic(this, &UMySubsystem::HandleClosed);
WebSocket->OnError.AddDynamic(this, &UMySubsystem::HandleError);

// Connect to the Discord Gateway (wss://)
TArray<FString> Protocols;
TMap<FString, FString> Headers;
Headers.Add(TEXT("Authorization"), TEXT("Bot ") + BotToken);
WebSocket->Connect(TEXT("wss://gateway.discord.gg/?v=10&encoding=json"), Protocols, Headers);

// Send a JSON text message
WebSocket->SendText(TEXT("{\"op\":2,\"d\":{\"token\":\"Bot ...\", ...}}"));

// Graceful shutdown (in Deinitialize())
if (WebSocket)
{
    WebSocket->Close(1000, TEXT("Server shutting down"));
    WebSocket = nullptr;
}
```

---

## Blueprint usage example

1. Call **CreateWebSocketClient** (static function) to create an instance.
2. Set `bAutoReconnect`, `ReconnectInitialDelaySeconds`, `MaxReconnectDelaySeconds` on the returned object.
3. Bind your event handlers to `OnConnected`, `OnMessage`, `OnClosed`, `OnError`, `OnReconnecting`.
4. Call **Connect** with your `ws://` or `wss://` URL, optional sub-protocols, and optional headers.
5. Call **SendText** / **SendBinary** to exchange messages after `OnConnected` fires.
6. Call **Close** when done.

---

## Adding SMLWebSocket as a dependency

### `.uplugin`

```json
{
  "Name": "SMLWebSocket",
  "Enabled": true,
  "SemVersion": "^1.0.0"
}
```

### `Build.cs`

```csharp
PublicDependencyModuleNames.AddRange(new string[] { "SMLWebSocket" });
```

---

## Build targets

| Target | SSL support |
|--------|------------|
| `Win64` (Windows dedicated server) | ✅ OpenSSL |
| `Linux` (Linux dedicated server) | ✅ OpenSSL |

SSL/OpenSSL dependencies are only compiled in for the two supported dedicated-server platforms.

---

## Dependencies

| Dependency | Version | Purpose |
|------------|---------|---------|
| SML | `^3.11.3` | Module load ordering |

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
