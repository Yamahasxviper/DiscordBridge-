// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "OnlineServicesEOS" dependency on dedicated-
// server builds.
//
// OnlineServicesEOS provides the UE Online Services v2 EOS backend for
// client-side features (authentication, friends, presence, sessions over EOS).
// CSS custom UnrealEngine does not ship OnlineServicesEOS in its dedicated-
// server binary. OnlineIntegration.uplugin has "OnlineServicesEOS" with
// TargetDenyList: Server (so it is not loaded on server), but
// OnlineIntegrationEOSExtensions.Build.cs references it as a private dependency.
// This stub ensures UBT can resolve the module name when scanning the full
// dependency graph for server Alpakit builds.

public class OnlineServicesEOS : ModuleRules
{
	public OnlineServicesEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.Add("Core");
		PublicDependencyModuleNames.Add("DummyHeaders");
	}
}
