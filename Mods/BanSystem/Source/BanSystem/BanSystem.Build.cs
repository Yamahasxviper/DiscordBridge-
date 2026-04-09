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
            // CSS dedicated-server game-mode component: UFGGameModeDSComponent::PreLogin
            // is hooked to cache EOS PUIDs from the client Options string before PostLogin.
            // FactoryDedicatedServer is only built for Server+Editor targets, matching
            // this mod's ServerOnly module type.
            "FactoryDedicatedServer",
            // SML patching API — SUBSCRIBE_UOBJECT_METHOD_AFTER for the PreLogin hook.
            "SML",
            // HTTP server — direct port of Tools/BanSystem apiServer.ts
            "HTTPServer",
            // HTTP client utilities (URL encoding/decoding)
            "HTTP",
            // Required for ENetCloseResult symbols referenced by UNetConnection::Close(),
            // which is called in BanEnforcer to hard-disconnect banned players.
            "NetCore",
            // SMLWebSocket — used by UBanWebSocketPusher to push live ban/warn events
            // to a configured WebSocket endpoint when bPushEventsToWebSocket=true.
            "SMLWebSocket",
        });
    }
}
