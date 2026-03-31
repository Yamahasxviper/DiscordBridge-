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
            // FGOnlineHelpers — the standard CSS Satisfactory mod helper module used
            // by every Alpakit C++ mod in this project.  Provides EOSId::GetProductUserId
            // as a non-inline DLL export (FGONLINEHELPERS_API) so that the EOS SDK
            // DLL-import symbols resolve inside FGOnlineHelpers.dll rather than in
            // every consumer's .obj file (which would cause LNK2019).
            "FGOnlineHelpers",
            // EOSDirectSDK — dedicated module providing direct EOS C SDK access.
            // Exposes EOSDirectSDK::PUIDFromString, PUIDToString, IsValidHandle,
            // GetPlatformHandle (EOS_HPlatform) and GetConnectInterface (EOS_HConnect).
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
