// Copyright Yamahasxviper. All Rights Reserved.

using UnrealBuildTool;

public class ServerWhitelist : ModuleRules
{
	public ServerWhitelist(ReadOnlyTargetRules Target) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			// FactoryGame – provides AFGChatManager, APlayerController, AGameModeBase.
			"FactoryGame",
			// SML runtime – ensures correct module load ordering on the dedicated server.
			"SML",
			// Unreal JSON serialisation (FJsonObject / TJsonReader / TJsonWriter).
			"Json",
		});
	}
}
