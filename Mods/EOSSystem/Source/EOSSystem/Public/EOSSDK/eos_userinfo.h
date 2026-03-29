// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_userinfo.h — EOS SDK UserInfo interface, written from scratch using only
// public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.

#pragma once

#include "eos_common.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Forward declare the UserInfo handle (defined in eos_platform.h)
// ─────────────────────────────────────────────────────────────────────────────
struct EOS_UserInfoHandle;
typedef struct EOS_UserInfoHandle* EOS_HUserInfo;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_UserInfo
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_USERINFO_API_LATEST 3

typedef struct EOS_UserInfo
{
    /** API version: must be EOS_USERINFO_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the user */
    EOS_EpicAccountId UserId;
    /** ISO 3166 alpha-2 country code (may be NULL) */
    const char*       Country;
    /** UTF-8 display name (may be NULL) */
    const char*       DisplayName;
    /** IETF BCP 47 preferred language tag (may be NULL) */
    const char*       PreferredLanguage;
    /** Nickname/alias the local user has given this user (may be NULL) */
    const char*       Nickname;
    /** Sanitized display name (may be NULL) */
    const char*       DisplayNameSanitized;
} EOS_UserInfo;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_UserInfo_QueryUserInfoOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_USERINFO_QUERYUSERINFO_API_LATEST 1

typedef struct EOS_UserInfo_QueryUserInfoOptions
{
    /** API version: must be EOS_USERINFO_QUERYUSERINFO_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the requesting user */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the user whose info is requested */
    EOS_EpicAccountId TargetUserId;
} EOS_UserInfo_QueryUserInfoOptions;

typedef struct EOS_UserInfo_QueryUserInfoCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
} EOS_UserInfo_QueryUserInfoCallbackInfo;

typedef void (EOS_CALL *EOS_UserInfo_OnQueryUserInfoCallback)(const EOS_UserInfo_QueryUserInfoCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_UserInfo_CopyUserInfoOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_USERINFO_COPYUSERINFO_API_LATEST 1

typedef struct EOS_UserInfo_CopyUserInfoOptions
{
    /** API version: must be EOS_USERINFO_COPYUSERINFO_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the local user */
    EOS_EpicAccountId LocalUserId;
    /** Epic Account ID of the target user whose info to copy */
    EOS_EpicAccountId TargetUserId;
} EOS_UserInfo_CopyUserInfoOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_UserInfo_QueryUserInfoByDisplayNameOptions / Callback
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST 1

typedef struct EOS_UserInfo_QueryUserInfoByDisplayNameOptions
{
    /** API version: must be EOS_USERINFO_QUERYUSERINFOBYDISPLAYNAME_API_LATEST */
    int32_t           ApiVersion;
    /** Epic Account ID of the requesting user */
    EOS_EpicAccountId LocalUserId;
    /** UTF-8 display name to search for */
    const char*       DisplayName;
} EOS_UserInfo_QueryUserInfoByDisplayNameOptions;

typedef struct EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo
{
    EOS_EResult       ResultCode;
    void*             ClientData;
    EOS_EpicAccountId LocalUserId;
    EOS_EpicAccountId TargetUserId;
    /** The display name that was searched (pointer to internal memory) */
    const char*       DisplayName;
} EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo;

typedef void (EOS_CALL *EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback)(const EOS_UserInfo_QueryUserInfoByDisplayNameCallbackInfo* Data);

// ─────────────────────────────────────────────────────────────────────────────
//  UserInfo interface function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────

/** Queries user info for the given target */
typedef void        (EOS_CALL *EOS_UserInfo_QueryUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoCallback CompletionDelegate);

/** Queries user info for the given display name */
typedef void        (EOS_CALL *EOS_UserInfo_QueryUserInfoByDisplayName_t)(EOS_HUserInfo Handle, const EOS_UserInfo_QueryUserInfoByDisplayNameOptions* Options, void* ClientData, EOS_UserInfo_OnQueryUserInfoByDisplayNameCallback CompletionDelegate);

/** Copies the cached user info into a new allocation; release with EOS_UserInfo_Release */
typedef EOS_EResult (EOS_CALL *EOS_UserInfo_CopyUserInfo_t)(EOS_HUserInfo Handle, const EOS_UserInfo_CopyUserInfoOptions* Options, EOS_UserInfo** OutUserInfo);

/** Releases memory allocated by EOS_UserInfo_CopyUserInfo */
typedef void        (EOS_CALL *EOS_UserInfo_Release_t)(EOS_UserInfo* UserInfo);

#ifdef __cplusplus
}
#endif
