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
            // EOSSystem — standalone mod providing UEOSConnectSubsystem with
            // forward (Steam64→PUID) and reverse (PUID→Steam64) async lookup and cache.
            // Optional at runtime (all call sites guard for nullptr); required at
            // compile time for the cross-platform ban-propagation code paths.
            "EOSSystem",
        });

        // ── BanSystem builds for both Editor and Server targets ──────────────────
        //
        // On dedicated-server builds, OnlineServicesEOSGS (CSS TargetDenyList=["Server"])
        // is absent and BanSystem never calls EOSGSS symbols directly.  BanIdResolver
        // delegates all EOS Product User ID extraction to EOSId::GetProductUserId
        // (FACTORYGAME_API, from Source/FactoryGame/Public/Online/FGOnlineHelpers.h),
        // which handles all platform guards internally — keeping BanSystem free of
        // any direct EOSGSS link dependency regardless of target type.
        //
        // EOS PUIDs are obtained server-side exclusively via the async
        // EOSConnectSubsystem::LookupPUIDBySteam64 path (raw EOS C SDK query).

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
        });
    }
}
