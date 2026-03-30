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

        // Public headers provided by this module (all inline — zero DLL overhead):
        //   EOSIdHelper.h  — EOSId::GetProductUserId()  : extract EOS PUID from FUniqueNetIdRepl
        //   EOSBanSDK.h    — compatibility forwarder: includes EOSDirectSDK.h and aliases
        //                    EOSBanSDK namespace to EOSDirectSDK namespace.
        //                    New code should depend on "EOSDirectSDK" and use EOSDirectSDK.h.
        //
        // All public deps below flow transitively to any mod that lists
        // "EOSIdHelper" in its own PublicDependencyModuleNames.

        PublicDependencyModuleNames.AddRange(new string[]
        {
            "Core",
            // FUniqueNetIdRepl, FAccountId and the V2 IsV2/GetV2Unsafe helpers
            "CoreOnline",
            // IOnlineAccountIdRegistry, EOnlineServices
            "OnlineServicesInterface",
            // UE::Online::GetProductUserId(FAccountId) — the Satisfactory V2 EOS path.
            // Satisfactory ships OnlineServicesEOSGS exclusively; the old V1
            // OnlineSubsystemEOS is disabled in FactoryGame.uproject.
            "OnlineServicesEOSGS",
            // LexToString(EOS_ProductUserId) -> FString
            "EOSShared",
            // EOS_ProductUserId, EOS_ProductUserId_IsValid, EOS_ProductUserId_FromString,
            // eos_common.h, eos_platform.h and all EOS C SDK headers
            "EOSSDK",
            // EOSDirectSDK — dedicated direct EOS C SDK access module.
            // EOSBanSDK.h in this module includes EOSDirectSDK.h and aliases
            // EOSBanSDK = EOSDirectSDK, so consumers of EOSIdHelper that include
            // EOSBanSDK.h automatically have access to both namespaces.
            "EOSDirectSDK",
        });

        // ── V1 EOS path (OnlineSubsystemEOS) ─────────────────────────────────
        // OnlineSubsystemEOS is currently "Enabled": false in FactoryGame.uproject,
        // so its .lib is never produced.  Listing it as a dependency causes:
        //   LNK1181: cannot open input file 'UnrealEditor-OnlineSubsystemEOS.lib'
        //
        // WITH_EOS_SUBSYSTEM_V1 defaults to 0 (disabled).
        // To re-enable the V1 IUniqueNetIdEOS code path when OnlineSubsystemEOS
        // becomes available:
        //   1. Set OnlineSubsystemEOS "Enabled": true in FactoryGame.uproject
        //   2. Change PublicDefinitions.Add below to "WITH_EOS_SUBSYSTEM_V1=1"
        //   3. Uncomment the PublicDependencyModuleNames.Add("OnlineSubsystemEOS") line
        PublicDefinitions.Add("WITH_EOS_SUBSYSTEM_V1=0");
        // PublicDependencyModuleNames.Add("OnlineSubsystemEOS");
    }
}
