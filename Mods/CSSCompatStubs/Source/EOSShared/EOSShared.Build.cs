// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "EOSShared" dependency declared in
// OnlineIntegration.Build.cs on dedicated-server Alpakit builds.
//
// Background
// ----------
// CSS custom UnrealEngine omits EOSShared from its dedicated-server binary.
// FactoryGame.Build.cs documents this explicitly:
//   "Exclude EOSShared on dedicated server, it is not enabled as a plugin
//    for the dedicated server so we cannot depend on it"
//
// Without this stub, every WindowsServer / LinuxServer Alpakit build of
// DiscordBridge fails with "cannot find module EOSShared".
//
// TargetDenyList: ["Game"] in CSSCompatStubs.uplugin ensures this stub is
// compiled only for Server targets. On Game targets, UBT uses the real
// CSS UE EOSShared engine plugin instead.

public class EOSShared : ModuleRules
{
	public EOSShared(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("DummyHeaders");
	}
}
