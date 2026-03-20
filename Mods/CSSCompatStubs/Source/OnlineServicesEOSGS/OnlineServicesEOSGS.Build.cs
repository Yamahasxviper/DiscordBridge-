// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "OnlineServicesEOSGS" dependency on dedicated-
// server builds.
//
// OnlineServicesEOSGS provides the UE Online Services v2 EOS Game Services
// backend for client-side features (leaderboards, titles, achievements via EOS).
// CSS custom UnrealEngine does not ship OnlineServicesEOSGS in its dedicated-
// server binary. OnlineIntegration.uplugin has "OnlineServicesEOSGS" with
// TargetDenyList: Server, but OnlineIntegrationEOSExtensions.Build.cs references
// it as a private dependency, which may trigger module resolution on server.

public class OnlineServicesEOSGS : ModuleRules
{
	public OnlineServicesEOSGS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.Add("Core");
	}
}
