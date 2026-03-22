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
			// GameplayTags module – required to declare and look up FGameplayTag values.
			"GameplayTags",
			// GameplayEvents plugin (Plugins/GameplayEvents) – used to dispatch and
			// subscribe to loose-coupled Discord bridge events so that other mods can
			// react to Discord connects/disconnects, player join/leave, and Discord
			// messages without depending on DiscordBridgeSubsystem directly.
			"GameplayEvents",
			// ReliableMessaging plugin (Plugins/ReliableMessaging) – allows client-side
			// mods to post messages to Discord by sending a UTF-8 payload over a
			// dedicated reliable channel (EDiscordRelayChannel::ForwardToDiscord) to the
			// server.  DiscordBridge registers a per-player handler on login and forwards
			// received payloads to the main bridged Discord channel(s).
			"ReliableMessaging",
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
			// CSS UnrealEngine-CSS online integration layer (Plugins/Online/OnlineIntegration).
			// Confirmed present: FactoryGame.Build.cs lists "OnlineIntegration" as a
			// public dependency.  UOnlineIntegrationSubsystem and UCommonSessionSubsystem
			// are used by the EOS platform diagnostic command (!server eos) to check
			// whether the CSS online services layer is active at runtime.
			// This avoids any dependency on "OnlineSubsystem" (v1 OSS) whose header
			// availability in the custom CSS engine build is not guaranteed for mods.
			"OnlineIntegration",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// UE Online Services v2 interface layer – provides the declaration of
			// FOnlineIdRegistryRegistry and related Online Services types used in
			// LocalUserInfo.h (FLocalUserNetIdBundle::SetAssociatedAccountId).
			// Kept as a private dependency because it is used only in .cpp files
			// (not in any public DiscordBridge header), and CSS custom UE dedicated-
			// server packages do not always include this as a public header export.
			// OnlineIntegration lists OnlineServicesInterface as a PublicDependency,
			// so its headers are already in scope for private compilation via the
			// OnlineIntegration transitive chain.
			"OnlineServicesInterface",
			// UE Online Services v2 common layer – required to resolve the definition
			// of FOnlineIdRegistryRegistry::Get(), the global singleton implemented in
			// OnlineServicesCommon (not in the interface module).
			// OnlineIntegration.Build.cs lists OnlineServicesCommon as a Private dep,
			// so it is NOT propagated to mod DLLs transitively; declaring it here as
			// a private dep satisfies the linker when building the DiscordBridge .so
			// without exposing the module headers to dependents of DiscordBridge.
			"OnlineServicesCommon",
		});
	}
}
