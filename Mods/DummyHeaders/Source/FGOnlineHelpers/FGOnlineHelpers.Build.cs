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
            // OnlineSubsystemEOS provides IUniqueNetIdEOS for the V1 "EOS"-typed
            // FUniqueNetId identity path.
            "OnlineSubsystemEOS",
            // EOSShared provides LexToString(EOS_ProductUserId) -> FString.
            "EOSShared",
            // EOSSDK provides EOS_ProductUserId, EOS_ProductUserId_IsValid, etc.
            "EOSSDK",
        });
    }
}
