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
            "CoreOnline",       // FUniqueNetIdRepl, FAccountId V1/V2 (IsV2, GetV2Unsafe)
            "Engine",
            "NetCore",
            "Json",
            "FactoryGame",
            "SML",
            // SMLWebSocket — for the standalone Discord Gateway (WebSocket) connection.
            "SMLWebSocket",
            // OnlineServicesInterface — EOnlineServices, IOnlineAccountIdRegistry
            // (standard UE5; required for FAccountId operations in the EOS V2 path).
            "OnlineServicesInterface",
            // EOSSystem — standalone mod providing UEOSConnectSubsystem with
            // forward (Steam64→PUID) and reverse (PUID→Steam64) async lookup and cache.
            // Optional at runtime (all call sites guard for nullptr); required at
            // compile time for the cross-platform ban-propagation code paths.
            "EOSSystem",
        });

        // ── EOS Product User ID extraction (non-server only) ──────────────────
        // OnlineServicesEOSGS provides UE::Online::GetProductUserId(FAccountId)
        // for the V2 (FAccountId) EOS identity path.  CSS marks this plugin with
        // TargetDenyList=["Server"] in OnlineIntegration.uplugin, so the server
        // distribution does not ship the library.  Guard the dependency and all
        // V2 EOS extraction code behind WITH_ONLINE_SERVICES_EOSGSS so server
        // builds compile cleanly without the missing library.
        //
        // EOSShared provides LexToString(EOS_ProductUserId) -> FString.
        // EOSSDK provides the EOS C SDK types (EOS_ProductUserId, EOS_ProductUserId_IsValid).
        //
        // All calls to these APIs are confined to BanIdResolver.cpp (a single
        // translation unit) so the DLL-import stubs are emitted only once and no
        // LNK2019 unresolved-external errors arise in other .obj files.
        if (Target.Type != TargetType.Server)
        {
            PublicDependencyModuleNames.AddRange(new string[]
            {
                "OnlineServicesEOSGS",
                "EOSShared",
                "EOSSDK",
            });
            PublicDefinitions.Add("WITH_ONLINE_SERVICES_EOSGSS=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_ONLINE_SERVICES_EOSGSS=0");
        }

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // HTTP — for the standalone Discord REST API (channel messages).
            "HTTP",
        });
    }
}
