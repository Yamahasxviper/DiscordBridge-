// Copyright Yamahasxviper. All Rights Reserved.

using UnrealBuildTool;

public class EOSIdHelper : ModuleRules
{
    public EOSIdHelper(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        CppStandard = CppStandardVersion.Cpp20;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        bLegacyPublicIncludePaths = false;

        // EOSIdHelper is now a minimal stub module.
        // EOSId::GetProductUserId has been consolidated into the FGOnlineHelpers
        // module, which is the standard Alpakit dependency used by every mod in
        // this project.  EOSIdHelper is retained in the uplugin for compatibility
        // but exports no public API of its own.
        //
        // EOSBanSDK.h is still accessible from this module as a forwarder to
        // EOSDirectSDK.h.  New code should depend on "EOSDirectSDK" directly.

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            // EOSDirectSDK — EOSBanSDK.h (in this module's Public/) includes
            // EOSDirectSDK.h and aliases EOSBanSDK = EOSDirectSDK.  Listing
            // EOSDirectSDK here makes it transitively available to any consumer
            // that still lists "EOSIdHelper" in their deps and includes EOSBanSDK.h.
            "EOSDirectSDK",
        });
    }
}
