// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "EOSShared" dependency declared in
// OnlineIntegration.Build.cs on dedicated-server Alpakit builds and
// prevents IntelliSense errors in the Editor.
//
// Background
// ----------
// CSS custom UnrealEngine omits EOSShared from its dedicated-server binary
// and does not ship module source for the Editor. FactoryGame.Build.cs
// documents this explicitly:
//   "Exclude EOSShared on dedicated server, it is not enabled as a plugin
//    for the dedicated server so we cannot depend on it"
//
// Without this stub, every WindowsServer / LinuxServer Alpakit build of
// DiscordBridge fails with "cannot find module EOSShared", and VS Code /
// Rider throw "Plugin EOSShared does not contain the EOSShared module"
// when generating IntelliSense include data for FactoryEditor.
//
// TargetDenyList: ["Game"] in CSSCompatStubs.uplugin ensures this stub is
// compiled only for Server and Editor targets. On Game targets, UBT uses the
// real CSS UE EOSShared engine plugin instead.
//
// In addition to satisfying the compile-time dependency, this module installs
// a native hook (via SML's NativeHookManager) that suppresses
// UFGLocalPlayer::RequestPublicPlayerAddress on dedicated servers.
// See Private/EOSSharedModule.cpp for details.

public class EOSShared : ModuleRules
{
	public EOSShared(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			// Required by all Alpakit C++ mods – provides stubs for engine headers
			// that CSS custom UnrealEngine does not ship in its Alpakit packages.
			"DummyHeaders",
			// FactoryGame – provides UFGLocalPlayer whose RequestPublicPlayerAddress
			// method is hooked in StartupModule to suppress the ipify.org request on
			// dedicated servers.
			"FactoryGame",
			// SML runtime – required for NativeHookManager (SUBSCRIBE_METHOD /
			// UNSUBSCRIBE_METHOD macros) used in StartupModule / ShutdownModule.
			"SML",
		});
	}
}
