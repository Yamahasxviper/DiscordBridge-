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

        // ── BanSystem is ServerOnly — it ONLY ever builds for TargetType.Server ────
        //
        // OnlineServicesEOSGS (CSS TargetDenyList=["Server"]) is absent from the
        // dedicated-server distribution and must NEVER be listed as a dep here.
        // BanIdResolver.cpp contains a WITH_ONLINE_SERVICES_EOSGSS-guarded V2 EOS
        // extraction block that is permanently dead code for this module type.
        // EOS PUIDs are obtained server-side exclusively via the async
        // EOSConnectSubsystem::LookupPUIDBySteam64 path (raw EOS C SDK query).
        //
        // The EOSSDK / EOSShared transitive deps needed by BanIdResolver.cpp at
        // compile time (for "EOSSDK/eos_sdk.h" includes in EOSConnectSubsystem.h)
        // flow in transitively through EOSSystem's public deps — no explicit listing
        // is needed here.
        PublicDefinitions.Add("WITH_ONLINE_SERVICES_EOSGSS=0");

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
        });
    }
}
