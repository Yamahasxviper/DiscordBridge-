# Bug TODO List

All 4 owner mods (BanChatCommands, BanSystem, DiscordBridge, SMLWebSocket) were scanned for bugs.
All bugs listed below have been fixed.

---

## BanChatCommands

### ✅ Fixed — Integer overflow in duration multiplication
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp`

**Fix applied:** Each `Total += Num * multiplier` is now preceded by overflow guards:
`if (Num > INT64_MAX / multiplier) return -1;` and `if (Total > INT64_MAX - Product) return -1;`.

---

### ✅ Fixed — Bare-integer duration silently truncated to int32
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp`

**Fix applied:** Replaced `FCString::Atoi` with `FCString::Atoi64`. Added validation that the
result is positive and `<= INT32_MAX`; returns `-1` otherwise.

---

### ✅ Fixed — `SaveToFile()` return value silently ignored
**Files:** `PlayerNoteRegistry.cpp` / `MuteRegistry.cpp`

**Fix applied:** All call sites now check the `bool` return value and emit a `UE_LOG(Error, ...)`
when a disk write fails.

---

### ✅ Fixed — `FDateTime::ParseIso8601()` return value ignored
**Files:** `MuteRegistry.cpp` / `PlayerNoteRegistry.cpp`

**Fix applied:** Each `ParseIso8601` call now checks the return; on failure it logs a warning and
`continue`s, skipping the malformed entry.

---

### ✅ Fixed — `NextId` counter has no overflow guard in `PlayerNoteRegistry`
**File:** `PlayerNoteRegistry.cpp`

**Fix applied:** `AddNote()` now checks `if (NextId == INT64_MAX)` before incrementing, logs an
error, and returns early.

---

### ✅ Fixed — `/clearwarn` missing int64 overflow guard before `FCString::Atoi64`
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp`

**Fix applied:** Added the same length + INT64_MAX lexicographic guard that `BanRestApi`'s
`ParseInt64Param` helper uses: rejects strings longer than 19 digits, and rejects 19-digit strings
whose decimal value exceeds `"9223372036854775807"`.

---

## BanSystem

### ✅ Fixed — 19-digit ID validation allows `int64` overflow in `FCString::Atoi64`
**File:** `BanRestApi.cpp` (6 sites)

**Fix applied:** Added `BanJson::ParseInt64Param()` helper that rejects 19-digit strings whose
decimal value exceeds `"9223372036854775807"` (INT64_MAX) via lexicographic comparison. All 6
validation + Atoi64 sites now use this helper.

---

### ✅ Fixed — Accumulated-seconds ticker resets to `0.0f` instead of subtracting the interval
**Files:** `ScheduledBanRegistry.cpp` / `BanSystemModule.cpp` (4 sites)

**Fix applied:** Replaced `AccumulatedSeconds = 0.0f` (and variants) with
`AccumulatedSeconds -= IntervalSeconds` at all four locations, preserving overshoot time.

---

### ✅ Fixed — `FDateTime::ParseIso8601()` return value ignored in `PlayerWarningRegistry`
**File:** `PlayerWarningRegistry.cpp`

**Fix applied:** Same pattern as MuteRegistry — check return, log warning, skip entry on failure.

---

### ✅ Fixed — POST /warnings response may return the wrong warning entry under concurrent load
**Files:** `BanRestApi.cpp` / `PlayerWarningRegistry.h` / `PlayerWarningRegistry.cpp`

**Fix applied:** `AddWarning(const FWarningEntry&)` now returns the populated `FWarningEntry`
(with assigned Id and WarnDate) from within the same lock acquisition. The REST handler uses this
return value directly, eliminating the separate `GetWarningsForUid` call.

---

### ✅ Fixed — `BanDatabase::JsonToEntry` returns `true` based solely on a non-empty `Uid` field
**File:** `BanDatabase.cpp`

**Fix applied:** Success condition is now `!OutEntry.Uid.IsEmpty() && !OutEntry.BannedBy.IsEmpty()`.

---

## DiscordBridge

### ✅ Fixed — `DiscordBridgeConfig.cpp` — `Mid()` called without verifying end indices
**File:** `DiscordBridgeConfig.cpp` (ScheduledAnnouncement Message and ChannelId blocks)

**Fix applied:** Added `End > Start` guard alongside the existing `End != INDEX_NONE` check.
When the condition fails, a `UE_LOG(Warning, ...)` is emitted to flag the malformed config line.

---

### ✅ Fixed — `WhitelistManager::LogAudit` undocumented lock requirement
**File:** `WhitelistManager.h`

**Fix applied:** Added `/** Caller must already hold Mutex. */` doc comment to the declaration.
The matching comment already existed in the `.cpp` implementation.

---

### ✅ Fixed — `AnnouncementTick` timer drift: accumulated seconds reset to `0.0f`
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/DiscordBridgeSubsystem.cpp`

**Fix applied:** Changed `AnnouncementAccumulatedSeconds = 0.0f` to
`AnnouncementAccumulatedSeconds -= IntervalSeconds`, matching the identical fix already applied
to `BackupAccumulatedSeconds`, `PruneAccumulatedSeconds`, and `SessionPruneAccumulatedSeconds`
in `BanSystemModule.cpp`.

---

### ✅ Fixed — `SaveTicketState`: `FJsonSerializer::Serialize` return value not checked
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** Wrapped the `Serialize` call in `if (!...)` and added an early `return` with an
`UE_LOG(Error, ...)`. Prevents a serialization failure from writing a corrupt/empty string to
`ActiveTickets.json.tmp` and losing all open ticket state on the next atomic rename.

---

### ✅ Fixed — `SaveTicketBlacklist` writes directly (no atomic tmp→rename)
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** Added `FJsonSerializer::Serialize` return check (early return on failure) and
converted the direct `SaveStringToFile(*Path)` to the same write-to-`.tmp`-then-`MoveFile`
atomic pattern used by `SaveTicketState` and every other registry in the codebase.

---

### ✅ Fixed — Ticket-feedback stats written without `FJsonSerializer::Serialize` check
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** The `FJsonSerializer::Serialize` call is now checked; the directory creation and
file write are gated inside `if (Serialize(...))` so a serialization failure no longer writes
empty content to the stats file.

---

## SMLWebSocket

### ✅ Fixed — RFC 6455 violation: no Close frame sent on fragment-sequencing error
**File:** `SMLWebSocketRunnable.cpp`

**Fix applied:** Both the mid-fragment interleave path and the fragment-size overflow path now
call `SendWsFrame(WsOpcode::Close, {0x03, 0xEA}, 2)` (status 1002) before returning `false`.

---

### ✅ Fixed — Reserved WebSocket opcodes (0xB–0xF) silently ignored instead of closing
**File:** `SMLWebSocketRunnable.cpp`

**Fix applied:** The `default:` branch now sends Close(1002) and returns `false`, satisfying
RFC 6455 §5.2.

---

### ✅ Fixed (documentation) — `EnqueueText`/`EnqueueBinary` TOCTOU gap
**File:** `SMLWebSocketRunnable.cpp`

**Fix applied:** Added a comment above `EnqueueText` explaining that a message enqueued between
the `bConnected` check and `OutboundMessages.Enqueue` may be silently dropped on disconnect,
and directing callers to `bQueueMessagesWhileDisconnected` for guaranteed delivery.

---

*Last updated: 2026-04-27. All bugs resolved.*
