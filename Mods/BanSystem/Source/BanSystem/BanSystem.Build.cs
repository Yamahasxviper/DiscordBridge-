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
            // Required for GameFramework/OnlineReplStructs.h (FUniqueNetIdRepl).
            // The CSS Alpakit server distribution does not ship OnlineReplStructs.h
            // through the standalone Engine dep — it is only reachable via FactoryGame's
            // transitive Engine include paths.
            "FactoryGame",
            // HTTP server — direct port of Tools/BanSystem apiServer.ts
            "HTTPServer",
            // HTTP client utilities (URL encoding/decoding)
            "HTTP",
            // Required for ENetCloseResult symbols referenced by UNetConnection::Close(),
            // which is called in BanEnforcer to hard-disconnect banned players.
            "NetCore",
        });
    }
}
