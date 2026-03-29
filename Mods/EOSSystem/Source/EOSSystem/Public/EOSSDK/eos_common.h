// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_common.h — common EOS SDK types, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_base.h"

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Account ID string lengths (including null terminator)
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_EPICACCOUNTID_MAX_LENGTH  33
#define EOS_PRODUCTUSERID_MAX_LENGTH  33

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EExternalAccountType
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EExternalAccountType
{
    EOS_EAT_EPIC       = 0,
    EOS_EAT_STEAM      = 1,
    EOS_EAT_PSN        = 2,
    EOS_EAT_XBL        = 3,
    EOS_EAT_DISCORD    = 4,
    EOS_EAT_GOG        = 5,
    EOS_EAT_NINTENDO   = 6,
    EOS_EAT_OCULUS     = 7,
    EOS_EAT_ITCHIO     = 8,
    EOS_EAT_AMAZON     = 9
} EOS_EExternalAccountType;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_ELoginStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_ELoginStatus
{
    EOS_LS_NotLoggedIn        = 0,
    EOS_LS_UsingLocalProfile  = 1,
    EOS_LS_LoggedIn           = 2
} EOS_ELoginStatus;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_EOnlineStatus
// ─────────────────────────────────────────────────────────────────────────────
typedef enum EOS_EOnlineStatus
{
    EOS_OS_Offline        = 0,
    EOS_OS_DoNotDisturb   = 1,
    EOS_OS_Away           = 2,
    EOS_OS_Online         = 3
} EOS_EOnlineStatus;

#ifdef __cplusplus
}
#endif
