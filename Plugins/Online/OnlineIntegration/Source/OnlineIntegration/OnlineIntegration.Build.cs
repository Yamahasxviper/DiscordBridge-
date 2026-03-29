// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class OnlineIntegration : ModuleRules
{
	public OnlineIntegration(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		// <FL> [WuttkeP] Add PLATFORM_PS5 and PLATFORM_XSX defines.
		bAllowConfidentialPlatformDefines = true;
		// </FL>
		bUseUnity = false; // Disable unity because mixing OnlineSubsystem and OnlineServices is no good (but OnlineGameActivityInterface.h only exists on OnlineSubsystem)
		// Suppress C4458 "declaration of 'X' hides class member" in the auto-generated
		// stub implementation files (OnlineFriendPrivate.cpp, OnlinePrivilegeObserver.cpp).
		// Those stubs are machine-generated; rewriting them to avoid shadowing is fragile.
		if (Target.Platform.IsInGroup(UnrealPlatformGroup.Windows))
		{
			AdditionalCompilerArgumentsForCPP += " /wd4458";
		}
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				// ... add other public dependencies that you statically link with here ...
				"OnlineServicesInterface", 
				"Engine", 
				"UMG", 
				"ModelViewViewModel",
				"OnlineServicesInterface",
				"FieldNotification",
				"GameplayEvents",
				"SlateCore",
				"EOSShared" // <FL> [KonradA, BGR] required for advanced EOS logging
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ControlFlows",
				"CoreUObject",
				"Engine",
				"Slate",
				"DeveloperSettings",
				// ... add private dependencies that you statically link with here ...	
				"OnlineServicesCommonEngineUtils", 
				"OnlineSubsystemUtils",
				"GameplayTags", 
				"ApplicationCore", 
				"CoreOnline", 
				"InputCore", 
				"OnlineServicesCommon",
				"OnlineSubsystem"
			});
		
		// <FL> [ZimmermannA] XSX/PS5 platform-specific dependencies not applicable for modding build. 
	}
}
