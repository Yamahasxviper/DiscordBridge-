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

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // IPluginManager — used to find the plugin's own base directory so we
            // can restore documentation comments in DefaultBanChatCommands.ini at
            // startup (UE's staging pipeline strips comments from Default*.ini via
            // FConfigFile).
            "Projects",
        });
    }
}
