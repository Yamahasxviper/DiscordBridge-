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
        });
    }
}
