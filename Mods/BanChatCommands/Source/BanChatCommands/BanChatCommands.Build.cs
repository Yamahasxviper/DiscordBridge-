using UnrealBuildTool;

public class BanChatCommands : ModuleRules
{
    public BanChatCommands(ReadOnlyTargetRules Target) : base(Target)
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
            "FactoryGame",
            "SML",
            // BanSystem — provides UBanDatabase, UBanEnforcer, FBanEntry, FBanTypes.
            "BanSystem",
        });
    }
}
