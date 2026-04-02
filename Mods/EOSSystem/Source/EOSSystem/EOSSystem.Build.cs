// Copyright Yamahasxviper. All Rights Reserved.
using UnrealBuildTool;

public class EOSSystem : ModuleRules
{
    public EOSSystem(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        bLegacyPublicIncludePaths = false;

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreUObject",
            "Engine",
            "DeveloperSettings",
            // FUniqueNetIdRepl — used in EOSConnectSubsystem HandlePostLogin/HandleLogout
            // to get the player's replicated network identity.
            "CoreOnline",
            // IOnlineAccountIdRegistry, FAccountId
            "OnlineServicesInterface",
            // Json + JsonUtilities — used in UEOSConnectSubsystem to persist the
            // PUID↔external-account reverse-lookup cache to disk across server restarts.
            // FJsonObject/FJsonSerializer are from Json; FJsonObjectConverter is from JsonUtilities.
            "Json",
            "JsonUtilities",
            // EOSSDK — provides the engine's ThirdParty EOS C SDK headers via angle-bracket
            // includes (<eos_common.h>, <eos_init.h>, <eos_types.h>, etc.).  The EOSSDK
            // module's include path is used by the Public/EOSSDK/ delegation wrappers to
            // forward all type definitions to the canonical engine headers, preventing
            // C2371/C2011/C3431 redefinition conflicts when BanSystem includes both
            // EOSSystem and EOSDirectSDK in the same translation unit.
            "EOSSDK",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Projects — used in FEOSSDKLoader::Load() via IPluginManager::Get().FindPlugin()
            // to add the plugin's own Binaries directory as a DLL candidate search path.
            "Projects",
            // EOSDirectSDK — UEOSSystemSubsystem calls RegisterPlatformHandle /
            // UnregisterPlatformHandle so that EOSDirectSDK::GetPlatformHandle()
            // returns our EOS_HPlatform without any dependency on IEOSSDKManager.
            "EOSDirectSDK",
            // FGOnlineHelpers — provides EOSId::GetProductUserId(FUniqueNetIdRepl),
            // which handles both V1 and V2 (EOSGSS-guarded) extraction internally.
            // Using this module-level helper removes the need for EOSConnectSubsystem
            // to depend on OnlineServicesEOSGS directly (CSS marks EOSGSS as
            // TargetDenyList=["Server"] so it is absent from Linux server builds).
            "FGOnlineHelpers",
            // FactoryGame is required as a private dependency so that EOSConnectSubsystem.cpp
            // can include "GameFramework/OnlineReplStructs.h" for the full FUniqueNetIdRepl
            // definition used in HandlePostLogin and HandleLogout.
            //
            // The CSS Alpakit server distribution does not ship OnlineReplStructs.h as part
            // of the standalone Engine module headers; it is available through FactoryGame's
            // transitive Engine dep.  Private (not Public) so consumers of EOSSystem do not
            // get an unintended FactoryGame transitive dep through this module.
            "FactoryGame",
        });
    }
}
