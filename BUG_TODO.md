# Bug TODO List

All 4 owner mods (BanChatCommands, BanSystem, DiscordBridge, SMLWebSocket) were scanned for bugs.
Bugs are listed from most critical to least critical within each mod.
None of these are fixed yet — this list tracks what needs to be addressed.

---

## BanChatCommands

### 🔴 High — Integer overflow in duration multiplication
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp:717-720`

Each individual `Num` digit-string is guarded against `int64` overflow, but the *multiplication*
`Num * 10080` (weeks), `Num * 1440` (days), `Num * 60` (hours), and the *accumulation* into
`Total` are not checked. A valid per-unit `Num` that is large enough can still overflow `int64`
after multiplication, or `Total` can overflow when multiple tokens are summed.

**Fix:** Before each `Total +=`, verify that the multiplication and addition don't exceed
`INT64_MAX`. Return `-1` on overflow.

---

### 🔴 High — Bare-integer duration silently truncated to int32
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp:691-692`

When `DurationStr` is a bare number (legacy "minutes only" format), the function calls
`FCString::Atoi()` which returns `int32`. Any value larger than `INT32_MAX` (~2 billion minutes)
silently wraps, and no error is returned. The suffix-based path below this already guards against
this via the `INT32_MAX` cap on line 728, but the bare-integer path does not.

**Fix:** Replace `FCString::Atoi` with `FCString::Atoi64`, validate the result is positive and
`<= INT32_MAX`, and return `-1` otherwise.

---

### 🟡 Medium — `SaveToFile()` return value silently ignored in `AddNote`/`RemoveNote`
**Files:**
- `Mods/BanChatCommands/Source/BanChatCommands/Private/PlayerNoteRegistry.cpp:63` (`AddNote`)
- `Mods/BanChatCommands/Source/BanChatCommands/Private/PlayerNoteRegistry.cpp:93` (`RemoveNote`)
- `Mods/BanChatCommands/Source/BanChatCommands/Private/MuteRegistry.cpp:80` (`MutePlayer`)
- `Mods/BanChatCommands/Source/BanChatCommands/Private/MuteRegistry.cpp:167` (`LoadFromFile` — prune path)
- `Mods/BanChatCommands/Source/BanChatCommands/Private/MuteRegistry.cpp:283` (`LoadFromFile` — prune path)

The in-memory state is modified successfully but if `SaveToFile()` returns `false` (disk full,
permission error, etc.) the failure is silently discarded. On the next restart the mod will reload
the old file, losing the change that was just made.

**Fix:** Log an error when `SaveToFile()` returns `false` in these call sites (matching the pattern
already used in `MutePlayer`'s `SaveToFile` error log).

---

### 🟡 Medium — `FDateTime::ParseIso8601()` return value ignored
**Files:**
- `Mods/BanChatCommands/Source/BanChatCommands/Private/MuteRegistry.cpp:258,260`
- `Mods/BanChatCommands/Source/BanChatCommands/Private/PlayerNoteRegistry.cpp:144`

`ParseIso8601` returns `bool`. When the stored date string is malformed the parse silently fails
and the entry's `MuteDate`/`ExpireDate`/`NoteDate` field is left at its default value
(`FDateTime(0)` = year 0001). A mute entry with an uninitialized `ExpireDate` could appear
permanently expired or permanently active depending on `IsExpired()` logic.

**Fix:** Check the return value; log a warning and `continue` (skip the entry) if parsing fails.

---

### 🟢 Low — `NextId` counter has no overflow guard in `PlayerNoteRegistry`
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/PlayerNoteRegistry.cpp:55`

`Entry.Id = NextId++` increments an `int64` counter with no overflow check. In practice an
`int64` counter on a game server will never reach `INT64_MAX`, but a wrap-around would cause
duplicate IDs if notes were persisted across the wrap, corrupting the note database.

**Fix:** Add `if (NextId == INT64_MAX) { /* log error, return false */ }` guard in `AddNote()`.

---

## BanSystem

### 🔴 High — 19-digit ID validation allows `int64` overflow in `FCString::Atoi64`
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanRestApi.cpp:1180` (and similar checks at
lines 616, 1908, 2084, 2390, 2585)

The guard `IdParam->Len() > 19` rejects strings longer than 19 characters but accepts any
19-digit string. The maximum valid `int64` is `9223372036854775807` (19 digits starting with `9`).
A string like `"9999999999999999999"` has 19 digits and passes the check, but is greater than
`INT64_MAX`, so `FCString::Atoi64` has undefined behavior on overflow (returns garbage or wraps).

**Fix:** After verifying `Len() <= 19`, additionally compare the string lexicographically against
`"9223372036854775807"` when the length equals 19, or use a safe parse helper that detects
overflow.

---

### 🔴 High — Accumulated-seconds ticker resets to `0.0f` instead of subtracting the interval
**Files:**
- `Mods/BanSystem/Source/BanSystem/Private/ScheduledBanRegistry.cpp:107`
- `Mods/BanSystem/Source/BanSystem/Private/BanSystemModule.cpp:598, 642, 689`

All four ticker accumulator blocks do:
```cpp
AccumulatedSeconds += DeltaTime;
if (AccumulatedSeconds < TickIntervalSeconds) return;
AccumulatedSeconds = 0.0f;   // ← BUG: drops the overshoot
```
If a frame spike causes `AccumulatedSeconds` to overshoot the interval (e.g., by 0.5 s), that
0.5 s of accumulated time is thrown away. This causes the next tick to fire 0.5 s later than it
should, meaning scheduled bans, backups, and prune operations all drift over time.

**Fix:** Replace `AccumulatedSeconds = 0.0f;` with `AccumulatedSeconds -= TickIntervalSeconds;`
at each of the four locations.

---

### 🟡 Medium — `FDateTime::ParseIso8601()` return value ignored in `PlayerWarningRegistry`
**File:** `Mods/BanSystem/Source/BanSystem/Private/PlayerWarningRegistry.cpp:289, 294`

Same class of bug as in BanChatCommands. A malformed `warnDate` or `expireDate` in the JSON file
silently leaves `Entry.WarnDate` / `Entry.ExpireDate` at `FDateTime(0)`, which can cause
warnings to appear either instantly expired or far in the future.

**Fix:** Check the return value; log a warning and skip the entry if parsing fails.

---

### 🟡 Medium — POST /warnings response may return the wrong warning entry under concurrent load
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanRestApi.cpp:1051-1054`

```cpp
WarnReg->AddWarning(NewWarnEntry);
TArray<FWarningEntry> AllForUid = WarnReg->GetWarningsForUid(Uid);
const FWarningEntry NewEntry = AllForUid.Num() > 0 ? AllForUid.Last() : FWarningEntry();
```
`AddWarning` and `GetWarningsForUid` are separate lock acquisitions. If a second request adds
another warning for the same UID between these two calls, `AllForUid.Last()` returns the second
warning, and the HTTP response for the first request contains the wrong entry.

**Fix:** Have `AddWarning` return the new `FWarningEntry` (including the assigned ID) directly, so
the caller does not need a second lookup.

---

### 🟢 Low — `BanDatabase::JsonToEntry` returns `true` based solely on a non-empty `Uid` field
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanDatabase.cpp` — `JsonToEntry`

The parse-success indicator is `!OutEntry.Uid.IsEmpty()`. Any JSON object that happens to have a
non-empty `uid` string is considered successfully parsed, even if every other required field
failed to parse. A partially-parsed entry could be inserted into the database with default/zero
values for `BanDate`, `BannedBy`, etc.

**Fix:** Validate that at minimum `uid`, `banDate`, and `bannedBy` are non-empty before returning
`true`.

---

## DiscordBridge

### 🟡 Medium — `DiscordBridgeConfig.cpp` — `Mid()` called without verifying `Start < Cleaned.Len()`
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/DiscordBridgeConfig.cpp` — multiple
string-parsing helpers (lines ~417, ~514, ~530, ~541)

Several `Cleaned.Mid(Start, End - Start)` calls compute `Start` as `Idx + Search.Len()`. If the
search pattern appears at the very end of the string, `Start` could equal or exceed
`Cleaned.Len()`, and `End` (found via another `Find`) could equal `INDEX_NONE` or be less than
`Start`. While `FString::Mid` is defined to return an empty string in these edge cases (not a
crash), it silently swallows a malformed config line that should be reported as an error.

**Fix:** After computing `Start`, add `if (Start >= Cleaned.Len() || End <= Start) continue;`
before calling `Mid`, and log a config-parse warning.

---

### 🟢 Low — `WhitelistManager::LogAudit` undocumented lock requirement
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/WhitelistManager.cpp:364`

`LogAudit()` is called from several functions that already hold `Mutex`, but this requirement is
not documented in the function signature or a comment. Any future caller that does not hold the
lock first will have a data race on `AuditLog`.

**Fix:** Add a comment `// Caller must already hold Mutex.` to the declaration, or convert to an
internal helper with a clearer naming convention (e.g., `LogAudit_Locked`).

---

## SMLWebSocket

### 🟡 Medium — RFC 6455 violation: no Close frame sent on fragment-sequencing error
**File:** `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketRunnable.cpp` —
`ProcessIncomingFrame` (fragment state check, around line 1361)

When a new data frame arrives while `FragmentBuffer` is non-empty (i.e., the remote peer sent an
unfragmented frame in the middle of a fragmented message), the function clears `FragmentBuffer`
and returns `false`, which closes the TCP connection. RFC 6455 §5.4 requires that the endpoint
first send a Close frame with status code 1002 (Protocol Error) before closing. Abruptly dropping
the socket without a Close frame is a protocol violation that may confuse the remote peer.

**Fix:** Before returning `false`, call the Close-frame sender with status code 1002.

---

### 🟡 Medium — Reserved WebSocket opcodes (0xB–0xF) silently ignored instead of closing
**File:** `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketRunnable.cpp` — opcode
`switch` statement (around line 1331)

RFC 6455 §5.2 states that reserved opcodes must cause the receiving endpoint to fail the
WebSocket connection. The current `default:` branch does not send a Close frame or fail; it just
logs a warning and continues processing. This is a protocol non-compliance issue.

**Fix:** In the `default:` branch, send a Close frame with status 1002 and return `false` to
close the connection.

---

### 🟡 Medium — `EnqueueText`/`EnqueueBinary` read `bConnected` without the outbound queue lock
**File:** `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketRunnable.cpp:462-479`

`bConnected` is a `FThreadSafeBool` (atomic), so reading it is safe. However, the
check-then-enqueue sequence is not atomic: the connection can be lost between the `if (!bConnected)`
check and the `OutboundMessages.Enqueue(...)` call. The message is then enqueued to a dead queue
and silently dropped when `CleanupConnection` clears it. In the current design this is acceptable
behaviour (the client will re-send on reconnect if needed), but it is undocumented.

**Fix (documentation):** Add a comment explaining that a message enqueued just before disconnect
may be silently lost, and that callers should use the `bQueueMessagesWhileDisconnected` option if
delivery is required.

---

### 🟢 Low — SHA-1 hash return values not checked in WebSocket server handshake
**File:** `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketServerRunnable.cpp:347-352`

`Sha.Update()` and `Sha.Final()` return `bool` but the return values are discarded. If the SHA-1
implementation reports an error the handshake accept key is computed from uninitialised memory,
causing every incoming client connection to fail with a cryptic "invalid Sec-WebSocket-Accept"
error and no logged reason.

**Fix:** Check both return values; log an error and reject the handshake if either fails.

---

*Last updated: 2026-04-26. All bugs above are unresolved.*
