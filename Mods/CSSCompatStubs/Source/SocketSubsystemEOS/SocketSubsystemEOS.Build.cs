// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

// Stub module that satisfies the "SocketSubsystemEOS" dependency on dedicated-
// server and Editor builds.
//
// SocketSubsystemEOS provides the EOS P2P socket subsystem used by client-side
// features (voice chat, relay, peer-to-peer connections via EOS).
// CSS custom UnrealEngine does not ship SocketSubsystemEOS in its dedicated-
// server binary. ReliableMessagingEOSP2P.Build.cs lists SocketSubsystemEOS as
// a private dependency. Although ReliableMessagingEOSP2P has
// TargetDenyList: Server (so it is not loaded on server), UBT may still
// attempt to resolve the SocketSubsystemEOS module name when scanning the full
// plugin dependency graph for server targets.
//
// For Editor targets, the engine SocketSubsystemEOS plugin is denied via
// FactoryGame.uproject (TargetDenyList: ["Server", "Editor"]) to prevent the
// plugin validation chain SocketSubsystemEOS.uplugin -> EOSShared.uplugin
// from failing due to EOSShared's module binary being absent from the CSS
// Editor distribution. This stub ensures UBT can resolve the
// SocketSubsystemEOS module name for ReliableMessagingEOSP2P when compiling
// for Editor, and for any future server-side module that may be added.

public class SocketSubsystemEOS : ModuleRules
{
	public SocketSubsystemEOS(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		CppStandard = CppStandardVersion.Cpp20;
		bLegacyPublicIncludePaths = false;

		PublicDependencyModuleNames.Add("Core");
	}
}
