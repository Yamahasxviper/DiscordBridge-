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
            // Json — MuteRegistry.cpp and PlayerNoteRegistry.cpp use FJsonObject /
            // FJsonSerializer directly. Declared explicitly here rather than relying
            // on it being a transitive public dep of BanSystem, so this module
            // continues to compile if BanSystem ever moves Json to PrivateDeps.
            "Json",
            // HTTP — BanChatCommands.cpp uses FHttpModule::Get().CreateRequest() for
            // the ReloadConfigWebhookUrl notification. Declared explicitly rather
            // than relying on FactoryGame's transitive HTTP export.
            "HTTP",
        });
    }
}
