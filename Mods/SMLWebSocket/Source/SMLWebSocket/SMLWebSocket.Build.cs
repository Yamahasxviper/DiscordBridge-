// Copyright Coffee Stain Studios. All Rights Reserved.

using UnrealBuildTool;

public class SMLWebSocket : ModuleRules
{
	public SMLWebSocket(ReadOnlyTargetRules Target) : base(Target)
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
			// SML runtime dependency – ensures correct module load ordering
			"SML",
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			// FSocket / ISocketSubsystem are only used in private implementation files
			// (SMLWebSocketRunnable.cpp), so Sockets is a private dependency.
			"Sockets",
		});

		// SSL and OpenSSL are only available – and only needed – on the two
		// dedicated-server platforms that Satisfactory supports.  Guarding the
		// dependency prevents accidental inclusion on unsupported platforms.
		if (Target.Platform == UnrealTargetPlatform.Win64 ||
		    Target.Platform == UnrealTargetPlatform.Linux)
		{
			PrivateDependencyModuleNames.AddRange(new string[]
			{
				// SSL module provides Unreal's SSL abstraction and links OpenSSL libs.
				"SSL",
				// OpenSSL provides raw OpenSSL headers (ssl.h, sha.h, bio.h, etc.)
				"OpenSSL",
			});
		}
	}
}
