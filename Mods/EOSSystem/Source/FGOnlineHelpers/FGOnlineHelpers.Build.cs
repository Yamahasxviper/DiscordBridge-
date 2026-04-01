using UnrealBuildTool;

public class FGOnlineHelpers : ModuleRules
{
    public FGOnlineHelpers(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        bLegacyPublicIncludePaths = false;

        // All inline functionality is provided in Public headers; FGOnlineHelpers.cpp
        // contains only the minimal IMPLEMENT_MODULE required to produce a loadable DLL.

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            "CoreOnline",              // FUniqueNetIdRepl, FAccountId (V1/V2)
            "OnlineServicesInterface", // EOnlineServices, IOnlineAccountIdRegistry
            // EOSSDK provides EOS_ProductUserId, EOS_ProductUserId_IsValid, etc.
            "EOSSDK",
        });

        // OnlineServicesEOSGS provides UE::Online::GetProductUserId(FAccountId)
        // for the V2 (FAccountId) EOS identity path.
        // CSS marks this plugin with TargetDenyList=["Server"] in OnlineIntegration.uplugin,
        // so libFactoryServer-OnlineServicesEOSGS-Linux-Shipping.so is absent from the
        // dedicated server distribution.  Guard the dependency and the V2 code path with
        // WITH_ONLINE_SERVICES_EOSGSS so non-server (game/editor) builds keep the V2 path
        // while server builds compile cleanly without the missing library.
        //
        // EOSShared provides LexToString(EOS_ProductUserId) -> FString, which is only
        // called inside the WITH_ONLINE_SERVICES_EOSGSS path (V2) and WITH_EOS_SUBSYSTEM_V1
        // path (V1).  CSS explicitly documents that EOSShared is not enabled as a plugin
        // for the dedicated server (FactoryGame.Build.cs comment:
        // "[ZolotukhinN:24/01/2024] Exclude EOSShared on dedicated server, it's not
        //  enabled as a plugin for the dedicated server so we cannot depend on it").
        // Guard alongside OnlineServicesEOSGS to avoid a missing-library link error on
        // the Linux dedicated server.
        if (Target.Type != TargetType.Server)
        {
            PublicDependencyModuleNames.Add("OnlineServicesEOSGS");
            PublicDependencyModuleNames.Add("EOSShared");
            PublicDefinitions.Add("WITH_ONLINE_SERVICES_EOSGSS=1");
        }
        else
        {
            PublicDefinitions.Add("WITH_ONLINE_SERVICES_EOSGSS=0");
        }

        // ── V1 EOS path (OnlineSubsystemEOS) ─────────────────────────────────
        // OnlineSubsystemEOS is currently "Enabled": false in FactoryGame.uproject,
        // so its .lib is never produced.  Listing it causes LNK1181.
        // WITH_EOS_SUBSYSTEM_V1 defaults to 0 (disabled).
        // To re-enable when OnlineSubsystemEOS becomes available:
        //   1. Set "OnlineSubsystemEOS" Enabled: true in FactoryGame.uproject
        //   2. Change the define below to "WITH_EOS_SUBSYSTEM_V1=1"
        //   3. Uncomment the PublicDependencyModuleNames.Add line below
        PublicDefinitions.Add("WITH_EOS_SUBSYSTEM_V1=0");
        // PublicDependencyModuleNames.Add("OnlineSubsystemEOS");
    }
}
