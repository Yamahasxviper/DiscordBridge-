using UnrealBuildTool;

public class BanSystem : ModuleRules
{
    public BanSystem(ReadOnlyTargetRules Target) : base(Target)
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
            "Json",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP server — direct port of Tools/BanSystem apiServer.ts
            "HTTPServer",
            // HTTP client utilities (URL encoding/decoding)
            "HTTP",
        });
    }
}
