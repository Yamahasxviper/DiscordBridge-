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
            // OnlineServicesEOSGS provides UE::Online::GetProductUserId(FAccountId)
            // for the V2 (FAccountId) EOS identity path.
            "OnlineServicesEOSGS",
            // EOSShared provides LexToString(EOS_ProductUserId) -> FString.
            "EOSShared",
            // EOSSDK provides EOS_ProductUserId, EOS_ProductUserId_IsValid, etc.
            "EOSSDK",
        });

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
