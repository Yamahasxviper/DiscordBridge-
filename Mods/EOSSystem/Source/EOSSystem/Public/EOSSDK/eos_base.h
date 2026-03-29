// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_base.h  — fundamental EOS C-SDK types written from scratch.
//
// These declarations are derived solely from the publicly available EOS SDK
// documentation (https://dev.epicgames.com/docs).  No UE EOSSDK module,
// no EOSShared, and no CSS FactoryGame headers are referenced anywhere in
// this file or the EOSSystem mod.
//
// INCLUDE GUARD NOTE
// ──────────────────
// The engine's ThirdParty EOSSDK eos_base.h also uses EOS_BASE_H as its
// include guard.  By defining EOS_BASE_H here, we ensure that when this
// mod-local header is included first in a translation unit, the engine's
// eos_base.h is skipped entirely — preventing the C1189 "macros not defined"
// error.
//
// STRUCT NAME NOTE
// ────────────────
// All opaque handle struct names match the real EOS SDK names (no 'Details'
// suffix).  This ensures that when both this header and the engine's EOSSDK
// headers are visible in the same translation unit, the typedef declarations
// are identical and no C2371 "redefinition; different basic types" errors
// occur.  Identical typedef redeclarations are permitted in C++.
//
// EOS_EResult / EOS_NotificationId NOTE
// ───────────────────────────────────────
// EOS_EResult and EOS_NotificationId are defined in eos_common.h (where they
// belong per real EOS SDK structure), not here.  eos_init.h and eos_platform.h
// both include eos_common.h to obtain these types.  This prevents the C2371
// conflict that occurs when our old 'typedef int32_t EOS_EResult' clashed with
// the engine's eos_common.h which defines EOS_EResult as a proper enum.

#ifndef EOS_BASE_H
#define EOS_BASE_H

#include <stdint.h>  // int32_t, uint64_t, etc.

// ─────────────────────────────────────────────────────────────────────────────
//  Calling convention macros
//  All three are required by the engine's eos_base.h check at line 35.
//  Define them here so that any translation unit that reaches the engine's
//  eos_base.h (via EOSDirectSDK's EOSSDK dependency) already has them set.
// ─────────────────────────────────────────────────────────────────────────────
#ifndef EOS_CALL
#  if defined(_WIN32)
#    define EOS_CALL __cdecl
#  else
#    define EOS_CALL
#  endif
#endif

#ifndef EOS_MEMORY_CALL
#  define EOS_MEMORY_CALL EOS_CALL
#endif

#ifndef EOS_USE_DLLEXPORT
#  define EOS_USE_DLLEXPORT 0
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Boolean
// ─────────────────────────────────────────────────────────────────────────────
typedef int32_t EOS_Bool;
#define EOS_TRUE  1
#define EOS_FALSE 0

// ─────────────────────────────────────────────────────────────────────────────
//  Opaque handle types  (all are pointer-sized; actual structs are internal)
//
//  IMPORTANT: struct names must match the real EOS SDK (no 'Details' suffix)
//  so that these typedefs are IDENTICAL to the engine's EOSSDK definitions.
//  Identical typedef redeclarations in the same TU are valid in C++.
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_PlatformHandle;
struct EOS_EpicAccountIdHandle;
struct EOS_ProductUserIdHandle;
struct EOS_ConnectHandle;
struct EOS_SessionsHandle;
struct EOS_SessionModificationHandle;
struct EOS_SessionDetailsHandle;
struct EOS_SessionSearchHandle;
struct EOS_AntiCheatServerHandle;
struct EOS_SanctionsHandle;
struct EOS_MetricsHandle;
struct EOS_ReportsHandle;
struct EOS_ContinuanceTokenHandle;
struct EOS_PlayerSanctionHandle;

typedef struct EOS_PlatformHandle*            EOS_HPlatform;
typedef struct EOS_EpicAccountIdHandle*       EOS_EpicAccountId;
typedef struct EOS_ProductUserIdHandle*       EOS_ProductUserId;
typedef struct EOS_ConnectHandle*             EOS_HConnect;
typedef struct EOS_SessionsHandle*            EOS_HSessions;
typedef struct EOS_SessionModificationHandle* EOS_HSessionModification;
typedef struct EOS_SessionDetailsHandle*      EOS_HSessionDetails;
typedef struct EOS_SessionSearchHandle*       EOS_HSessionSearch;
typedef struct EOS_AntiCheatServerHandle*     EOS_HAntiCheatServer;
typedef struct EOS_SanctionsHandle*           EOS_HSanctions;
typedef struct EOS_MetricsHandle*             EOS_HMetrics;
typedef struct EOS_ReportsHandle*             EOS_HReports;
typedef struct EOS_ContinuanceTokenHandle*    EOS_ContinuanceToken;
typedef struct EOS_PlayerSanctionHandle*      EOS_HPlayerSanction;

#endif // EOS_BASE_H
