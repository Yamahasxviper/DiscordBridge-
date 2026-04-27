#!/usr/bin/env bash
# =============================================================================
# Scripts/audit_bugs.sh
# Exhaustive mechanical bug-pattern audit for all 4 owner mods.
#
# Usage:
#   cd <repo-root>
#   bash Scripts/audit_bugs.sh
#
# Exit code: 0 = no issues found, 1 = one or more issues found.
#
# Each check is applied to EVERY source file in all 4 mods so that a pattern
# fixed in one file is automatically verified across the entire codebase.
# This prevents the class of miss where a bug is caught in the first file
# encountered but identical code in other files goes unnoticed.
# =============================================================================

REPO_ROOT="$(cd "$(dirname "$0")/.." && pwd)"
MOD_ROOT="$REPO_ROOT/Mods"

# Directories that contain owner-authored code (excludes SML/Alpakit/etc.)
OWNER_MODS=(
    "$MOD_ROOT/BanChatCommands/Source"
    "$MOD_ROOT/BanSystem/Source"
    "$MOD_ROOT/DiscordBridge/Source"
    "$MOD_ROOT/SMLWebSocket/Source"
)

RED='\033[0;31m'
YEL='\033[0;33m'
GRN='\033[0;32m'
NC='\033[0m'

ISSUES=0

# Helper: print a finding and increment the counter.
fail() {
    local check="$1"; shift
    echo -e "${RED}[FAIL]${NC} ${YEL}${check}${NC}"
    while [[ $# -gt 0 ]]; do
        echo "       $1"; shift
    done
    echo
    ISSUES=$((ISSUES + 1))
}

pass() {
    echo -e "${GRN}[PASS]${NC} $1"
}

# Build a single grep path list from all owner mod source directories.
MOD_PATHS=()
for d in "${OWNER_MODS[@]}"; do
    if [[ -d "$d" ]]; then
        MOD_PATHS+=("$d")
    fi
done

echo "========================================================"
echo " DiscordBridge bug-pattern audit"
echo " Scanning ${#MOD_PATHS[@]} mod source tree(s)"
echo "========================================================"
echo

# =============================================================================
# CHECK 1: INT64_MAX guard on NextId increment
#
# Pattern: any "NextId = <expr>.Id + 1" without the guard
# "(E.Id < INT64_MAX) ? E.Id + 1 : E.Id" (or equivalent).
#
# When a file has "NextId = X.Id + 1" but NOT the INT64_MAX guard string, it
# is a potential overflow.  We look for the plain increment pattern and then
# verify the guard is present in the same file.
# =============================================================================
echo "--- CHECK 1: INT64_MAX guard on all NextId increment loops ---"

while IFS= read -r -d '' file; do
    # Does this file contain an unguarded "= X.Id + 1" NextId assignment?
    if grep -qP '\bNextId\s*=\s*\(?\w+\.Id\s*\+\s*1' "$file"; then
        # Good only if the INT64_MAX guard is present somewhere in the file.
        if ! grep -qP 'INT64_MAX' "$file"; then
            fail "CHECK 1 – INT64_MAX guard" \
                "File: $file" \
                "Has 'NextId = X.Id + 1' but no INT64_MAX guard." \
                "Fix: wrap the increment:  NextId = (X.Id < INT64_MAX) ? X.Id + 1 : X.Id;"
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 1 done"
echo

# =============================================================================
# CHECK 2: Temp file cleanup on IFileManager::Move() failure
#
# Every SaveToFile() that does:
#   1. FFileHelper::SaveStringToFile(..., *TmpPath, ...)
#   2. IFileManager::Get().Move(*Dest, *TmpPath, ...)
# must also call IFileManager::Get().Delete(*TmpPath) in the failure branch.
#
# Strategy: find every file that calls both SaveStringToFile and Move.
# Then verify that Delete(*TmpPath) also appears (even if on the happy path
# the Move succeeds, the error branch must clean up).
# =============================================================================
echo "--- CHECK 2: Temp file cleanup after IFileManager::Move() failure ---"

while IFS= read -r -d '' file; do
    has_save=$(grep -cP 'SaveStringToFile' "$file")
    has_move=$(grep -cP 'IFileManager::Get\(\)\.Move' "$file")
    if [[ "$has_save" -gt 0 && "$has_move" -gt 0 ]]; then
        has_delete=$(grep -cP 'IFileManager::Get\(\)\.Delete' "$file")
        if [[ "$has_delete" -eq 0 ]]; then
            fail "CHECK 2 – temp file cleanup" \
                "File: $file" \
                "Uses SaveStringToFile + Move but has no IFileManager::Get().Delete() call." \
                "Fix: add IFileManager::Get().Delete(*TmpPath) in the Move failure branch."
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 2 done"
echo

# =============================================================================
# CHECK 3: FDateTime::ParseIso8601 return value checked
#
# Every call to FDateTime::ParseIso8601() must use its bool return value.
# Bare "FDateTime::ParseIso8601(...)" (without assigning or checking the bool)
# silently leaves the output FDateTime unchanged on malformed input.
# =============================================================================
echo "--- CHECK 3: FDateTime::ParseIso8601() return value not ignored ---"

while IFS= read -r -d '' file; do
    # A call is "unchecked" when ParseIso8601 appears at the start of a
    # statement (i.e., not assigned to a variable or used in a condition).
    # Pattern: line is like "    FDateTime::ParseIso8601(..." without if/=.
    if grep -qP '^\s*FDateTime::ParseIso8601\s*\(' "$file"; then
        fail "CHECK 3 – ParseIso8601 return ignored" \
            "File: $file" \
            "FDateTime::ParseIso8601() return value is not checked (call result discarded)." \
            "Fix: use 'if (!FDateTime::ParseIso8601(..., Out)) { /* handle error */ }'"
        grep -nP '^\s*FDateTime::ParseIso8601\s*\(' "$file" | while IFS= read -r ln; do
            echo "       Line: $ln"
        done
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 3 done"
echo

# =============================================================================
# CHECK 4: BIO_write / BIO_read return value checked for < 0
#
# OpenSSL BIO functions return -1 on error, not 0.  An equality check
# "Written != BytesRead" catches it numerically but the error branch should
# explicitly test for negative values first to avoid misleading log output.
# =============================================================================
echo "--- CHECK 4: BIO_write/BIO_read return values handled correctly ---"

while IFS= read -r -d '' file; do
    if grep -qP '\bBIO_write\b|\bBIO_read\b' "$file"; then
        # Warn if no "< 0" or "<0" check is present alongside BIO calls.
        if ! grep -qP '<\s*0' "$file"; then
            fail "CHECK 4 – BIO_write/BIO_read negative return" \
                "File: $file" \
                "Uses BIO_write or BIO_read but has no '< 0' error check." \
                "Fix: check 'if (Written < 0)' before '!= BytesRead' comparison."
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 4 done"
echo

# =============================================================================
# CHECK 5: REST API POST/PATCH/DELETE handlers have CheckBodySize
#
# Every HTTP handler lambda that reads the request body via BodyToString()
# must call BanJson::CheckBodySize(Req) first to return HTTP 413 rather
# than silently treating an oversized body as empty JSON.
# =============================================================================
echo "--- CHECK 5: CheckBodySize present in all body-reading REST handlers ---"

while IFS= read -r -d '' file; do
    if grep -qP 'BodyToString' "$file"; then
        body_count=$(grep -cP 'BodyToString' "$file")
        check_count=$(grep -cP 'CheckBodySize' "$file")
        # Each BodyToString site should have a corresponding CheckBodySize.
        # We use a simple count heuristic; if counts differ, flag for review.
        if [[ "$check_count" -lt "$body_count" ]]; then
            fail "CHECK 5 – CheckBodySize coverage" \
                "File: $file" \
                "BodyToString() called $body_count time(s) but CheckBodySize() only $check_count time(s)." \
                "Fix: add 'if (auto SizeErr = BanJson::CheckBodySize(Req)) { Done(MoveTemp(SizeErr)); return true; }' before each BodyToString call."
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 5 done"
echo

# =============================================================================
# CHECK 6: FCriticalSection / FScopeLock present in all registry classes
#          that have shared mutable state
#
# Any class with both a mutable TMap or TArray member AND a public method
# should protect that state with a mutex.  We check that every .h file
# declaring a TMap or TArray also declares an FCriticalSection member.
# =============================================================================
echo "--- CHECK 6: FCriticalSection present in all registry/subsystem headers ---"

while IFS= read -r -d '' file; do
    # Only scan headers that look like subsystems or registries.
    basename="$(basename "$file")"
    if [[ "$basename" =~ Registry|Subsystem|Database|Pusher|Enforcer|Log|Client ]]; then
        has_container=$(grep -cP '\bTMap\b|\bTArray\b|\bTSet\b' "$file")
        has_mutex=$(grep -cP '\bFCriticalSection\b|\bFRWLock\b' "$file")
        if [[ "$has_container" -gt 0 && "$has_mutex" -eq 0 ]]; then
            fail "CHECK 6 – missing FCriticalSection" \
                "File: $file" \
                "Declares TMap/TArray/TSet but has no FCriticalSection or FRWLock." \
                "Review whether this class is accessed from multiple threads; add a mutex if so."
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.h" -print0 2>/dev/null)

pass "CHECK 6 done"
echo

# =============================================================================
# CHECK 7: Double-fragment size check before MoveTemp (WebSocket server)
#
# In SMLWebSocketServerRunnable.cpp the oversized-fragment check must appear
# BEFORE the MoveTemp(Payload) that consumes the data.  If the check comes
# after MoveTemp, the payload is already destroyed when we try to reject it.
# =============================================================================
echo "--- CHECK 7: Fragment size check before MoveTemp in WebSocket server ---"

SERVER_RUNNABLE="$MOD_ROOT/SMLWebSocket/Source/SMLWebSocket/Private/SMLWebSocketServerRunnable.cpp"
if [[ -f "$SERVER_RUNNABLE" ]]; then
    # Get line numbers for the size check and the MoveTemp.
    size_line=$(grep -n 'MaxMessageBytes\|MaxFrameSize\|too large\|oversized\|MaxPayload' "$SERVER_RUNNABLE" | head -1 | cut -d: -f1)
    move_line=$(grep -n 'MoveTemp.*Payload\|FragmentBuffer.*MoveTemp' "$SERVER_RUNNABLE" | head -1 | cut -d: -f1)
    if [[ -n "$size_line" && -n "$move_line" ]]; then
        if [[ "$size_line" -gt "$move_line" ]]; then
            fail "CHECK 7 – size check after MoveTemp" \
                "File: $SERVER_RUNNABLE" \
                "Size check (line $size_line) comes AFTER MoveTemp (line $move_line)." \
                "Fix: move the size check above the MoveTemp(Payload) call."
        fi
    fi
fi

pass "CHECK 7 done"
echo

# =============================================================================
# CHECK 8: static_cast<int32> from float/double without range guard
#
# Casting an unbounded float/double to int32 is UB when the value is outside
# [INT32_MIN, INT32_MAX].  Every static_cast<int32>() applied to a float or
# the result of a floating-point function (GetIniFloat, etc.) must be wrapped
# in a clamping helper or explicit range check.
# =============================================================================
echo "--- CHECK 8: static_cast<int32> from float/double without range guard ---"

while IFS= read -r -d '' file; do
    # Look for direct static_cast<int32>(GetIniFloat or similar float source).
    if grep -qP 'static_cast<int32>\s*\(\s*(GetIniFloat|GetFloat|\.GetFloat|[0-9]+\.[0-9]+f?)' "$file"; then
        fail "CHECK 8 – unsafe float-to-int32 cast" \
            "File: $file" \
            "static_cast<int32> applied to a float/GetIniFloat result without range guard." \
            "Fix: use a helper like GetIniInt() that clamps before casting."
        grep -nP 'static_cast<int32>\s*\(\s*(GetIniFloat|GetFloat|\.GetFloat)' "$file" | while IFS= read -r ln; do
            echo "       Line: $ln"
        done
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 8 done"
echo

# =============================================================================
# CHECK 9: Integer overflow in duration arithmetic (ban/mute parsing)
#
# Duration parsers that do "Total += Num * multiplier" can overflow int64
# when user input is very large.  Each multiplication must be preceded by
# an INT64_MAX / multiplier guard.
# =============================================================================
echo "--- CHECK 9: Overflow guards in duration arithmetic ---"

while IFS= read -r -d '' file; do
    if grep -qP 'Total\s*\+=\s*\w+\s*\*\s*(multiplier|60|3600|86400|10080)' "$file" ||
       grep -qP 'Num\s*\*\s*(60|3600|86400|10080)\b' "$file"; then
        if ! grep -qP 'INT64_MAX\s*/\s*(multiplier|60|3600|86400|10080)' "$file"; then
            fail "CHECK 9 – duration multiplication overflow" \
                "File: $file" \
                "Duration arithmetic without INT64_MAX / multiplier overflow guard." \
                "Fix: add 'if (Num > INT64_MAX / multiplier) return -1;' before each multiply."
        fi
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 9 done"
echo

# =============================================================================
# CHECK 10: JSON parse root validated before use
#
# Every FJsonSerializer::Deserialize call must check both the bool return AND
# Root.IsValid() before accessing Root->...  A missing check leads to a null
# dereference when JSON input is malformed.
# =============================================================================
echo "--- CHECK 10: JSON parse results validated before use ---"

while IFS= read -r -d '' file; do
    # Count Deserialize calls vs. IsValid() checks in the same file.
    deser_count=$(grep -cP 'FJsonSerializer::Deserialize' "$file")
    valid_count=$(grep -cP '\.IsValid\(\)' "$file")
    if [[ "$deser_count" -gt 0 && "$valid_count" -lt "$deser_count" ]]; then
        fail "CHECK 10 – JSON parse not fully validated" \
            "File: $file" \
            "FJsonSerializer::Deserialize called $deser_count time(s) but IsValid() only $valid_count time(s)." \
            "Fix: every Deserialize call site must check '|| !Root.IsValid()'."
    fi
done < <(find "${MOD_PATHS[@]}" -name "*.cpp" -print0 2>/dev/null)

pass "CHECK 10 done"
echo

# =============================================================================
# SUMMARY
# =============================================================================
echo "========================================================"
if [[ "$ISSUES" -eq 0 ]]; then
    echo -e "${GRN}All checks passed — no issues found.${NC}"
    exit 0
else
    echo -e "${RED}${ISSUES} issue(s) found. Review the FAIL entries above.${NC}"
    exit 1
fi
