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
            "JsonUtilities",
            "DummyHeaders",
            "FactoryGame",
            "SML",
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
            // SMLWebSocket — for the standalone Discord Gateway (WebSocket) connection.
            "SMLWebSocket",
            // DiscordBridge — optional add-on integration.
            // When DiscordBridge is installed, BanSystem shares its existing Discord bot
            // connection instead of opening a second Gateway.  Guarded at runtime with
            // FModuleManager::IsModuleLoaded("DiscordBridge") so BanSystem still works
            // standalone when DiscordBridge is not installed.
            "DiscordBridge",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
        });
    }
}
