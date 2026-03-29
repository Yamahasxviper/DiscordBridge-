// Copyright Yamahasxviper. All Rights Reserved.

using UnrealBuildTool;

public class OnlineSubsystemEOS : ModuleRules
{
	public OnlineSubsystemEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		bLegacyPublicIncludePaths = false;

		// All public types are header-only (IUniqueNetIdEOS, FUniqueNetIdEOS).
		// This module is a compile-time stub that provides the OnlineSubsystemEOS
		// interface for server builds where the real EOS client plugin is absent.

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreOnline",      // FUniqueNetId, FUniqueNetIdRef
			"EOSSDK",          // EOS_ProductUserId, EOS_EpicAccountId, EOS_ProductUserId_IsValid
			"EOSShared",       // LexToString(EOS_ProductUserId) → FString
		});
	}
}
