// Copyright Yamahasxviper. All Rights Reserved.

using UnrealBuildTool;

/**
 * EOSDirectSDK module
 *
 * Dedicated module providing direct EOS C SDK access for Satisfactory server mods.
 *
 * This module is the authoritative source for all direct EOS C SDK helpers:
 *   EOSDirectSDK.h  —  EOSDirectSDK namespace
 *     PUIDFromString()          : FString  →  EOS_ProductUserId  (via EOS_ProductUserId_FromString)
 *     PUIDToString()            : EOS_ProductUserId  →  FString  (via EOS_ProductUserId_ToString)
 *     IsValidHandle()           : EOS_ProductUserId validity (via EOS_ProductUserId_IsValid)
 *     GetPlatformHandle()       : EOS_HPlatform from the module-static registry
 *     RegisterPlatformHandle()  : called by UEOSSystemSubsystem after EOS_Platform_Create
 *     UnregisterPlatformHandle(): called by UEOSSystemSubsystem before EOS_Platform_Release
 *     GetConnectInterface()     : EOS_HConnect via EOS_Platform_GetConnectInterface
 *
 * DESIGN RATIONALE
 * ─────────────────
 * The EOSIdHelper module extracts EOS PUIDs from UE's OnlineServices abstraction
 * layer (FUniqueNetIdRepl → EOS PUID string).  EOSDirectSDK is separate because
 * its concern is different: it provides raw EOS C SDK handle access, enabling any
 * EOS C SDK call (EOS_Connect_*, EOS_Sanctions_*, EOS_UserInfo_*, etc.) to be made
 * against Satisfactory's own EOS platform.
 *
 * GetPlatformHandle() uses a module-static registry populated by
 * UEOSSystemSubsystem — no dependency on IEOSSDKManager, IEOSPlatformHandle,
 * or EOSShared is needed.  This eliminates the recurring C2039 build errors
 * caused by CSS engine variants that ship incompatible or missing EOSShared APIs.
 *
 * DEPENDENCIES (minimal by design)
 * ──────────────────────────────────
 * Only Core and EOSSDK are required.  No UE Online Services or EOSShared layer
 * dependency is needed — this module speaks directly to the EOS C SDK.
 * All public deps flow transitively to any mod that lists "EOSDirectSDK" in its
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

        // Platform functions (GetPlatformHandle, RegisterPlatformHandle,
        // UnregisterPlatformHandle) are defined in EOSDirectSDK.cpp — the module
        // produces a real DLL rather than being header-only.

        PublicDependencyModuleNames.AddRange(new string[]
        {
            // Core — FString, TCHAR_TO_UTF8
            "Core",
            // EOSSDK — all EOS C SDK headers:
            //   eos_common.h  — EOS_ProductUserId, EOS_HPlatform, EOS_Bool,
            //                   EOS_ProductUserId_FromString/IsValid/ToString
            //   eos_platform.h — EOS_Platform_GetConnectInterface, etc.
            "EOSSDK",
        });
    }
}
