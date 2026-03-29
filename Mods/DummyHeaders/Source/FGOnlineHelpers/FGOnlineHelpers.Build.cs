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
            // NOTE: OnlineSubsystemEOS is intentionally omitted — it is disabled
            // ("Enabled": false) in FactoryGame.uproject and Satisfactory does not
            // use the old V1 OnlineSubsystemEOS path.  Including it would produce
            // LNK1181 (cannot open input file 'UnrealEditor-OnlineSubsystemEOS.lib').
        });
    }
}
