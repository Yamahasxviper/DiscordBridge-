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
            "FactoryGame",
            "SML",
            // SMLWebSocket — for the standalone Discord Gateway (WebSocket) connection.
            "SMLWebSocket",
            // EOSIdHelper — custom plugin providing EOS Product User ID extraction
            // via OnlineServicesEOSGS (V2).  Replaces the DummyHeaders/FGOnlineHelpers
            // dependency which required the disabled OnlineSubsystemEOS plugin.
            "EOSIdHelper",
            // EOSDirectSDK — dedicated module providing direct EOS C SDK access.
            // Exposes EOSDirectSDK::PUIDFromString, PUIDToString, IsValidHandle,
            // GetPlatformHandle (EOS_HPlatform) and GetConnectInterface (EOS_HConnect).
            // Also accessible as EOSBanSDK:: via the alias in EOSBanSDK.h.
            "EOSDirectSDK",
            // EOSSystem — standalone EOS system providing UEOSConnectSubsystem with
            // forward (Steam64→PUID) and reverse (PUID→Steam64) lookup cache.
            // Optional at runtime: used for cross-platform ban ID resolution.
            "EOSSystem",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
        });
    }
}
