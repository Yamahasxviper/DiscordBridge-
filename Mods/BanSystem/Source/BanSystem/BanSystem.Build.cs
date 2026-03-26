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
            "NetCore",
            "Json",
            "DummyHeaders",
            "FactoryGame",
            "SML",
            // SMLWebSocket — for the standalone Discord Gateway (WebSocket) connection.
            "SMLWebSocket",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
        });
    }
}
