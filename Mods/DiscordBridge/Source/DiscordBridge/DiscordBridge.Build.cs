// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

public class DiscordBridge : ModuleRules
{
	public DiscordBridge(ReadOnlyTargetRules Target) : base(Target)
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
			// Required by all Alpakit C++ mods so UBT can resolve engine headers at mod compile time.
			"DummyHeaders",
			// FactoryGame – provides AFGChatManager, AFGPlayerController, etc.
			// Declared explicitly even though it is transitively available through SML.
			"FactoryGame",
			// SML runtime dependency – ensures correct module load ordering.
			"SML",
			// Our own SSL-backed WebSocket client plugin (SMLWebSocket mod in this repo).
			// Confirmed available: built alongside this mod by Alpakit.
			"SMLWebSocket",
			// Unreal HTTP module – confirmed present in Satisfactory's custom UE build.
			// Verified: FactoryGame.Build.cs lists "HTTP" in PublicDependencyModuleNames,
			// which makes it transitively available to every SML-dependent mod.
			"HTTP",
			// Unreal JSON serialisation module (FJsonObject / TJsonReader / TJsonWriter /
			// FJsonSerializer) – confirmed present in Satisfactory's custom UE build.
			// Verified: FactoryGame.Build.cs and SML.Build.cs both list "Json" as a
			// public dependency.  Note: "JsonUtilities" (FJsonObjectConverter) is NOT
			// listed here because this module does not use UStruct-to-JSON conversion.
			"Json",
		});
	}
}
