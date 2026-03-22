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
			// Header stubs for APIs not present in Satisfactory's custom UE build.
			// Required by all Alpakit C++ mods so UBT can resolve engine headers.
			"DummyHeaders",
			// SML runtime – ensures correct module load ordering.
			"SML",
			// DiscordBridge – provides UDiscordBridgeSubsystem, the Discord Gateway
			// connection, and all REST helper methods (RespondToInteraction,
			// DeleteDiscordChannel, CreateDiscordGuildTextChannel, etc.) that the
			// ticket system relies on.
			"DiscordBridge",
			// Unreal HTTP module – confirmed present in Satisfactory's custom UE build.
			"HTTP",
			// JSON serialisation (FJsonObject / TJsonReader / TJsonWriter).
			"Json",
		});
	}
}
