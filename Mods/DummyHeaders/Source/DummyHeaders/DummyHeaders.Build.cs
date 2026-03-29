using UnrealBuildTool;

public class DummyHeaders : ModuleRules
{
    public DummyHeaders(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        bLegacyPublicIncludePaths = false;

        // Header-only module — all functionality is provided as inline
        // code in Public headers; there are no private source files.

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
