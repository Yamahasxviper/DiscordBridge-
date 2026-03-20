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

		// SSL and OpenSSL are needed on both platforms that Satisfactory builds for:
		//   Win64 – covers the Windows game client (FactoryGameEGS / FactoryGameSteam,
		//           TargetType.Game) AND the Windows dedicated server (FactoryServer,
		//           TargetType.Server).  PLATFORM_WINDOWS=1 in both cases so all three
		//           Alpakit targets "Windows", "WindowsServer" are handled here.
		//   Linux – covers the Linux dedicated server (FactoryServer, TargetType.Server).
		//           This is the "LinuxServer" Alpakit target.
		// No other platforms are used by Satisfactory's current build matrix.
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
