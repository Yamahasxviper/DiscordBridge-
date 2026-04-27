// Copyright Yamahasxviper. All Rights Reserved.
//
// Automation tests for pure-logic static helpers in UBanDiscordSubsystem.
//
// Covered functions (static — no subsystem lifecycle / game state required):
//   UBanDiscordSubsystem::ParseDurationMinutes()
//   UBanDiscordSubsystem::IsValidEOSPUID()
//   UBanDiscordSubsystem::IsValidIPQuery()

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"

#include "BanDiscordSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
//  ParseDurationMinutes
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FParseDurationMinutesTest,
    "DiscordBridge.BanDiscordSubsystem.ParseDurationMinutes",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FParseDurationMinutesTest::RunTest(const FString& Parameters)
{
    // ── Permanent / empty keywords → 0 ───────────────────────────────────────
    TestEqual(TEXT("empty string → 0"),   UBanDiscordSubsystem::ParseDurationMinutes(TEXT("")),         0);
    TestEqual(TEXT("perm → 0"),           UBanDiscordSubsystem::ParseDurationMinutes(TEXT("perm")),     0);
    TestEqual(TEXT("permanent → 0"),      UBanDiscordSubsystem::ParseDurationMinutes(TEXT("permanent")),0);
    TestEqual(TEXT("PERM → 0 (case)"),    UBanDiscordSubsystem::ParseDurationMinutes(TEXT("PERM")),     0);
    TestEqual(TEXT("Permanent → 0"),      UBanDiscordSubsystem::ParseDurationMinutes(TEXT("Permanent")),0);

    // ── Plain positive integers (bare minutes) ────────────────────────────────
    TestEqual(TEXT("\"30\" → 30"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("30")),   30);
    TestEqual(TEXT("\"60\" → 60"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("60")),   60);
    TestEqual(TEXT("\"1440\" → 1440"),    UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1440")), 1440);

    // ── Plain 0 and negative integers → 0 (treated as permanent) ─────────────
    TestEqual(TEXT("\"0\" → 0"),          UBanDiscordSubsystem::ParseDurationMinutes(TEXT("0")),  0);
    // Negative is not IsNumeric() (has '-') → falls to multi-token → '-' not a digit → 0
    TestEqual(TEXT("\"-5\" → 0"),         UBanDiscordSubsystem::ParseDurationMinutes(TEXT("-5")), 0);

    // ── Single-unit suffixes ──────────────────────────────────────────────────
    TestEqual(TEXT("30m → 30"),           UBanDiscordSubsystem::ParseDurationMinutes(TEXT("30m")),  30);
    TestEqual(TEXT("2h → 120"),           UBanDiscordSubsystem::ParseDurationMinutes(TEXT("2h")),   120);
    TestEqual(TEXT("1d → 1440"),          UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1d")),   1440);
    TestEqual(TEXT("1w → 10080"),         UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1w")),   10080);
    TestEqual(TEXT("7d → 10080"),         UBanDiscordSubsystem::ParseDurationMinutes(TEXT("7d")),   10080);
    TestEqual(TEXT("24h → 1440"),         UBanDiscordSubsystem::ParseDurationMinutes(TEXT("24h")),  1440);

    // ── Compound (concatenated) duration strings ───────────────────────────────
    TestEqual(TEXT("2h30m → 150"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("2h30m")),    150);
    TestEqual(TEXT("1d12h → 2160"),       UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1d12h")),    2160);
    TestEqual(TEXT("1w2d → 12960"),       UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1w2d")),     12960);
    // 1w=10080, 2d=2880, 3h=180, 4m=4 → 13144
    TestEqual(TEXT("1w2d3h4m → 13144"),   UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1w2d3h4m")), 13144);

    // ── Compound with spaces between tokens ───────────────────────────────────
    TestEqual(TEXT("\"2h 30m\" → 150"),   UBanDiscordSubsystem::ParseDurationMinutes(TEXT("2h 30m")), 150);
    TestEqual(TEXT("\"1d 12h\" → 2160"),  UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1d 12h")), 2160);
    TestEqual(TEXT("\"1w 2d 3h\" → 13140"),
        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1w 2d 3h")), 13140); // 10080+2880+180

    // ── Case-insensitive suffix (uppercase W/D/H/M) ───────────────────────────
    TestEqual(TEXT("2H → 120"),           UBanDiscordSubsystem::ParseDurationMinutes(TEXT("2H")),   120);
    TestEqual(TEXT("1D → 1440"),          UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1D")),   1440);
    TestEqual(TEXT("1W → 10080"),         UBanDiscordSubsystem::ParseDurationMinutes(TEXT("1W")),   10080);
    TestEqual(TEXT("30M → 30"),           UBanDiscordSubsystem::ParseDurationMinutes(TEXT("30M")),  30);

    // ── Invalid inputs → 0 ───────────────────────────────────────────────────
    TestEqual(TEXT("\"abc\" → 0"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("abc")),  0);
    TestEqual(TEXT("\"30x\" → 0"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("30x")),  0);
    TestEqual(TEXT("\"h30\" → 0"),        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("h30")),  0);
    // Digits without trailing unit
    TestEqual(TEXT("\"30h20\" → 0 (no unit at end)"),
        UBanDiscordSubsystem::ParseDurationMinutes(TEXT("30h20")), 0);

    // ── Zero-valued tokens ────────────────────────────────────────────────────
    // "0m" → bHadToken=true but Total=0 → returns 0
    TestEqual(TEXT("0m → 0"),             UBanDiscordSubsystem::ParseDurationMinutes(TEXT("0m")),   0);
    // "0d" → 0
    TestEqual(TEXT("0d → 0"),             UBanDiscordSubsystem::ParseDurationMinutes(TEXT("0d")),   0);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  IsValidEOSPUID
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIsValidEOSPUIDTest,
    "DiscordBridge.BanDiscordSubsystem.IsValidEOSPUID",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FIsValidEOSPUIDTest::RunTest(const FString& Parameters)
{
    // ── Valid 32-character lowercase hex ──────────────────────────────────────
    TestTrue(TEXT("32 lower hex → valid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed12345678abcdef0123456789")));

    // ── Valid 32-character uppercase hex ──────────────────────────────────────
    TestTrue(TEXT("32 upper hex → valid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020AED12345678ABCDEF0123456789")));

    // ── Valid 32-character mixed-case hex ─────────────────────────────────────
    TestTrue(TEXT("32 mixed-case hex → valid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed12345678ABCDEF0123456789")));

    // ── All zeros ─────────────────────────────────────────────────────────────
    TestTrue(TEXT("32 zeros → valid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00000000000000000000000000000000")));

    // ── Too short (31 characters) ─────────────────────────────────────────────
    TestFalse(TEXT("31 chars → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed12345678abcdef012345678")));

    // ── Too long (33 characters) ──────────────────────────────────────────────
    TestFalse(TEXT("33 chars → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed12345678abcdef01234567890")));

    // ── Non-hex character ('g') ───────────────────────────────────────────────
    TestFalse(TEXT("32 chars with 'g' → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed12345678abcdef012345678g")));

    // ── Contains a space ──────────────────────────────────────────────────────
    TestFalse(TEXT("32 chars with space → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("00020aed 2345678abcdef0123456789")));

    // ── Empty string ──────────────────────────────────────────────────────────
    TestFalse(TEXT("empty string → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("")));

    // ── Compound UID format (should NOT be valid as a raw PUID) ──────────────
    TestFalse(TEXT("compound UID → invalid"),
        UBanDiscordSubsystem::IsValidEOSPUID(TEXT("EOS:00020aed12345678abcdef01234567")));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  IsValidIPQuery
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FIsValidIPQueryTest,
    "DiscordBridge.BanDiscordSubsystem.IsValidIPQuery",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FIsValidIPQueryTest::RunTest(const FString& Parameters)
{
    // ── Explicit "IP:" prefix ──────────────────────────────────────────────────
    TestTrue(TEXT("IP:192.168.1.1 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("IP:192.168.1.1")));
    TestTrue(TEXT("IP:10.0.0.1 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("IP:10.0.0.1")));
    TestTrue(TEXT("IP:255.255.255.255 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("IP:255.255.255.255")));

    // ── Bare IPv4 address (digits + dots, has at least one dot) ───────────────
    TestTrue(TEXT("bare 192.168.1.1 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("192.168.1.1")));
    TestTrue(TEXT("bare 10.0.0.1 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("10.0.0.1")));
    TestTrue(TEXT("bare 127.0.0.1 → valid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("127.0.0.1")));

    // ── Invalid: player name containing a dot ─────────────────────────────────
    TestFalse(TEXT("player.name → invalid (has letter)"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("player.name")));

    // ── Invalid: digits only, no dot ─────────────────────────────────────────
    TestFalse(TEXT("12345 → invalid (no dot)"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("12345")));

    // ── Invalid: "IP:" with empty address ─────────────────────────────────────
    TestFalse(TEXT("IP: (empty address) → invalid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("IP:")));

    // ── Invalid: EOS compound UID ─────────────────────────────────────────────
    TestFalse(TEXT("EOS:abc → invalid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("EOS:abc")));

    // ── Invalid: empty string ─────────────────────────────────────────────────
    TestFalse(TEXT("empty string → invalid"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("")));

    // ── Invalid: alphanumeric with dot (not a bare IP) ────────────────────────
    TestFalse(TEXT("abc.def → invalid (has letter)"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("abc.def")));

    // ── Invalid: mixed digits and letters in numeric portion ──────────────────
    TestFalse(TEXT("192.168.1.a → invalid (has letter in last octet)"),
        UBanDiscordSubsystem::IsValidIPQuery(TEXT("192.168.1.a")));

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
