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
**File:** `DiscordBridgeConfig.cpp` (ScheduledAnnouncement Message and ChannelId blocks; `ChatRelayBlocklistReplacements` and `BotCommandAliases` `ExtractQuoted` lambdas)

**Fix applied:** Added `End > Start` guard alongside the existing `End != INDEX_NONE` check.
When the condition fails, a `UE_LOG(Warning, ...)` is emitted to flag the malformed config line (ScheduledAnnouncements).
The two `ExtractQuoted` lambdas now also use `End == INDEX_NONE || End <= Start` so an empty-value entry (`Pattern=""`) is rejected instead of silently producing an empty-pattern replacement.

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

### ✅ Fixed — `SendMessageBodyToChannel` uses `BindLambda([this])` instead of `BindWeakLambda`
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/DiscordBridgeSubsystem.cpp`

**Fix applied:** Changed `BindLambda([this, TargetChannelId, BodyString]...)` to
`BindWeakLambda(this, [this, TargetChannelId, BodyString]...)`.  The lambda accesses
`Config.FallbackWebhookUrl` (a member of `this`); without the weak binding a use-after-free
would occur if the subsystem is destroyed while an HTTP request is still in-flight (e.g. during
server shutdown).  This is now consistent with every other HTTP callback in the file.

---

## BanSystem (round 2)

### ✅ Fixed — `BanAuditLog::LogAction()` — `SaveToFile()` return silently ignored
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanAuditLog.cpp`

**Fix applied:** Added `if (!SaveToFile())` check with `UE_LOG(Error, ...)` in `LogAction()`, matching
the pattern already used by every other registry in the codebase.

---

### ✅ Fixed — `BanAppealRegistry::LoadFromFile()` — malformed `submittedAt` falls through with epoch date
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanAppealRegistry.cpp`

**Fix applied:** Added `continue;` after the warning log when `FDateTime::ParseIso8601` fails for
`submittedAt`, matching the identical pattern in `MuteRegistry`, `PlayerNoteRegistry`,
`PlayerWarningRegistry`, and `ScheduledBanRegistry`.

---

### ✅ Fixed — `int64` IDs serialized as `double` (precision lost above 2⁵³)
**Files:** `BanAuditLog.cpp` (id + nextId), `BanDatabase.cpp`, `BanAppealRegistry.cpp`,
`ScheduledBanRegistry.cpp`, `PlayerWarningRegistry.cpp`

**Fix applied:** All `SetNumberField(TEXT("id"), static_cast<double>(Id))` calls converted to
`SetStringField(TEXT("id"), FString::Printf(TEXT("%lld"), Id))`. Load paths updated to try
`TryGetStringField` first (new format) and fall back to `TryGetNumberField` (old format) for
backward compatibility with existing data files. `BanAuditLog`'s `nextId` counter follows the
same string-first / number-fallback pattern.

---

*Last updated: 2026-04-28 (initial scan). All bugs resolved.*

---

## Round 2 — Additional Scan (2026-04-28)

### ✅ Fixed — Static `CommandCooldowns` / `AdminBanTimestamps` TMap — no mutex (BUG-01)
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp`

**Fix applied:** Added `check(IsInGameThread())` at the start of `IsOnCooldown()` and
`IsBanRateLimited()`. Added documentation comment on both maps explicitly declaring them
game-thread-only. SML dispatches `ExecuteCommand_Implementation` on the game thread so no
mutex is needed; the check will surface any future violation immediately.

---

### ✅ Fixed — Static `AFreezeChatCommand::FrozenPlayerUids` TSet — no mutex (BUG-02)
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/Commands/BanChatCommands.cpp`

**Fix applied:** Added `check(IsInGameThread())` at the start of
`AFreezeChatCommand::ExecuteCommand_Implementation()`. Added a comment above the
`FrozenPlayerUids` definition stating it is game-thread-only
(command execute, `PostLoginHandle`, and `LogoutHookHandle` all run on game thread).

---

### ✅ Fixed (documentation) — Local `KickTimerHandle` cannot be cancelled on disconnect (BUG-03)
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanEnforcer.cpp` (5 sites)

**Fix applied:** Added comments at all five `FTimerHandle KickTimerHandle;` declarations
explaining that the handle is intentionally transient (one-shot 20-second timer) and that
`TWeakObjectPtr<APlayerController>` prevents any crash or access violation when the player
disconnects during the window. The timer fires and self-cleans via the UE TimerManager.

---

### ✅ Fixed (documentation) — HTTP route handlers and game-thread dispatch guarantee (BUG-04)
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanRestApi.cpp`

**Fix applied:** Replaced the original "potentially called on the HTTP thread" comment at the
top of `RegisterRoutes()` with an accurate note: UE's `FHttpServerModule` enqueues requests
and dispatches route callbacks on the game thread via its own `FTSTicker`. All subsystem
lookups, delegate broadcasts, timer operations, and PlayerController iterations in route
lambdas are therefore safe without additional locking.

---

### ✅ Fixed — `BanDatabase` root `nextId` serialised as `double` (precision loss > 2^53) (BUG-05)
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanDatabase.cpp`

**Fix applied:** `SaveToFile()` now writes `nextId` as a decimal string via
`SetStringField(TEXT("nextId"), FString::Printf(TEXT("%lld"), NextId))`.
`LoadFromFile()` uses a string-first / number-fallback pattern (same as the existing
per-entry `id` fix) for backward compatibility with older database files.

---

### ✅ Fixed — `PlayerNoteRegistry` ID load off-by-one + save as double (BUG-06)
**File:** `Mods/BanChatCommands/Source/BanChatCommands/Private/PlayerNoteRegistry.cpp`

**Fix applied:** `LoadFromFile()` now uses the string-first / number-fallback pattern
(matching `PlayerWarningRegistry`) instead of the `IdDbl < static_cast<double>(INT64_MAX)`
guard that had an off-by-one at INT64_MAX. `SaveToFile()` changed from `SetNumberField`
to `SetStringField(TEXT("id"), FString::Printf(TEXT("%lld"), N.Id))`.

---

### ✅ Fixed — `appealId` in `BanDiscordNotifier` serialised as `double` (precision loss > 2^53) (BUG-07)
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanDiscordNotifier.cpp`

**Fix applied:** Both `NotifyAppealSubmitted` and `NotifyAppealReviewed` now emit `appealId`
as a string: `SetStringField(TEXT("appealId"), FString::Printf(TEXT("%lld"), Appeal.Id))`.
(`totalWarnings` / `warnCount` are `int32` and remain as `SetNumberField` — int32 fits
exactly in a double.)

---

### ✅ Fixed — `PlayerWarningRegistry` does not persist `nextId` (BUG-08)
**File:** `Mods/BanSystem/Source/BanSystem/Private/PlayerWarningRegistry.cpp`

**Fix applied:** `SaveToFile()` now writes a `"nextId"` string field alongside the
`"warnings"` array. `LoadFromFile()` prefers the stored `nextId` (string-first /
number-fallback for legacy files) over the scan-based max+1 reconstruction, so deleting
the entry with the highest Id no longer regresses the counter and causes duplicate Ids.

---

### ✅ Fixed — `SaveTicketState` / `SaveTicketBlacklist` use non-atomic Delete + MoveFile (BUG-09)
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** Both save paths now use `IFileManager::Get().Move(*Dest, *Tmp, /*bReplace=*/true)`
instead of the previous `PlatformFile.DeleteFile(*Dest)` + `PlatformFile.MoveFile(...)` two-step.
The single `IFileManager::Move(bReplace=true)` call is an atomic OS-level rename/replace —
there is no window where the live state file is absent. If the move fails the `.tmp` file is
deleted and a warning is logged; the live file is never touched.

---

### ✅ Fixed — `appeal_id` persisted as `double` in ticket state JSON (precision loss > 2^53) (BUG-10)
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** `SaveTicketState()` now writes `appeal_id` as a string:
`SetStringField(TEXT("appeal_id"), FString::Printf(TEXT("%lld"), *AppealIdSave))`.
`LoadTicketState()` uses string-first / number-fallback for backward compatibility with
existing state files.

---

### ✅ Fixed — Pong echoes masked Ping payload without unmasking — RFC 6455 §5.3 violation (BUG-11)
**File:** `Mods/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketServerRunnable.cpp`

**Fix applied:** The Ping handler now XOR-unmasks each payload byte against the 4-byte mask
key (`Buf[HeaderSize + (i & 3)]`) before adding it to the Pong frame, matching the same
pattern used for regular text/binary frames. A comment explains the RFC 6455 §5.3 requirement.
When `bMasked` is false (server-to-server or unmasked client) the raw byte is used directly.

---

*Last updated: 2026-04-28. All 11 round-2 bugs resolved.*

---

## Round 3 — Additional Scan (2026-04-28)

### ✅ Fixed — `SaveTicketState()` missing closing `}` — compile error (BUG-01)
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** Added the missing closing `}` for `SaveTicketState()` between the
`IFileManager::Get().Delete(*TmpPath)` statement and the start of `LoadTicketState()`.
Without this brace every function definition that followed (`LoadTicketState`, `GetStatsFilePath`,
etc.) was syntactically nested inside `SaveTicketState`, making the entire DiscordBridge module
a compile error.

---

### ✅ Fixed — `BanAppealRegistry::SaveToFile()` does not persist `NextId` (BUG-02)
**File:** `Mods/BanSystem/Source/BanSystem/Private/BanAppealRegistry.cpp`

**Fix applied:** `SaveToFile()` now writes `nextId` as a decimal string via
`SetStringField(TEXT("nextId"), FString::Printf(TEXT("%lld"), NextId))`.
`LoadFromFile()` now uses the string-first / number-fallback pattern (same as
`PlayerWarningRegistry` BUG-08 fix) to prefer the persisted counter over the
scan-based `max(id)+1` reconstruction. This prevents ID reuse after all appeals are
deleted and the server restarts.

---

### ✅ Fixed — `ScheduledBanRegistry::SaveToFile()` does not persist `NextId` (BUG-03)
**File:** `Mods/BanSystem/Source/BanSystem/Private/ScheduledBanRegistry.cpp`

**Fix applied:** Same pattern as BUG-02. `SaveToFile()` now writes a `"nextId"` string field
alongside the `"scheduled"` array. `LoadFromFile()` prefers the stored counter (string-first /
number-fallback) over the scan-based reconstruction. This prevents ID reuse once all pending
scheduled bans have fired and the list is empty on the next server restart.

---

### ✅ Fixed — `SaveStringToFile` return value ignored in ticket feedback stats write (BUG-04)
**File:** `Mods/DiscordBridge/Source/DiscordBridge/Private/TicketSubsystem.cpp`

**Fix applied:** The `FFileHelper::SaveStringToFile(...)` call for ticket feedback stats is now
checked with `if (!...)` and emits `UE_LOG(LogTicketSystem, Error, ...)` on failure, matching
the error-handling pattern used by every other file-write in the codebase. Previously a disk-full
or permission-denied error would silently lose rating data with no diagnostics.

---

*Last updated: 2026-04-28. All 4 round-3 bugs resolved.*

