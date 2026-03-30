// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_common.h — delegates to the real EOS SDK eos_common.h, with fallback
// definitions for the CSS UE5.3.2 engine which ships a minimal EOSSDK that
// provides EOS handle typedefs (via eos_types.h) but omits the enum value
// constants (EOS_Success, EOS_InvalidParameters, EOS_Duplicate, etc.).
//
// The real EOS SDK eos_common.h defines these as C enum values in EOS_EResult.
// If the CSS engine's eos_common.h instead typedef's EOS_EResult as int32_t
// (providing the TYPE without named values), the constants are simply absent.
//
// Strategy:
//   1. Attempt to include <eos_common.h> via __has_include (avoids C1083 if
//      the file is absent from the CSS EOSSDK distribution).
//   2. Provide each enum value constant as a fallback #define macro, guarded by
//      #ifndef.  This guard is INTENTIONALLY a macro check, not an enum check:
//      - If the real header defines the constant as a macro: #ifndef is FALSE →
//        our definition is skipped (correct, avoid C4005 redefinition warning).
//      - If the real header defines the constant as an enum value: #ifndef is
//        TRUE → we still define the macro.  The macro (same integer value)
//        safely shadows the enum value — no type difference in practice.
//      - If the constant is simply absent: #ifndef is TRUE → we define it.
//
// NOTE: Do NOT define EOS_COMMON_H BEFORE the #include — that would cause
// the real header's own guard to skip its contents.

#pragma once

#if defined(__has_include) && __has_include(<eos_common.h>)
#  include <eos_common.h>
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EResult value constants.
//  Each is guarded by #ifndef so an existing macro (from the real SDK) wins.
//  If the real SDK defines these only as enum values (not macros), the guards
//  are vacuously true and the macros are added; the integer values are identical
//  to what the real SDK enum uses, so all comparisons remain correct.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_Success
#  define EOS_Success                  0
#  define EOS_NoConnection             1
#  define EOS_InvalidCredentials       2
#  define EOS_InvalidUser              3
#  define EOS_InvalidAuth              4
#  define EOS_AccessDenied             5
#  define EOS_MissingPermissions       6
#  define EOS_TokenNotAccount          7
#  define EOS_TooManyRequests          8
#  define EOS_AlreadyPending           9
#  define EOS_InvalidParameters        10
#  define EOS_InvalidRequest           11
#  define EOS_UnrecognizedResponse     12
#  define EOS_IncompatibleVersion      13
#  define EOS_NotConfigured            14
#  define EOS_AlreadyConfigured        15
#  define EOS_NotImplemented           16
#  define EOS_Canceled                 17
#  define EOS_NotFound                 18
#  define EOS_OperationWillRetry       19
#  define EOS_NoChange                 20
#  define EOS_VersionMismatch          21
#  define EOS_LimitExceeded            22
#  define EOS_Duplicate                23
#  define EOS_MissingParameters        24
#  define EOS_InvalidSandboxId         25
#  define EOS_TimedOut                 26
#  define EOS_PartialResult            27
#  define EOS_Missing_Role             28
#  define EOS_Missing_Feature          29
#  define EOS_Invalid_Sandbox          30
#  define EOS_Invalid_Deployment       31
#  define EOS_Invalid_Product          32
#  define EOS_Invalid_ProductUserID    33
#  define EOS_ServiceFailure           34
#  define EOS_CacheDirectoryMissing    35
#  define EOS_CacheDirectoryInvalid    36
#  define EOS_InvalidState             37
#  define EOS_RequestInProgress        38
#  define EOS_ApplicationSuspended     39
#  define EOS_NetworkChanged           40
#endif // EOS_Success

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EExternalAccountType value constants (EOS_EAT_*).
//  Used by the Connect subsystem for external account mapping queries.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_EAT_EPIC
#  define EOS_EAT_EPIC            0
#  define EOS_EAT_STEAM           1
#  define EOS_EAT_PSN             2
#  define EOS_EAT_XBL             3
#  define EOS_EAT_DISCORD         4
#  define EOS_EAT_GOG             5
#  define EOS_EAT_NINTENDO        6
#  define EOS_EAT_UPLAY           7
#  define EOS_EAT_OPENID          8
#  define EOS_EAT_APPLE           9
#  define EOS_EAT_GOOGLE          10
#  define EOS_EAT_OCULUS          11
#  define EOS_EAT_ITCHIO          12
#  define EOS_EAT_AMAZON          13
#endif // EOS_EAT_EPIC
