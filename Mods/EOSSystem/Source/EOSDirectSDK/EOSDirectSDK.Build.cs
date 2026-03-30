// Copyright Yamahasxviper. All Rights Reserved.

using UnrealBuildTool;

/**
 * EOSDirectSDK module
 *
 * Dedicated module providing direct EOS C SDK access for Satisfactory server mods.
 *
 * This module is the authoritative source for all direct EOS C SDK helpers:
 *   EOSDirectSDK.h  —  EOSDirectSDK namespace
 *     PUIDFromString()       : FString  →  EOS_ProductUserId  (via EOS_ProductUserId_FromString)
 *     PUIDToString()         : EOS_ProductUserId  →  FString  (via LexToString)
 *     IsValidHandle()        : EOS_ProductUserId validity (via EOS_ProductUserId_IsValid)
 *     GetPlatformHandle()    : EOS_HPlatform from IEOSSDKManager (engine-owned platform)
 *     GetConnectInterface()  : EOS_HConnect via EOS_Platform_GetConnectInterface
 *
 * DESIGN RATIONALE
 * ─────────────────
 * The EOSIdHelper module extracts EOS PUIDs from UE's OnlineServices abstraction
 * layer (FUniqueNetIdRepl → EOS PUID string).  EOSDirectSDK is separate because
 * its concern is different: it provides raw EOS C SDK handle access, enabling any
 * EOS C SDK call (EOS_Connect_*, EOS_Sanctions_*, EOS_UserInfo_*, etc.) to be made
 * against Satisfactory's live EOS platform without initialising a second instance.
 *
 * DEPENDENCIES (minimal by design)
 * ──────────────────────────────────
 * Only Core, EOSSDK, and EOSShared are required.  No UE Online Services layer
 * dependency is needed — this module speaks directly to the EOS C SDK.
 * All deps flow transitively to any mod that lists "EOSDirectSDK" in its
 * PublicDependencyModuleNames.
 */
public class EOSDirectSDK : ModuleRules
{
    public EOSDirectSDK(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        bLegacyPublicIncludePaths = false;

        // All functionality is provided in EOSDirectSDK.h as inline functions —
        // zero runtime overhead, no additional DLL calls.
        // EOSDirectSDK.cpp contains only the IMPLEMENT_MODULE stub required to
        // produce a loadable DLL.

        PublicDependencyModuleNames.AddRange(new string[]
        {
            // Core — FString, TCHAR_TO_UTF8, TArray, TSharedRef
            "Core",
            // EOSShared — provides:
            //   LexToString(EOS_ProductUserId)  →  FString
            //   IEOSSDKManager::Get()           →  singleton access to engine EOS platforms
            //   IEOSPlatformHandle              →  wraps EOS_HPlatform (public member: PlatformHandle)
            "EOSShared",
            // EOSSDK — all EOS C SDK headers:
            //   eos_common.h    — EOS_ProductUserId, EOS_HPlatform, EOS_Bool, EOS_TRUE/FALSE
            //   eos_platform.h  — EOS_Platform_GetConnectInterface, EOS_Platform_GetUserInfoInterface
            //   EOS_ProductUserId_FromString, EOS_ProductUserId_IsValid
            "EOSSDK",
        });
    }
}
