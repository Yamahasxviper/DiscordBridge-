# SMLWebSocket – Release Notes

## v1.0.0 – Initial Release

*Compatible with Satisfactory build ≥ 416835 and SML ≥ 3.11.3*

---

### Overview

SMLWebSocket is a **standalone server-only C++ mod** that provides a custom
WebSocket client with SSL/OpenSSL support for Satisfactory dedicated server mods
built with Alpakit.

Satisfactory uses a **custom Coffee Stain Studios Unreal Engine build** that
removes several engine modules not used by the game, including `WebSockets`.
SMLWebSocket fills this gap by implementing the full RFC 6455 WebSocket protocol
directly over Unreal's low-level TCP socket layer (`FSocket` / `ISocketSubsystem`),
with optional TLS encryption via OpenSSL on Win64 and Linux server targets.

Any mod that needs a persistent WebSocket connection (such as DiscordBridge or
TicketSystem in standalone mode) lists SMLWebSocket as a dependency.

---

### Features

| Feature | Details |
|---------|---------|
| `ws://` and `wss://` | Plain TCP and TLS (OpenSSL) connections |
| RFC 6455 framing | Text frames, binary frames, ping/pong, close handshake |
| Auto-reconnect | Exponential back-off with configurable initial delay, max delay, and max attempts |
| Blueprint API | Full Blueprint exposure — `CreateWebSocketClient`, `Connect`, `SendText`, `SendBinary`, `Close`, `IsConnected` |
| C++ API | Bind delegates directly; all callbacks fire on the game thread |
| Thread-safe callbacks | `OnConnected`, `OnMessage`, `OnBinaryMessage`, `OnClosed`, `OnError`, `OnReconnecting` all dispatched on the game thread |
| Alpakit-compatible | No dependency on engine modules absent from Satisfactory's custom UE build |

---

### Build targets

| Target | SSL support |
|--------|------------|
| `Win64` (Windows dedicated server) | ✅ OpenSSL |
| `Linux` (Linux dedicated server) | ✅ OpenSSL |

---

### Requirements

| Dependency | Minimum version |
|------------|----------------|
| Satisfactory (dedicated server) | build ≥ 416835 |
| SML | ≥ 3.11.3 |

---

### Getting Started

See the [SMLWebSocket README](README.md) for C++ and Blueprint usage examples and
instructions on adding SMLWebSocket as a dependency to your own mod.

---

*For further help visit the Satisfactory Modding Discord: <https://discord.gg/xkVJ73E>*
