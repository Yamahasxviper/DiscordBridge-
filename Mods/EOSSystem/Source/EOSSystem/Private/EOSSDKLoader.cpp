// Copyright Yamahasxviper. All Rights Reserved.
#include "EOSSDKLoader.h"
#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSDKLoader, Log, All);

#define LOAD_EOS_FN(Name) \
    fp_##Name = (Name##_t)FPlatformProcess::GetDllExport(DllHandle, TEXT(#Name)); \
    if (!fp_##Name) { UE_LOG(LogEOSSDKLoader, Verbose, TEXT("EOS fn not resolved: " #Name)); } \
    else { ++Resolved; }

bool FEOSSDKLoader::Load()
{
    if (bLoaded) return true;

    // ── Candidate DLL paths ───────────────────────────────────────────────
    TArray<FString> Candidates;
#if PLATFORM_WINDOWS
    Candidates.Add(FPaths::EngineDir() / TEXT("Binaries/Win64/EOSSDK-Win64-Shipping.dll"));
    Candidates.Add(FPaths::ProjectDir() / TEXT("Binaries/Win64/EOSSDK-Win64-Shipping.dll"));
    Candidates.Add(FPlatformProcess::BaseDir() + TEXT("EOSSDK-Win64-Shipping.dll"));
    Candidates.Add(FPaths::ProjectDir() / TEXT("EOSSDK-Win64-Shipping.dll"));
#elif PLATFORM_LINUX
    Candidates.Add(FPaths::EngineDir() / TEXT("Binaries/Linux/libEOSSDK-Linux-Shipping.so"));
    Candidates.Add(FPaths::ProjectDir() / TEXT("Binaries/Linux/libEOSSDK-Linux-Shipping.so"));
    Candidates.Add(FPlatformProcess::BaseDir() + TEXT("libEOSSDK-Linux-Shipping.so"));
#endif

    for (const FString& Path : Candidates)
    {
        DllHandle = FPlatformProcess::GetDllHandle(*Path);
        if (DllHandle)
        {
            UE_LOG(LogEOSSDKLoader, Log, TEXT("EOS SDK DLL loaded from: %s"), *Path);
            break;
        }
    }

    if (!DllHandle)
    {
        UE_LOG(LogEOSSDKLoader, Error, TEXT("EOS SDK DLL not found in any candidate path. EOS will be unavailable."));
        return false;
    }

    int32 Resolved = 0;

    // Platform / Init
    LOAD_EOS_FN(EOS_Initialize)
    LOAD_EOS_FN(EOS_Shutdown)
    LOAD_EOS_FN(EOS_Platform_Create)
    LOAD_EOS_FN(EOS_Platform_Release)
    LOAD_EOS_FN(EOS_Platform_Tick)
    LOAD_EOS_FN(EOS_Platform_SetLogLevel)
    LOAD_EOS_FN(EOS_Logging_SetCallback)
    LOAD_EOS_FN(EOS_EResult_ToString)
    LOAD_EOS_FN(EOS_ProductUserId_ToString)
    LOAD_EOS_FN(EOS_ProductUserId_FromString)
    LOAD_EOS_FN(EOS_ProductUserId_IsValid)
    LOAD_EOS_FN(EOS_EpicAccountId_ToString)
    LOAD_EOS_FN(EOS_EpicAccountId_IsValid)
    LOAD_EOS_FN(EOS_EpicAccountId_FromString)

    // Interface getters
    LOAD_EOS_FN(EOS_Platform_GetConnectInterface)
    LOAD_EOS_FN(EOS_Platform_GetAuthInterface)
    LOAD_EOS_FN(EOS_Platform_GetUserInfoInterface)
    LOAD_EOS_FN(EOS_Platform_GetFriendsInterface)
    LOAD_EOS_FN(EOS_Platform_GetPresenceInterface)
    LOAD_EOS_FN(EOS_Platform_GetSessionsInterface)
    LOAD_EOS_FN(EOS_Platform_GetLobbyInterface)
    LOAD_EOS_FN(EOS_Platform_GetAchievementsInterface)
    LOAD_EOS_FN(EOS_Platform_GetStatsInterface)
    LOAD_EOS_FN(EOS_Platform_GetLeaderboardsInterface)
    LOAD_EOS_FN(EOS_Platform_GetPlayerDataStorageInterface)
    LOAD_EOS_FN(EOS_Platform_GetTitleStorageInterface)
    LOAD_EOS_FN(EOS_Platform_GetAntiCheatServerInterface)
    LOAD_EOS_FN(EOS_Platform_GetSanctionsInterface)
    LOAD_EOS_FN(EOS_Platform_GetMetricsInterface)
    LOAD_EOS_FN(EOS_Platform_GetReportsInterface)
    LOAD_EOS_FN(EOS_Platform_GetEcomInterface)
    LOAD_EOS_FN(EOS_Platform_GetRTCInterface)
    LOAD_EOS_FN(EOS_Platform_GetRTCAdminInterface)
    LOAD_EOS_FN(EOS_Platform_GetP2PInterface)

    // Auth
    LOAD_EOS_FN(EOS_Auth_Login)
    LOAD_EOS_FN(EOS_Auth_Logout)
    LOAD_EOS_FN(EOS_Auth_GetLoginStatus)
    LOAD_EOS_FN(EOS_Auth_GetLoggedInAccountsCount)
    LOAD_EOS_FN(EOS_Auth_GetLoggedInAccountByIndex)
    LOAD_EOS_FN(EOS_Auth_CopyUserAuthToken)
    LOAD_EOS_FN(EOS_Auth_Token_Release)

    // Connect
    LOAD_EOS_FN(EOS_Connect_Login)
    LOAD_EOS_FN(EOS_Connect_CreateUser)
    LOAD_EOS_FN(EOS_Connect_CreateDeviceId)
    LOAD_EOS_FN(EOS_Connect_DeleteDeviceId)
    LOAD_EOS_FN(EOS_Connect_QueryExternalAccountMappings)
    LOAD_EOS_FN(EOS_Connect_GetExternalAccountMapping)
    LOAD_EOS_FN(EOS_Connect_GetLoginStatus)
    LOAD_EOS_FN(EOS_Connect_GetLoggedInUsersCount)
    LOAD_EOS_FN(EOS_Connect_GetLoggedInUserByIndex)
    LOAD_EOS_FN(EOS_Connect_AddNotifyAuthExpiration)
    LOAD_EOS_FN(EOS_Connect_RemoveNotifyAuthExpiration)
    LOAD_EOS_FN(EOS_Connect_AddNotifyLoginStatusChanged)
    LOAD_EOS_FN(EOS_Connect_RemoveNotifyLoginStatusChanged)

    // UserInfo
    LOAD_EOS_FN(EOS_UserInfo_QueryUserInfo)
    LOAD_EOS_FN(EOS_UserInfo_QueryUserInfoByDisplayName)
    LOAD_EOS_FN(EOS_UserInfo_CopyUserInfo)
    LOAD_EOS_FN(EOS_UserInfo_Release)

    // Friends
    LOAD_EOS_FN(EOS_Friends_QueryFriends)
    LOAD_EOS_FN(EOS_Friends_GetFriendsCount)
    LOAD_EOS_FN(EOS_Friends_GetFriendAtIndex)
    LOAD_EOS_FN(EOS_Friends_GetStatus)
    LOAD_EOS_FN(EOS_Friends_SendInvite)
    LOAD_EOS_FN(EOS_Friends_AcceptInvite)
    LOAD_EOS_FN(EOS_Friends_RejectInvite)
    LOAD_EOS_FN(EOS_Friends_AddNotifyFriendsUpdate)
    LOAD_EOS_FN(EOS_Friends_RemoveNotifyFriendsUpdate)

    // Presence
    LOAD_EOS_FN(EOS_Presence_QueryPresence)
    LOAD_EOS_FN(EOS_Presence_CreatePresenceModification)
    LOAD_EOS_FN(EOS_Presence_SetPresence)
    LOAD_EOS_FN(EOS_PresenceModification_SetStatus)
    LOAD_EOS_FN(EOS_PresenceModification_SetRawRichText)
    LOAD_EOS_FN(EOS_PresenceModification_SetData)
    LOAD_EOS_FN(EOS_PresenceModification_Release)

    // Sessions
    LOAD_EOS_FN(EOS_Sessions_CreateSessionModification)
    LOAD_EOS_FN(EOS_Sessions_UpdateSessionModification)
    LOAD_EOS_FN(EOS_Sessions_UpdateSession)
    LOAD_EOS_FN(EOS_Sessions_DestroySession)
    LOAD_EOS_FN(EOS_Sessions_JoinSession)
    LOAD_EOS_FN(EOS_Sessions_StartSession)
    LOAD_EOS_FN(EOS_Sessions_EndSession)
    LOAD_EOS_FN(EOS_Sessions_RegisterPlayers)
    LOAD_EOS_FN(EOS_Sessions_UnregisterPlayers)
    LOAD_EOS_FN(EOS_SessionModification_SetBucketId)
    LOAD_EOS_FN(EOS_SessionModification_SetHostAddress)
    LOAD_EOS_FN(EOS_SessionModification_SetMaxPlayers)
    LOAD_EOS_FN(EOS_SessionModification_SetJoinInProgressAllowed)
    LOAD_EOS_FN(EOS_SessionModification_SetPermissionLevel)
    LOAD_EOS_FN(EOS_SessionModification_AddAttribute)
    LOAD_EOS_FN(EOS_SessionModification_RemoveAttribute)
    LOAD_EOS_FN(EOS_SessionModification_Release)

    // AntiCheat
    LOAD_EOS_FN(EOS_AntiCheatServer_BeginSession)
    LOAD_EOS_FN(EOS_AntiCheatServer_EndSession)
    LOAD_EOS_FN(EOS_AntiCheatServer_RegisterConnectedClient)
    LOAD_EOS_FN(EOS_AntiCheatServer_UnregisterConnectedClient)
    LOAD_EOS_FN(EOS_AntiCheatServer_ReceiveMessageFromClient)
    LOAD_EOS_FN(EOS_AntiCheatServer_AddNotifyMessageToClient)
    LOAD_EOS_FN(EOS_AntiCheatServer_RemoveNotifyMessageToClient)
    LOAD_EOS_FN(EOS_AntiCheatServer_AddNotifyClientActionRequired)
    LOAD_EOS_FN(EOS_AntiCheatServer_RemoveNotifyClientActionRequired)
    LOAD_EOS_FN(EOS_AntiCheatServer_GetProtectMessageOutputLength)
    LOAD_EOS_FN(EOS_AntiCheatServer_ProtectMessage)
    LOAD_EOS_FN(EOS_AntiCheatServer_UnprotectMessage)

    // Sanctions
    LOAD_EOS_FN(EOS_Sanctions_QueryActivePlayerSanctions)
    LOAD_EOS_FN(EOS_Sanctions_GetPlayerSanctionCount)
    LOAD_EOS_FN(EOS_Sanctions_CopyPlayerSanctionByIndex)
    LOAD_EOS_FN(EOS_Sanctions_PlayerSanction_Release)

    // Stats
    LOAD_EOS_FN(EOS_Stats_IngestStat)
    LOAD_EOS_FN(EOS_Stats_QueryStats)
    LOAD_EOS_FN(EOS_Stats_GetStatsCount)
    LOAD_EOS_FN(EOS_Stats_CopyStatByIndex)
    LOAD_EOS_FN(EOS_Stats_CopyStatByName)
    LOAD_EOS_FN(EOS_Stats_Stat_Release)

    // Achievements
    LOAD_EOS_FN(EOS_Achievements_QueryPlayerAchievements)
    LOAD_EOS_FN(EOS_Achievements_GetPlayerAchievementCount)
    LOAD_EOS_FN(EOS_Achievements_CopyPlayerAchievementByIndex)
    LOAD_EOS_FN(EOS_Achievements_PlayerAchievement_Release)
    LOAD_EOS_FN(EOS_Achievements_UnlockAchievements)

    // Leaderboards
    LOAD_EOS_FN(EOS_Leaderboards_QueryLeaderboardDefinitions)
    LOAD_EOS_FN(EOS_Leaderboards_QueryLeaderboardRanks)
    LOAD_EOS_FN(EOS_Leaderboards_QueryLeaderboardUserScores)
    LOAD_EOS_FN(EOS_Leaderboards_GetLeaderboardDefinitionCount)
    LOAD_EOS_FN(EOS_Leaderboards_CopyLeaderboardDefinitionByIndex)
    LOAD_EOS_FN(EOS_Leaderboards_GetLeaderboardRecordCount)
    LOAD_EOS_FN(EOS_Leaderboards_CopyLeaderboardRecordByIndex)
    LOAD_EOS_FN(EOS_Leaderboards_LeaderboardDefinition_Release)
    LOAD_EOS_FN(EOS_Leaderboards_LeaderboardRecord_Release)

    // Player Data Storage
    LOAD_EOS_FN(EOS_PlayerDataStorage_QueryFile)
    LOAD_EOS_FN(EOS_PlayerDataStorage_QueryFileList)
    LOAD_EOS_FN(EOS_PlayerDataStorage_GetFileMetadataCount)
    LOAD_EOS_FN(EOS_PlayerDataStorage_CopyFileMetadataByIndex)
    LOAD_EOS_FN(EOS_PlayerDataStorage_FileMetadata_Release)
    LOAD_EOS_FN(EOS_PlayerDataStorage_ReadFile)
    LOAD_EOS_FN(EOS_PlayerDataStorage_WriteFile)
    LOAD_EOS_FN(EOS_PlayerDataStorage_DeleteFile)

    // Title Storage
    LOAD_EOS_FN(EOS_TitleStorage_QueryFile)
    LOAD_EOS_FN(EOS_TitleStorage_QueryFileList)
    LOAD_EOS_FN(EOS_TitleStorage_GetFileMetadataCount)
    LOAD_EOS_FN(EOS_TitleStorage_CopyFileMetadataByIndex)
    LOAD_EOS_FN(EOS_TitleStorage_ReadFile)
    LOAD_EOS_FN(EOS_TitleStorage_FileMetadata_Release)

    // Metrics
    LOAD_EOS_FN(EOS_Metrics_BeginPlayerSession)
    LOAD_EOS_FN(EOS_Metrics_EndPlayerSession)

    // Reports
    LOAD_EOS_FN(EOS_Reports_SendPlayerBehaviorReport)

    // Ecom
    LOAD_EOS_FN(EOS_Ecom_QueryOwnership)
    LOAD_EOS_FN(EOS_Ecom_QueryEntitlements)
    LOAD_EOS_FN(EOS_Ecom_GetEntitlementsCount)
    LOAD_EOS_FN(EOS_Ecom_CopyEntitlementByIndex)
    LOAD_EOS_FN(EOS_Ecom_Entitlement_Release)

    bLoaded = true;
    UE_LOG(LogEOSSDKLoader, Log, TEXT("EOS SDK loaded — %d functions resolved."), Resolved);
    return true;
}

void FEOSSDKLoader::Unload()
{
    if (DllHandle)
    {
        FPlatformProcess::FreeDllHandle(DllHandle);
        DllHandle = nullptr;
    }
    bLoaded = false;

    // Zero all function pointers
    fp_EOS_Initialize = nullptr; fp_EOS_Shutdown = nullptr;
    fp_EOS_Platform_Create = nullptr; fp_EOS_Platform_Release = nullptr; fp_EOS_Platform_Tick = nullptr;
    fp_EOS_Platform_SetLogLevel = nullptr; fp_EOS_Logging_SetCallback = nullptr;
    fp_EOS_EResult_ToString = nullptr;
    fp_EOS_ProductUserId_ToString = nullptr; fp_EOS_ProductUserId_FromString = nullptr; fp_EOS_ProductUserId_IsValid = nullptr;
    fp_EOS_EpicAccountId_ToString = nullptr; fp_EOS_EpicAccountId_IsValid = nullptr; fp_EOS_EpicAccountId_FromString = nullptr;
    fp_EOS_Platform_GetConnectInterface = nullptr; fp_EOS_Platform_GetAuthInterface = nullptr;
    fp_EOS_Platform_GetUserInfoInterface = nullptr; fp_EOS_Platform_GetFriendsInterface = nullptr;
    fp_EOS_Platform_GetPresenceInterface = nullptr; fp_EOS_Platform_GetSessionsInterface = nullptr;
    fp_EOS_Platform_GetLobbyInterface = nullptr; fp_EOS_Platform_GetAchievementsInterface = nullptr;
    fp_EOS_Platform_GetStatsInterface = nullptr; fp_EOS_Platform_GetLeaderboardsInterface = nullptr;
    fp_EOS_Platform_GetPlayerDataStorageInterface = nullptr; fp_EOS_Platform_GetTitleStorageInterface = nullptr;
    fp_EOS_Platform_GetAntiCheatServerInterface = nullptr; fp_EOS_Platform_GetSanctionsInterface = nullptr;
    fp_EOS_Platform_GetMetricsInterface = nullptr; fp_EOS_Platform_GetReportsInterface = nullptr;
    fp_EOS_Platform_GetEcomInterface = nullptr; fp_EOS_Platform_GetRTCInterface = nullptr;
    fp_EOS_Platform_GetRTCAdminInterface = nullptr; fp_EOS_Platform_GetP2PInterface = nullptr;
    fp_EOS_Auth_Login = nullptr; fp_EOS_Auth_Logout = nullptr; fp_EOS_Auth_GetLoginStatus = nullptr;
    fp_EOS_Auth_GetLoggedInAccountsCount = nullptr; fp_EOS_Auth_GetLoggedInAccountByIndex = nullptr;
    fp_EOS_Auth_CopyUserAuthToken = nullptr; fp_EOS_Auth_Token_Release = nullptr;
    fp_EOS_Connect_Login = nullptr; fp_EOS_Connect_CreateUser = nullptr; fp_EOS_Connect_CreateDeviceId = nullptr;
    fp_EOS_Connect_DeleteDeviceId = nullptr; fp_EOS_Connect_QueryExternalAccountMappings = nullptr;
    fp_EOS_Connect_GetExternalAccountMapping = nullptr; fp_EOS_Connect_GetLoginStatus = nullptr;
    fp_EOS_Connect_GetLoggedInUsersCount = nullptr; fp_EOS_Connect_GetLoggedInUserByIndex = nullptr;
    fp_EOS_Connect_AddNotifyAuthExpiration = nullptr; fp_EOS_Connect_RemoveNotifyAuthExpiration = nullptr;
    fp_EOS_Connect_AddNotifyLoginStatusChanged = nullptr; fp_EOS_Connect_RemoveNotifyLoginStatusChanged = nullptr;
    fp_EOS_UserInfo_QueryUserInfo = nullptr; fp_EOS_UserInfo_QueryUserInfoByDisplayName = nullptr;
    fp_EOS_UserInfo_CopyUserInfo = nullptr; fp_EOS_UserInfo_Release = nullptr;
    fp_EOS_Friends_QueryFriends = nullptr; fp_EOS_Friends_GetFriendsCount = nullptr;
    fp_EOS_Friends_GetFriendAtIndex = nullptr; fp_EOS_Friends_GetStatus = nullptr;
    fp_EOS_Friends_SendInvite = nullptr; fp_EOS_Friends_AcceptInvite = nullptr; fp_EOS_Friends_RejectInvite = nullptr;
    fp_EOS_Friends_AddNotifyFriendsUpdate = nullptr; fp_EOS_Friends_RemoveNotifyFriendsUpdate = nullptr;
    fp_EOS_Presence_QueryPresence = nullptr; fp_EOS_Presence_CreatePresenceModification = nullptr;
    fp_EOS_Presence_SetPresence = nullptr; fp_EOS_PresenceModification_SetStatus = nullptr;
    fp_EOS_PresenceModification_SetRawRichText = nullptr; fp_EOS_PresenceModification_SetData = nullptr;
    fp_EOS_PresenceModification_Release = nullptr;
    fp_EOS_Sessions_CreateSessionModification = nullptr; fp_EOS_Sessions_UpdateSessionModification = nullptr;
    fp_EOS_Sessions_UpdateSession = nullptr; fp_EOS_Sessions_DestroySession = nullptr;
    fp_EOS_Sessions_JoinSession = nullptr; fp_EOS_Sessions_StartSession = nullptr; fp_EOS_Sessions_EndSession = nullptr;
    fp_EOS_Sessions_RegisterPlayers = nullptr; fp_EOS_Sessions_UnregisterPlayers = nullptr;
    fp_EOS_SessionModification_SetBucketId = nullptr; fp_EOS_SessionModification_SetHostAddress = nullptr;
    fp_EOS_SessionModification_SetMaxPlayers = nullptr; fp_EOS_SessionModification_SetJoinInProgressAllowed = nullptr;
    fp_EOS_SessionModification_SetPermissionLevel = nullptr; fp_EOS_SessionModification_AddAttribute = nullptr;
    fp_EOS_SessionModification_RemoveAttribute = nullptr; fp_EOS_SessionModification_Release = nullptr;
    fp_EOS_AntiCheatServer_BeginSession = nullptr; fp_EOS_AntiCheatServer_EndSession = nullptr;
    fp_EOS_AntiCheatServer_RegisterConnectedClient = nullptr; fp_EOS_AntiCheatServer_UnregisterConnectedClient = nullptr;
    fp_EOS_AntiCheatServer_ReceiveMessageFromClient = nullptr; fp_EOS_AntiCheatServer_AddNotifyMessageToClient = nullptr;
    fp_EOS_AntiCheatServer_RemoveNotifyMessageToClient = nullptr; fp_EOS_AntiCheatServer_AddNotifyClientActionRequired = nullptr;
    fp_EOS_AntiCheatServer_RemoveNotifyClientActionRequired = nullptr; fp_EOS_AntiCheatServer_GetProtectMessageOutputLength = nullptr;
    fp_EOS_AntiCheatServer_ProtectMessage = nullptr; fp_EOS_AntiCheatServer_UnprotectMessage = nullptr;
    fp_EOS_Sanctions_QueryActivePlayerSanctions = nullptr; fp_EOS_Sanctions_GetPlayerSanctionCount = nullptr;
    fp_EOS_Sanctions_CopyPlayerSanctionByIndex = nullptr; fp_EOS_Sanctions_PlayerSanction_Release = nullptr;
    fp_EOS_Stats_IngestStat = nullptr; fp_EOS_Stats_QueryStats = nullptr; fp_EOS_Stats_GetStatsCount = nullptr;
    fp_EOS_Stats_CopyStatByIndex = nullptr; fp_EOS_Stats_CopyStatByName = nullptr; fp_EOS_Stats_Stat_Release = nullptr;
    fp_EOS_Achievements_QueryPlayerAchievements = nullptr; fp_EOS_Achievements_GetPlayerAchievementCount = nullptr;
    fp_EOS_Achievements_CopyPlayerAchievementByIndex = nullptr; fp_EOS_Achievements_PlayerAchievement_Release = nullptr;
    fp_EOS_Achievements_UnlockAchievements = nullptr;
    fp_EOS_Leaderboards_QueryLeaderboardDefinitions = nullptr; fp_EOS_Leaderboards_QueryLeaderboardRanks = nullptr;
    fp_EOS_Leaderboards_QueryLeaderboardUserScores = nullptr; fp_EOS_Leaderboards_GetLeaderboardDefinitionCount = nullptr;
    fp_EOS_Leaderboards_CopyLeaderboardDefinitionByIndex = nullptr; fp_EOS_Leaderboards_GetLeaderboardRecordCount = nullptr;
    fp_EOS_Leaderboards_CopyLeaderboardRecordByIndex = nullptr; fp_EOS_Leaderboards_LeaderboardDefinition_Release = nullptr;
    fp_EOS_Leaderboards_LeaderboardRecord_Release = nullptr;
    fp_EOS_PlayerDataStorage_QueryFile = nullptr; fp_EOS_PlayerDataStorage_QueryFileList = nullptr;
    fp_EOS_PlayerDataStorage_GetFileMetadataCount = nullptr; fp_EOS_PlayerDataStorage_CopyFileMetadataByIndex = nullptr;
    fp_EOS_PlayerDataStorage_FileMetadata_Release = nullptr; fp_EOS_PlayerDataStorage_ReadFile = nullptr;
    fp_EOS_PlayerDataStorage_WriteFile = nullptr; fp_EOS_PlayerDataStorage_DeleteFile = nullptr;
    fp_EOS_TitleStorage_QueryFile = nullptr; fp_EOS_TitleStorage_QueryFileList = nullptr;
    fp_EOS_TitleStorage_GetFileMetadataCount = nullptr; fp_EOS_TitleStorage_CopyFileMetadataByIndex = nullptr;
    fp_EOS_TitleStorage_ReadFile = nullptr; fp_EOS_TitleStorage_FileMetadata_Release = nullptr;
    fp_EOS_Metrics_BeginPlayerSession = nullptr; fp_EOS_Metrics_EndPlayerSession = nullptr;
    fp_EOS_Reports_SendPlayerBehaviorReport = nullptr;
    fp_EOS_Ecom_QueryOwnership = nullptr; fp_EOS_Ecom_QueryEntitlements = nullptr;
    fp_EOS_Ecom_GetEntitlementsCount = nullptr; fp_EOS_Ecom_CopyEntitlementByIndex = nullptr;
    fp_EOS_Ecom_Entitlement_Release = nullptr;

    UE_LOG(LogEOSSDKLoader, Log, TEXT("EOS SDK unloaded."));
}

FString FEOSSDKLoader::ResultToString(EOS_EResult Result)
{
    FEOSSDKLoader& SDK = Get();
    if (SDK.bLoaded && SDK.fp_EOS_EResult_ToString)
    {
        const char* Str = SDK.fp_EOS_EResult_ToString(Result);
        if (Str) return UTF8_TO_TCHAR(Str);
    }
    return FString::Printf(TEXT("EOS_EResult(%d)"), static_cast<int32>(Result));
}
