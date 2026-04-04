// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

public class TicketSystem : ModuleRules
{
	public TicketSystem(ReadOnlyTargetRules Target) : base(Target)
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
			// SML runtime – ensures correct module load ordering.
			"SML",
			// Custom WebSocket client with SSL support for the standalone Discord
			// Gateway connection (UTicketDiscordProvider).
			"SMLWebSocket",
			// HTTP client used by the standalone Discord REST API calls.
			"HTTP",
			// JSON serialisation (FJsonObject / TJsonReader / TJsonWriter).
			"Json",
		});
	}
}
