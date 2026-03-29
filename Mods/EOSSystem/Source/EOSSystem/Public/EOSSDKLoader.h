// Copyright Yamahasxviper. All Rights Reserved.
//
// FEOSSDKLoader — dynamically loads the EOS C SDK DLL at runtime.
// Zero dependency on the UE engine EOSSDK/EOSShared modules.

#pragma once

#include "CoreMinimal.h"
#include "EOSSDK/eos_sdk.h"

class EOSSYSTEM_API FEOSSDKLoader
{
public:
    static FEOSSDKLoader& Get() { static FEOSSDKLoader I; return I; }

    bool           Load();
    void           Unload();
    bool           IsLoaded() const { return bLoaded; }
    static FString ResultToString(EOS_EResult Result);

    // ── Platform / Init ────────────────────────────────────────────────────
    EOS_Initialize_t                                fp_EOS_Initialize                                = nullptr;
    EOS_Shutdown_t                                  fp_EOS_Shutdown                                  = nullptr;
    EOS_Platform_Create_t                           fp_EOS_Platform_Create                           = nullptr;
    EOS_Platform_Release_t                          fp_EOS_Platform_Release                          = nullptr;
    EOS_Platform_Tick_t                             fp_EOS_Platform_Tick                             = nullptr;
    EOS_Platform_SetLogLevel_t                      fp_EOS_Platform_SetLogLevel                      = nullptr;
    EOS_Logging_SetCallback_t                       fp_EOS_Logging_SetCallback                       = nullptr;
    EOS_EResult_ToString_t                          fp_EOS_EResult_ToString                          = nullptr;
    EOS_ProductUserId_ToString_t                    fp_EOS_ProductUserId_ToString                    = nullptr;
    EOS_ProductUserId_FromString_t                  fp_EOS_ProductUserId_FromString                  = nullptr;
    EOS_ProductUserId_IsValid_t                     fp_EOS_ProductUserId_IsValid                     = nullptr;
    EOS_EpicAccountId_ToString_t                    fp_EOS_EpicAccountId_ToString                    = nullptr;
    EOS_EpicAccountId_IsValid_t                     fp_EOS_EpicAccountId_IsValid                     = nullptr;
    EOS_EpicAccountId_FromString_t                  fp_EOS_EpicAccountId_FromString                  = nullptr;

    // ── Interface getters ─────────────────────────────────────────────────
    EOS_Platform_GetConnectInterface_t              fp_EOS_Platform_GetConnectInterface              = nullptr;
    EOS_Platform_GetAuthInterface_t                 fp_EOS_Platform_GetAuthInterface                 = nullptr;
    EOS_Platform_GetUserInfoInterface_t             fp_EOS_Platform_GetUserInfoInterface             = nullptr;
    EOS_Platform_GetFriendsInterface_t              fp_EOS_Platform_GetFriendsInterface              = nullptr;
    EOS_Platform_GetPresenceInterface_t             fp_EOS_Platform_GetPresenceInterface             = nullptr;
    EOS_Platform_GetSessionsInterface_t             fp_EOS_Platform_GetSessionsInterface             = nullptr;
    EOS_Platform_GetLobbyInterface_t                fp_EOS_Platform_GetLobbyInterface                = nullptr;
    EOS_Platform_GetAchievementsInterface_t         fp_EOS_Platform_GetAchievementsInterface         = nullptr;
    EOS_Platform_GetStatsInterface_t                fp_EOS_Platform_GetStatsInterface                = nullptr;
    EOS_Platform_GetLeaderboardsInterface_t         fp_EOS_Platform_GetLeaderboardsInterface         = nullptr;
    EOS_Platform_GetPlayerDataStorageInterface_t    fp_EOS_Platform_GetPlayerDataStorageInterface    = nullptr;
    EOS_Platform_GetTitleStorageInterface_t         fp_EOS_Platform_GetTitleStorageInterface         = nullptr;
    EOS_Platform_GetAntiCheatServerInterface_t      fp_EOS_Platform_GetAntiCheatServerInterface      = nullptr;
    EOS_Platform_GetSanctionsInterface_t            fp_EOS_Platform_GetSanctionsInterface            = nullptr;
    EOS_Platform_GetMetricsInterface_t              fp_EOS_Platform_GetMetricsInterface              = nullptr;
    EOS_Platform_GetReportsInterface_t              fp_EOS_Platform_GetReportsInterface              = nullptr;
    EOS_Platform_GetEcomInterface_t                 fp_EOS_Platform_GetEcomInterface                 = nullptr;
    EOS_Platform_GetRTCInterface_t                  fp_EOS_Platform_GetRTCInterface                  = nullptr;
    EOS_Platform_GetRTCAdminInterface_t             fp_EOS_Platform_GetRTCAdminInterface             = nullptr;
    EOS_Platform_GetP2PInterface_t                  fp_EOS_Platform_GetP2PInterface                  = nullptr;

    // ── Auth ──────────────────────────────────────────────────────────────
    EOS_Auth_Login_t                                fp_EOS_Auth_Login                                = nullptr;
    EOS_Auth_Logout_t                               fp_EOS_Auth_Logout                               = nullptr;
    EOS_Auth_GetLoginStatus_t                       fp_EOS_Auth_GetLoginStatus                       = nullptr;
    EOS_Auth_GetLoggedInAccountsCount_t             fp_EOS_Auth_GetLoggedInAccountsCount             = nullptr;
    EOS_Auth_GetLoggedInAccountByIndex_t            fp_EOS_Auth_GetLoggedInAccountByIndex            = nullptr;
    EOS_Auth_CopyUserAuthToken_t                    fp_EOS_Auth_CopyUserAuthToken                    = nullptr;
    EOS_Auth_Token_Release_t                        fp_EOS_Auth_Token_Release                        = nullptr;

    // ── Connect ───────────────────────────────────────────────────────────
    EOS_Connect_Login_t                             fp_EOS_Connect_Login                             = nullptr;
    EOS_Connect_CreateUser_t                        fp_EOS_Connect_CreateUser                        = nullptr;
    EOS_Connect_CreateDeviceId_t                    fp_EOS_Connect_CreateDeviceId                    = nullptr;
    EOS_Connect_DeleteDeviceId_t                    fp_EOS_Connect_DeleteDeviceId                    = nullptr;
    EOS_Connect_QueryExternalAccountMappings_t      fp_EOS_Connect_QueryExternalAccountMappings      = nullptr;
    EOS_Connect_GetExternalAccountMapping_t         fp_EOS_Connect_GetExternalAccountMapping         = nullptr;
    EOS_Connect_GetLoginStatus_t                    fp_EOS_Connect_GetLoginStatus                    = nullptr;
    EOS_Connect_GetLoggedInUsersCount_t             fp_EOS_Connect_GetLoggedInUsersCount             = nullptr;
    EOS_Connect_GetLoggedInUserByIndex_t            fp_EOS_Connect_GetLoggedInUserByIndex            = nullptr;
    EOS_Connect_AddNotifyAuthExpiration_t           fp_EOS_Connect_AddNotifyAuthExpiration           = nullptr;
    EOS_Connect_RemoveNotifyAuthExpiration_t        fp_EOS_Connect_RemoveNotifyAuthExpiration        = nullptr;
    EOS_Connect_AddNotifyLoginStatusChanged_t       fp_EOS_Connect_AddNotifyLoginStatusChanged       = nullptr;
    EOS_Connect_RemoveNotifyLoginStatusChanged_t    fp_EOS_Connect_RemoveNotifyLoginStatusChanged    = nullptr;

    // ── UserInfo ──────────────────────────────────────────────────────────
    EOS_UserInfo_QueryUserInfo_t                    fp_EOS_UserInfo_QueryUserInfo                    = nullptr;
    EOS_UserInfo_QueryUserInfoByDisplayName_t       fp_EOS_UserInfo_QueryUserInfoByDisplayName       = nullptr;
    EOS_UserInfo_CopyUserInfo_t                     fp_EOS_UserInfo_CopyUserInfo                     = nullptr;
    EOS_UserInfo_Release_t                          fp_EOS_UserInfo_Release                          = nullptr;

    // ── Friends ───────────────────────────────────────────────────────────
    EOS_Friends_QueryFriends_t                      fp_EOS_Friends_QueryFriends                      = nullptr;
    EOS_Friends_GetFriendsCount_t                   fp_EOS_Friends_GetFriendsCount                   = nullptr;
    EOS_Friends_GetFriendAtIndex_t                  fp_EOS_Friends_GetFriendAtIndex                  = nullptr;
    EOS_Friends_GetStatus_t                         fp_EOS_Friends_GetStatus                         = nullptr;
    EOS_Friends_SendInvite_t                        fp_EOS_Friends_SendInvite                        = nullptr;
    EOS_Friends_AcceptInvite_t                      fp_EOS_Friends_AcceptInvite                      = nullptr;
    EOS_Friends_RejectInvite_t                      fp_EOS_Friends_RejectInvite                      = nullptr;
    EOS_Friends_AddNotifyFriendsUpdate_t            fp_EOS_Friends_AddNotifyFriendsUpdate            = nullptr;
    EOS_Friends_RemoveNotifyFriendsUpdate_t         fp_EOS_Friends_RemoveNotifyFriendsUpdate         = nullptr;

    // ── Presence ──────────────────────────────────────────────────────────
    EOS_Presence_QueryPresence_t                    fp_EOS_Presence_QueryPresence                    = nullptr;
    EOS_Presence_CreatePresenceModification_t       fp_EOS_Presence_CreatePresenceModification       = nullptr;
    EOS_Presence_SetPresence_t                      fp_EOS_Presence_SetPresence                      = nullptr;
    EOS_PresenceModification_SetStatus_t            fp_EOS_PresenceModification_SetStatus            = nullptr;
    EOS_PresenceModification_SetRawRichText_t       fp_EOS_PresenceModification_SetRawRichText       = nullptr;
    EOS_PresenceModification_SetData_t              fp_EOS_PresenceModification_SetData              = nullptr;
    EOS_PresenceModification_Release_t              fp_EOS_PresenceModification_Release              = nullptr;

    // ── Sessions ──────────────────────────────────────────────────────────
    EOS_Sessions_CreateSessionModification_t        fp_EOS_Sessions_CreateSessionModification        = nullptr;
    EOS_Sessions_UpdateSessionModification_t        fp_EOS_Sessions_UpdateSessionModification        = nullptr;
    EOS_Sessions_UpdateSession_t                    fp_EOS_Sessions_UpdateSession                    = nullptr;
    EOS_Sessions_DestroySession_t                   fp_EOS_Sessions_DestroySession                   = nullptr;
    EOS_Sessions_JoinSession_t                      fp_EOS_Sessions_JoinSession                      = nullptr;
    EOS_Sessions_StartSession_t                     fp_EOS_Sessions_StartSession                     = nullptr;
    EOS_Sessions_EndSession_t                       fp_EOS_Sessions_EndSession                       = nullptr;
    EOS_Sessions_RegisterPlayers_t                  fp_EOS_Sessions_RegisterPlayers                  = nullptr;
    EOS_Sessions_UnregisterPlayers_t                fp_EOS_Sessions_UnregisterPlayers                = nullptr;
    EOS_SessionModification_SetBucketId_t           fp_EOS_SessionModification_SetBucketId           = nullptr;
    EOS_SessionModification_SetHostAddress_t        fp_EOS_SessionModification_SetHostAddress        = nullptr;
    EOS_SessionModification_SetMaxPlayers_t         fp_EOS_SessionModification_SetMaxPlayers         = nullptr;
    EOS_SessionModification_SetJoinInProgressAllowed_t fp_EOS_SessionModification_SetJoinInProgressAllowed = nullptr;
    EOS_SessionModification_SetPermissionLevel_t    fp_EOS_SessionModification_SetPermissionLevel    = nullptr;
    EOS_SessionModification_AddAttribute_t          fp_EOS_SessionModification_AddAttribute          = nullptr;
    EOS_SessionModification_RemoveAttribute_t       fp_EOS_SessionModification_RemoveAttribute       = nullptr;
    EOS_SessionModification_Release_t               fp_EOS_SessionModification_Release               = nullptr;

    // ── AntiCheat ─────────────────────────────────────────────────────────
    EOS_AntiCheatServer_BeginSession_t                      fp_EOS_AntiCheatServer_BeginSession                      = nullptr;
    EOS_AntiCheatServer_EndSession_t                        fp_EOS_AntiCheatServer_EndSession                        = nullptr;
    EOS_AntiCheatServer_RegisterConnectedClient_t           fp_EOS_AntiCheatServer_RegisterConnectedClient           = nullptr;
    EOS_AntiCheatServer_UnregisterConnectedClient_t         fp_EOS_AntiCheatServer_UnregisterConnectedClient         = nullptr;
    EOS_AntiCheatServer_ReceiveMessageFromClient_t          fp_EOS_AntiCheatServer_ReceiveMessageFromClient          = nullptr;
    EOS_AntiCheatServer_AddNotifyMessageToClient_t          fp_EOS_AntiCheatServer_AddNotifyMessageToClient          = nullptr;
    EOS_AntiCheatServer_RemoveNotifyMessageToClient_t       fp_EOS_AntiCheatServer_RemoveNotifyMessageToClient       = nullptr;
    EOS_AntiCheatServer_AddNotifyClientActionRequired_t     fp_EOS_AntiCheatServer_AddNotifyClientActionRequired     = nullptr;
    EOS_AntiCheatServer_RemoveNotifyClientActionRequired_t  fp_EOS_AntiCheatServer_RemoveNotifyClientActionRequired  = nullptr;
    EOS_AntiCheatServer_GetProtectMessageOutputLength_t     fp_EOS_AntiCheatServer_GetProtectMessageOutputLength     = nullptr;
    EOS_AntiCheatServer_ProtectMessage_t                    fp_EOS_AntiCheatServer_ProtectMessage                    = nullptr;
    EOS_AntiCheatServer_UnprotectMessage_t                  fp_EOS_AntiCheatServer_UnprotectMessage                  = nullptr;

    // ── Sanctions ─────────────────────────────────────────────────────────
    EOS_Sanctions_QueryActivePlayerSanctions_t      fp_EOS_Sanctions_QueryActivePlayerSanctions      = nullptr;
    EOS_Sanctions_GetPlayerSanctionCount_t          fp_EOS_Sanctions_GetPlayerSanctionCount          = nullptr;
    EOS_Sanctions_CopyPlayerSanctionByIndex_t       fp_EOS_Sanctions_CopyPlayerSanctionByIndex       = nullptr;
    EOS_Sanctions_PlayerSanction_Release_t          fp_EOS_Sanctions_PlayerSanction_Release          = nullptr;

    // ── Stats ─────────────────────────────────────────────────────────────
    EOS_Stats_IngestStat_t                          fp_EOS_Stats_IngestStat                          = nullptr;
    EOS_Stats_QueryStats_t                          fp_EOS_Stats_QueryStats                          = nullptr;
    EOS_Stats_GetStatsCount_t                       fp_EOS_Stats_GetStatsCount                       = nullptr;
    EOS_Stats_CopyStatByIndex_t                     fp_EOS_Stats_CopyStatByIndex                     = nullptr;
    EOS_Stats_CopyStatByName_t                      fp_EOS_Stats_CopyStatByName                      = nullptr;
    EOS_Stats_Stat_Release_t                        fp_EOS_Stats_Stat_Release                        = nullptr;

    // ── Achievements ──────────────────────────────────────────────────────
    EOS_Achievements_QueryPlayerAchievements_t      fp_EOS_Achievements_QueryPlayerAchievements      = nullptr;
    EOS_Achievements_GetPlayerAchievementCount_t    fp_EOS_Achievements_GetPlayerAchievementCount    = nullptr;
    EOS_Achievements_CopyPlayerAchievementByIndex_t fp_EOS_Achievements_CopyPlayerAchievementByIndex = nullptr;
    EOS_Achievements_PlayerAchievement_Release_t    fp_EOS_Achievements_PlayerAchievement_Release    = nullptr;
    EOS_Achievements_UnlockAchievements_t           fp_EOS_Achievements_UnlockAchievements           = nullptr;

    // ── Leaderboards ──────────────────────────────────────────────────────
    EOS_Leaderboards_QueryLeaderboardDefinitions_t      fp_EOS_Leaderboards_QueryLeaderboardDefinitions      = nullptr;
    EOS_Leaderboards_QueryLeaderboardRanks_t            fp_EOS_Leaderboards_QueryLeaderboardRanks            = nullptr;
    EOS_Leaderboards_QueryLeaderboardUserScores_t       fp_EOS_Leaderboards_QueryLeaderboardUserScores       = nullptr;
    EOS_Leaderboards_GetLeaderboardDefinitionCount_t    fp_EOS_Leaderboards_GetLeaderboardDefinitionCount    = nullptr;
    EOS_Leaderboards_CopyLeaderboardDefinitionByIndex_t fp_EOS_Leaderboards_CopyLeaderboardDefinitionByIndex = nullptr;
    EOS_Leaderboards_GetLeaderboardRecordCount_t        fp_EOS_Leaderboards_GetLeaderboardRecordCount        = nullptr;
    EOS_Leaderboards_CopyLeaderboardRecordByIndex_t     fp_EOS_Leaderboards_CopyLeaderboardRecordByIndex     = nullptr;
    EOS_Leaderboards_LeaderboardDefinition_Release_t    fp_EOS_Leaderboards_LeaderboardDefinition_Release    = nullptr;
    EOS_Leaderboards_LeaderboardRecord_Release_t        fp_EOS_Leaderboards_LeaderboardRecord_Release        = nullptr;

    // ── Player Data Storage ───────────────────────────────────────────────
    EOS_PlayerDataStorage_QueryFile_t               fp_EOS_PlayerDataStorage_QueryFile               = nullptr;
    EOS_PlayerDataStorage_QueryFileList_t           fp_EOS_PlayerDataStorage_QueryFileList           = nullptr;
    EOS_PlayerDataStorage_GetFileMetadataCount_t    fp_EOS_PlayerDataStorage_GetFileMetadataCount    = nullptr;
    EOS_PlayerDataStorage_CopyFileMetadataByIndex_t fp_EOS_PlayerDataStorage_CopyFileMetadataByIndex = nullptr;
    EOS_PlayerDataStorage_FileMetadata_Release_t    fp_EOS_PlayerDataStorage_FileMetadata_Release    = nullptr;
    EOS_PlayerDataStorage_ReadFile_t                fp_EOS_PlayerDataStorage_ReadFile                = nullptr;
    EOS_PlayerDataStorage_WriteFile_t               fp_EOS_PlayerDataStorage_WriteFile               = nullptr;
    EOS_PlayerDataStorage_DeleteFile_t              fp_EOS_PlayerDataStorage_DeleteFile              = nullptr;

    // ── Title Storage ─────────────────────────────────────────────────────
    EOS_TitleStorage_QueryFile_t                    fp_EOS_TitleStorage_QueryFile                    = nullptr;
    EOS_TitleStorage_QueryFileList_t                fp_EOS_TitleStorage_QueryFileList                = nullptr;
    EOS_TitleStorage_GetFileMetadataCount_t         fp_EOS_TitleStorage_GetFileMetadataCount         = nullptr;
    EOS_TitleStorage_CopyFileMetadataByIndex_t      fp_EOS_TitleStorage_CopyFileMetadataByIndex      = nullptr;
    EOS_TitleStorage_ReadFile_t                     fp_EOS_TitleStorage_ReadFile                     = nullptr;
    EOS_TitleStorage_FileMetadata_Release_t         fp_EOS_TitleStorage_FileMetadata_Release         = nullptr;

    // ── Metrics ───────────────────────────────────────────────────────────
    EOS_Metrics_BeginPlayerSession_t                fp_EOS_Metrics_BeginPlayerSession                = nullptr;
    EOS_Metrics_EndPlayerSession_t                  fp_EOS_Metrics_EndPlayerSession                  = nullptr;

    // ── Reports ───────────────────────────────────────────────────────────
    EOS_Reports_SendPlayerBehaviorReport_t          fp_EOS_Reports_SendPlayerBehaviorReport          = nullptr;

    // ── Ecom ──────────────────────────────────────────────────────────────
    EOS_Ecom_QueryOwnership_t                       fp_EOS_Ecom_QueryOwnership                       = nullptr;
    EOS_Ecom_QueryEntitlements_t                    fp_EOS_Ecom_QueryEntitlements                    = nullptr;
    EOS_Ecom_GetEntitlementsCount_t                 fp_EOS_Ecom_GetEntitlementsCount                 = nullptr;
    EOS_Ecom_CopyEntitlementByIndex_t               fp_EOS_Ecom_CopyEntitlementByIndex               = nullptr;
    EOS_Ecom_Entitlement_Release_t                  fp_EOS_Ecom_Entitlement_Release                  = nullptr;

private:
    FEOSSDKLoader() = default;
    bool bLoaded    = false;
    void* DllHandle = nullptr;
};
