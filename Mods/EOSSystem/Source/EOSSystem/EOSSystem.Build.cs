// Copyright Yamahasxviper. All Rights Reserved.
// Standalone EOS System mod — zero dependency on engine EOSSDK / EOSShared.
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
            // Json + JsonUtilities — used in UEOSConnectSubsystem to persist the
            // PUID↔external-account reverse-lookup cache to disk across server restarts.
            // FJsonObject/FJsonSerializer are from Json; FJsonObjectConverter is from JsonUtilities.
            "Json",
            "JsonUtilities",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Projects — used in FEOSSDKLoader::Load() via IPluginManager::Get().FindPlugin()
            // to add the plugin's own Binaries directory as a DLL candidate search path.
            "Projects",
        });
    }
}
