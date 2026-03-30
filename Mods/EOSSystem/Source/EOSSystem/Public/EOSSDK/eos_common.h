// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_common.h — delegates to the real EOS SDK eos_common.h, with fallback
// definitions for the CSS UE5.3.2 engine which ships a minimal EOSSDK that
// provides EOS handle typedefs (via eos_types.h) but omits the enum value
// constants (EOS_Success, EOS_InvalidParameters, EOS_Duplicate, etc.).
//
// The real EOS SDK eos_common.h defines these as C enum values in EOS_EResult.
// The CSS engine may define EOS_EResult as a C++ scoped enum class (enum class
// EOS_EResult : int32_t), which does NOT allow implicit comparison with integer
// literals — requiring an explicit cast when the constant is not a named member.
//
// Strategy:
//   1. Attempt to include <eos_common.h> via __has_include (avoids C1083 if
//      the file is absent from the CSS EOSSDK distribution).
//   2. Provide each enum value constant as a fallback #define macro, guarded by
//      #ifndef so an existing macro definition from the real SDK wins (avoiding
//      C4005 redefinition warnings).  Constants are defined using C-style casts
//      (e.g. ((EOS_EResult)0)) so they are type-compatible whether EOS_EResult
//      is a plain typedef, a C unscoped enum, or a C++ scoped enum class.
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
//  evaluate to true and the macros are added using explicit C-style casts so
//  comparisons work whether EOS_EResult is a typedef-int, C enum, or C++ enum
//  class.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_Success
// Use explicit casts to EOS_EResult so these macros work whether the engine
// defines EOS_EResult as a plain typedef-int, a C unscoped enum, or a C++
// scoped enum class (which does not allow implicit integer comparisons).
#  define EOS_Success                  ((EOS_EResult)0)
#  define EOS_NoConnection             ((EOS_EResult)1)
#  define EOS_InvalidCredentials       ((EOS_EResult)2)
#  define EOS_InvalidUser              ((EOS_EResult)3)
#  define EOS_InvalidAuth              ((EOS_EResult)4)
#  define EOS_AccessDenied             ((EOS_EResult)5)
#  define EOS_MissingPermissions       ((EOS_EResult)6)
#  define EOS_TokenNotAccount          ((EOS_EResult)7)
#  define EOS_TooManyRequests          ((EOS_EResult)8)
#  define EOS_AlreadyPending           ((EOS_EResult)9)
#  define EOS_InvalidParameters        ((EOS_EResult)10)
#  define EOS_InvalidRequest           ((EOS_EResult)11)
#  define EOS_UnrecognizedResponse     ((EOS_EResult)12)
#  define EOS_IncompatibleVersion      ((EOS_EResult)13)
#  define EOS_NotConfigured            ((EOS_EResult)14)
#  define EOS_AlreadyConfigured        ((EOS_EResult)15)
#  define EOS_NotImplemented           ((EOS_EResult)16)
#  define EOS_Canceled                 ((EOS_EResult)17)
#  define EOS_NotFound                 ((EOS_EResult)18)
#  define EOS_OperationWillRetry       ((EOS_EResult)19)
#  define EOS_NoChange                 ((EOS_EResult)20)
#  define EOS_VersionMismatch          ((EOS_EResult)21)
#  define EOS_LimitExceeded            ((EOS_EResult)22)
#  define EOS_Duplicate                ((EOS_EResult)23)
#  define EOS_MissingParameters        ((EOS_EResult)24)
#  define EOS_InvalidSandboxId         ((EOS_EResult)25)
#  define EOS_TimedOut                 ((EOS_EResult)26)
#  define EOS_PartialResult            ((EOS_EResult)27)
#  define EOS_Missing_Role             ((EOS_EResult)28)
#  define EOS_Missing_Feature          ((EOS_EResult)29)
#  define EOS_Invalid_Sandbox          ((EOS_EResult)30)
#  define EOS_Invalid_Deployment       ((EOS_EResult)31)
#  define EOS_Invalid_Product          ((EOS_EResult)32)
#  define EOS_Invalid_ProductUserID    ((EOS_EResult)33)
#  define EOS_ServiceFailure           ((EOS_EResult)34)
#  define EOS_CacheDirectoryMissing    ((EOS_EResult)35)
#  define EOS_CacheDirectoryInvalid    ((EOS_EResult)36)
#  define EOS_InvalidState             ((EOS_EResult)37)
#  define EOS_RequestInProgress        ((EOS_EResult)38)
#  define EOS_ApplicationSuspended     ((EOS_EResult)39)
#  define EOS_NetworkChanged           ((EOS_EResult)40)
#endif // EOS_Success

// ─────────────────────────────────────────────────────────────────────────────
//  Fallback EOS_EExternalAccountType value constants (EOS_EAT_*).
//  Used by the Connect subsystem for external account mapping queries.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_EAT_EPIC
// Use explicit casts so these constants work with a C++ scoped enum class.
#  define EOS_EAT_EPIC            ((EOS_EExternalAccountType)0)
#  define EOS_EAT_STEAM           ((EOS_EExternalAccountType)1)
#  define EOS_EAT_PSN             ((EOS_EExternalAccountType)2)
#  define EOS_EAT_XBL             ((EOS_EExternalAccountType)3)
#  define EOS_EAT_DISCORD         ((EOS_EExternalAccountType)4)
#  define EOS_EAT_GOG             ((EOS_EExternalAccountType)5)
#  define EOS_EAT_NINTENDO        ((EOS_EExternalAccountType)6)
#  define EOS_EAT_UPLAY           ((EOS_EExternalAccountType)7)
#  define EOS_EAT_OPENID          ((EOS_EExternalAccountType)8)
#  define EOS_EAT_APPLE           ((EOS_EExternalAccountType)9)
#  define EOS_EAT_GOOGLE          ((EOS_EExternalAccountType)10)
#  define EOS_EAT_OCULUS          ((EOS_EExternalAccountType)11)
#  define EOS_EAT_ITCHIO          ((EOS_EExternalAccountType)12)
#  define EOS_EAT_AMAZON          ((EOS_EExternalAccountType)13)
#endif // EOS_EAT_EPIC

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_INVALID_NOTIFICATIONID — sentinel value for invalid notification handles.
//  Defined in the EOS SDK as 0.  The CSS engine's minimal eos_common.h may omit
//  it; guard with #ifndef so the real SDK definition wins when present.
//  Required by EOSConnectSubsystem.h member initialiser:
//    EOS_NotificationId AuthExpiryNotif = EOS_INVALID_NOTIFICATIONID;
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_INVALID_NOTIFICATIONID
#  define EOS_INVALID_NOTIFICATIONID 0
#endif
