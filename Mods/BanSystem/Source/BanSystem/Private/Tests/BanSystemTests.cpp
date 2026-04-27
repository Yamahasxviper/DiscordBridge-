// Copyright Yamahasxviper. All Rights Reserved.
//
// Automation tests for pure-logic helpers in the BanSystem mod.
//
// Covered functions (no subsystem lifecycle / file I/O required):
//   FBanTemplate::FromConfigString()
//   FBanTemplate::ParseTemplates()
//   FBanEntry::IsExpired()
//   FBanEntry::MatchesUid()
//   FWarningEntry::IsExpired()
//   UBanDatabase::MakeUid()
//   UBanDatabase::ParseUid()

#include "CoreTypes.h"
#include "Misc/AutomationTest.h"
#include "Math/UnrealMathUtility.h"

#include "BanTypes.h"
#include "BanDatabase.h"

#if WITH_DEV_AUTOMATION_TESTS

// ─────────────────────────────────────────────────────────────────────────────
//  FBanTemplate::FromConfigString
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBanTemplateFromConfigStringTest,
    "BanSystem.BanTypes.BanTemplate.FromConfigString",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBanTemplateFromConfigStringTest::RunTest(const FString& Parameters)
{
    FBanTemplate T;

    // ── Valid 3-part template ─────────────────────────────────────────────────
    TestTrue(TEXT("3-part: returns true"),
        FBanTemplate::FromConfigString(TEXT("griefing|1440|Griefing"), T));
    TestEqual(TEXT("3-part: slug"),           T.Slug,            FString(TEXT("griefing")));
    TestEqual(TEXT("3-part: DurationMinutes"), T.DurationMinutes, 1440);
    TestEqual(TEXT("3-part: reason"),          T.Reason,          FString(TEXT("Griefing")));
    TestTrue (TEXT("3-part: Category is empty"), T.Category.IsEmpty());

    // ── Valid 4-part template (with category) ─────────────────────────────────
    TestTrue(TEXT("4-part: returns true"),
        FBanTemplate::FromConfigString(TEXT("hack|0|Hacking|cheating"), T));
    TestEqual(TEXT("4-part: slug"),     T.Slug,     FString(TEXT("hack")));
    TestEqual(TEXT("4-part: duration"), T.DurationMinutes, 0);
    TestEqual(TEXT("4-part: reason"),   T.Reason,   FString(TEXT("Hacking")));
    TestEqual(TEXT("4-part: category"), T.Category, FString(TEXT("cheating")));

    // ── Permanent duration (0) ────────────────────────────────────────────────
    TestTrue(TEXT("zero duration: returns true"),
        FBanTemplate::FromConfigString(TEXT("perm|0|Permanent ban"), T));
    TestEqual(TEXT("zero duration: slug"),     T.Slug,            FString(TEXT("perm")));
    TestEqual(TEXT("zero duration: minutes"),  T.DurationMinutes, 0);

    // ── Negative duration (treated as permanent by convention) ────────────────
    TestTrue(TEXT("negative duration: returns true"),
        FBanTemplate::FromConfigString(TEXT("slug|-60|reason"), T));
    TestEqual(TEXT("negative duration: value"), T.DurationMinutes, -60);

    // ── Very large duration clamped to INT32_MAX ──────────────────────────────
    TestTrue(TEXT("oversized duration: returns true"),
        FBanTemplate::FromConfigString(TEXT("slug|9999999999999|reason"), T));
    TestEqual(TEXT("oversized duration: clamped"), T.DurationMinutes, INT32_MAX);

    // ── Reason that contains spaces ───────────────────────────────────────────
    TestTrue(TEXT("spaced reason: returns true"),
        FBanTemplate::FromConfigString(TEXT("harassment|4320|Harassment and bullying"), T));
    TestEqual(TEXT("spaced reason: reason"),
        T.Reason, FString(TEXT("Harassment and bullying")));

    // ── Too few parts (1 field) ───────────────────────────────────────────────
    TestFalse(TEXT("1 field: returns false"),
        FBanTemplate::FromConfigString(TEXT("justslug"), T));

    // ── Too few parts (2 fields) ──────────────────────────────────────────────
    TestFalse(TEXT("2 fields: returns false"),
        FBanTemplate::FromConfigString(TEXT("slug|1440"), T));

    // ── Non-numeric duration ──────────────────────────────────────────────────
    TestFalse(TEXT("non-numeric duration: returns false"),
        FBanTemplate::FromConfigString(TEXT("slug|abc|reason"), T));

    // ── Alphanumeric duration (partially numeric) ─────────────────────────────
    TestFalse(TEXT("partial-numeric duration: returns false"),
        FBanTemplate::FromConfigString(TEXT("slug|12abc|reason"), T));

    // ── Empty string ──────────────────────────────────────────────────────────
    TestFalse(TEXT("empty string: returns false"),
        FBanTemplate::FromConfigString(TEXT(""), T));

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FBanTemplate::ParseTemplates
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBanTemplateParseTemplatesTest,
    "BanSystem.BanTypes.BanTemplate.ParseTemplates",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBanTemplateParseTemplatesTest::RunTest(const FString& Parameters)
{
    // ── Empty input ───────────────────────────────────────────────────────────
    {
        TArray<FString> Empty;
        TArray<FBanTemplate> Result = FBanTemplate::ParseTemplates(Empty);
        TestEqual(TEXT("empty input: result is empty"), Result.Num(), 0);
    }

    // ── All invalid ───────────────────────────────────────────────────────────
    {
        TArray<FString> AllBad = {
            TEXT("onlyslug"),
            TEXT("slug|notanumber|reason"),
            TEXT("slug|1440"),
        };
        TArray<FBanTemplate> Result = FBanTemplate::ParseTemplates(AllBad);
        TestEqual(TEXT("all invalid: result is empty"), Result.Num(), 0);
    }

    // ── Mixed valid and invalid ───────────────────────────────────────────────
    {
        TArray<FString> Mixed = {
            TEXT("a|60|Reason A"),
            TEXT("b|invalid|Reason B"),
            TEXT("c|120|Reason C|cat"),
        };
        TArray<FBanTemplate> Result = FBanTemplate::ParseTemplates(Mixed);
        TestEqual(TEXT("mixed: 2 valid templates parsed"), Result.Num(), 2);
        if (Result.Num() >= 2)
        {
            TestEqual(TEXT("mixed: first slug"),     Result[0].Slug,            FString(TEXT("a")));
            TestEqual(TEXT("mixed: first duration"), Result[0].DurationMinutes, 60);
            TestEqual(TEXT("mixed: second slug"),    Result[1].Slug,            FString(TEXT("c")));
            TestEqual(TEXT("mixed: second duration"),Result[1].DurationMinutes, 120);
            TestEqual(TEXT("mixed: second category"),Result[1].Category,        FString(TEXT("cat")));
        }
    }

    // ── All valid ─────────────────────────────────────────────────────────────
    {
        TArray<FString> AllGood = {
            TEXT("s1|30|Short ban"),
            TEXT("s2|1440|Day ban|griefing"),
            TEXT("s3|0|Permanent|cheating"),
        };
        TArray<FBanTemplate> Result = FBanTemplate::ParseTemplates(AllGood);
        TestEqual(TEXT("all valid: 3 templates"), Result.Num(), 3);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FBanEntry::IsExpired / FBanEntry::MatchesUid
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBanEntryTest,
    "BanSystem.BanTypes.BanEntry",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBanEntryTest::RunTest(const FString& Parameters)
{
    // ── IsExpired: permanent ban never expires ────────────────────────────────
    {
        FBanEntry E;
        E.bIsPermanent = true;
        TestFalse(TEXT("permanent ban: IsExpired() = false"), E.IsExpired());
    }

    // ── IsExpired: temp ban with future expiry ────────────────────────────────
    {
        FBanEntry E;
        E.bIsPermanent = false;
        E.ExpireDate   = FDateTime::UtcNow() + FTimespan::FromHours(1.0);
        TestFalse(TEXT("temp ban with future expiry: IsExpired() = false"), E.IsExpired());
    }

    // ── IsExpired: temp ban with past expiry ──────────────────────────────────
    {
        FBanEntry E;
        E.bIsPermanent = false;
        E.ExpireDate   = FDateTime::UtcNow() - FTimespan::FromMinutes(1.0);
        TestTrue(TEXT("temp ban with past expiry: IsExpired() = true"), E.IsExpired());
    }

    // ── MatchesUid: matches primary UID (exact case) ──────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        TestTrue(TEXT("MatchesUid: primary exact"), E.MatchesUid(TEXT("EOS:abc123")));
    }

    // ── MatchesUid: primary UID is case-insensitive ───────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        TestTrue(TEXT("MatchesUid: primary case-insensitive"),
            E.MatchesUid(TEXT("eos:ABC123")));
    }

    // ── MatchesUid: matches linked UID ───────────────────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        E.LinkedUids.Add(TEXT("EOS:def456"));
        E.LinkedUids.Add(TEXT("EOS:ghi789"));
        TestTrue(TEXT("MatchesUid: linked UID found"),
            E.MatchesUid(TEXT("EOS:def456")));
        TestTrue(TEXT("MatchesUid: second linked UID found"),
            E.MatchesUid(TEXT("EOS:ghi789")));
    }

    // ── MatchesUid: linked UID is case-insensitive ────────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        E.LinkedUids.Add(TEXT("EOS:def456"));
        TestTrue(TEXT("MatchesUid: linked UID case-insensitive"),
            E.MatchesUid(TEXT("eos:DEF456")));
    }

    // ── MatchesUid: unknown UID does not match ────────────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        E.LinkedUids.Add(TEXT("EOS:def456"));
        TestFalse(TEXT("MatchesUid: unrelated UID not found"),
            E.MatchesUid(TEXT("EOS:zzz999")));
    }

    // ── MatchesUid: empty search UID ──────────────────────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        TestFalse(TEXT("MatchesUid: empty search string"),
            E.MatchesUid(TEXT("")));
    }

    // ── MatchesUid: no linked UIDs ────────────────────────────────────────────
    {
        FBanEntry E;
        E.Uid = TEXT("EOS:abc123");
        // No LinkedUids
        TestFalse(TEXT("MatchesUid: no linked UIDs - not found"),
            E.MatchesUid(TEXT("EOS:other")));
        TestTrue(TEXT("MatchesUid: no linked UIDs - primary still found"),
            E.MatchesUid(TEXT("EOS:abc123")));
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FWarningEntry::IsExpired
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWarningEntryExpiryTest,
    "BanSystem.BanTypes.WarningEntry.IsExpired",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FWarningEntryExpiryTest::RunTest(const FString& Parameters)
{
    // ── No expiry: warning is never expired ───────────────────────────────────
    {
        FWarningEntry W;
        W.bHasExpiry = false;
        TestFalse(TEXT("no expiry: IsExpired() = false"), W.IsExpired());
    }

    // ── Has expiry, time is in the past ──────────────────────────────────────
    {
        FWarningEntry W;
        W.bHasExpiry  = true;
        W.ExpireDate  = FDateTime::UtcNow() - FTimespan::FromHours(1.0);
        TestTrue(TEXT("past expiry: IsExpired() = true"), W.IsExpired());
    }

    // ── Has expiry, time is in the future ────────────────────────────────────
    {
        FWarningEntry W;
        W.bHasExpiry  = true;
        W.ExpireDate  = FDateTime::UtcNow() + FTimespan::FromHours(24.0);
        TestFalse(TEXT("future expiry: IsExpired() = false"), W.IsExpired());
    }

    // ── bHasExpiry = false overrides ExpireDate even if it is in the past ────
    {
        FWarningEntry W;
        W.bHasExpiry = false;
        W.ExpireDate = FDateTime::UtcNow() - FTimespan::FromDays(365.0);
        TestFalse(TEXT("bHasExpiry=false ignores past ExpireDate"), W.IsExpired());
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  UBanDatabase::MakeUid / ParseUid  (static helpers — no subsystem needed)
// ─────────────────────────────────────────────────────────────────────────────

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBanDatabaseUidHelpersTest,
    "BanSystem.BanDatabase.UidHelpers",
    EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::SmokeFilter)

bool FBanDatabaseUidHelpersTest::RunTest(const FString& Parameters)
{
    // ── MakeUid: basic EOS platform ───────────────────────────────────────────
    TestEqual(TEXT("MakeUid EOS"),
        UBanDatabase::MakeUid(TEXT("EOS"), TEXT("abc123")),
        FString(TEXT("EOS:abc123")));

    // ── MakeUid: platform is upper-cased ─────────────────────────────────────
    TestEqual(TEXT("MakeUid platform upper-cased"),
        UBanDatabase::MakeUid(TEXT("eos"), TEXT("abc123")),
        FString(TEXT("EOS:abc123")));

    // ── MakeUid: IP platform ──────────────────────────────────────────────────
    TestEqual(TEXT("MakeUid IP"),
        UBanDatabase::MakeUid(TEXT("IP"), TEXT("192.168.1.1")),
        FString(TEXT("IP:192.168.1.1")));

    // ── MakeUid: round-trip with ParseUid ────────────────────────────────────
    {
        const FString Uid = UBanDatabase::MakeUid(TEXT("EOS"), TEXT("00020aed12345678abcdef0123456789"));
        FString Platform, RawId;
        UBanDatabase::ParseUid(Uid, Platform, RawId);
        TestEqual(TEXT("Round-trip: platform"), Platform, FString(TEXT("EOS")));
        TestEqual(TEXT("Round-trip: raw id"),   RawId,    FString(TEXT("00020aed12345678abcdef0123456789")));
    }

    // ── ParseUid: standard compound UID ──────────────────────────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT("EOS:abc123"), Platform, RawId);
        TestEqual(TEXT("ParseUid EOS platform"), Platform, FString(TEXT("EOS")));
        TestEqual(TEXT("ParseUid EOS raw id"),   RawId,    FString(TEXT("abc123")));
    }

    // ── ParseUid: IP compound UID ─────────────────────────────────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT("IP:10.0.0.1"), Platform, RawId);
        TestEqual(TEXT("ParseUid IP platform"), Platform, FString(TEXT("IP")));
        TestEqual(TEXT("ParseUid IP raw id"),   RawId,    FString(TEXT("10.0.0.1")));
    }

    // ── ParseUid: lowercase platform is upper-cased ───────────────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT("eos:abc123"), Platform, RawId);
        TestEqual(TEXT("ParseUid lower platform uppercased"), Platform, FString(TEXT("EOS")));
    }

    // ── ParseUid: no colon → UNKNOWN platform ────────────────────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT("plainstring"), Platform, RawId);
        TestEqual(TEXT("ParseUid no colon: platform = UNKNOWN"), Platform, FString(TEXT("UNKNOWN")));
        TestEqual(TEXT("ParseUid no colon: raw id = full input"), RawId, FString(TEXT("plainstring")));
    }

    // ── ParseUid: colon at position 0 → treated as no-colon ──────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT(":abc"), Platform, RawId);
        // FindChar returns ColonIdx=0; condition requires ColonIdx > 0 → falls to else
        TestEqual(TEXT("ParseUid colon at pos 0: platform = UNKNOWN"), Platform, FString(TEXT("UNKNOWN")));
        TestEqual(TEXT("ParseUid colon at pos 0: raw id = full input"), RawId, FString(TEXT(":abc")));
    }

    // ── ParseUid: multiple colons — only first colon is the delimiter ─────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT("EOS:abc:def"), Platform, RawId);
        TestEqual(TEXT("ParseUid multi-colon: platform"), Platform, FString(TEXT("EOS")));
        TestEqual(TEXT("ParseUid multi-colon: raw id includes rest"), RawId, FString(TEXT("abc:def")));
    }

    // ── ParseUid: empty string ────────────────────────────────────────────────
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(TEXT(""), Platform, RawId);
        TestEqual(TEXT("ParseUid empty: platform = UNKNOWN"), Platform, FString(TEXT("UNKNOWN")));
        TestTrue (TEXT("ParseUid empty: raw id is empty"), RawId.IsEmpty());
    }

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
