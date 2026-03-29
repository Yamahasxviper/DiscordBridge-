// Copyright Yamahasxviper. All Rights Reserved.
using UnrealBuildTool;

public class EOSSystem : ModuleRules
{
    public EOSSystem(ReadOnlyTargetRules Target) : base(Target)
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
            "DeveloperSettings",
            // Json + JsonUtilities — used in UEOSConnectSubsystem to persist the
            // PUID↔external-account reverse-lookup cache to disk across server restarts.
            // FJsonObject/FJsonSerializer are from Json; FJsonObjectConverter is from JsonUtilities.
            "Json",
            "JsonUtilities",
            // EOSSDK — provides the engine's ThirdParty EOS C SDK headers via angle-bracket
            // includes (<eos_common.h>, <eos_init.h>, <eos_types.h>, etc.).  The EOSSDK
            // module's include path is used by the Public/EOSSDK/ delegation wrappers to
            // forward all type definitions to the canonical engine headers, preventing
            // C2371/C2011/C3431 redefinition conflicts when BanSystem includes both
            // EOSSystem and EOSDirectSDK (which also pulls in real EOSSDK headers via
            // EOSShared) in the same translation unit.
            "EOSSDK",
        });

        PrivateDependencyModuleNames.AddRange(new string[]
        {
            // Projects — used in FEOSSDKLoader::Load() via IPluginManager::Get().FindPlugin()
            // to add the plugin's own Binaries directory as a DLL candidate search path.
            "Projects",
        });
    }
}
