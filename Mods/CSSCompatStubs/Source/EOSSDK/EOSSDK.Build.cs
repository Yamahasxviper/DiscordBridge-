// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "EOSSDK" dependency on dedicated-server builds.
//
// EOSSDK provides the raw Epic Online Services SDK headers and libraries.
// CSS custom UnrealEngine does not ship EOSSDK in its dedicated-server binary.
// OnlineIntegrationEOSExtensions.Build.cs (a private module inside the CSS UE
// OnlineIntegration plugin) lists EOSSDK as a private dependency. Although
// OnlineIntegrationEOSExtensions has TargetDenyList: Server, UBT may still
// attempt to resolve the EOSSDK module name when scanning the full plugin
// dependency graph for server targets.

public class EOSSDK : ModuleRules
{
	public EOSSDK(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.Add("Core");
	}
}
