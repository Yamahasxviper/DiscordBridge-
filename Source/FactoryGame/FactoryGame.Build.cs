// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System;
using System.Linq;
using Microsoft.Extensions.Logging;

public class FactoryGame : ModuleRules
{
	public FactoryGame( ReadOnlyTargetRules Target ) : base(Target)
	{
		CppStandard = CppStandardVersion.Cpp20;
		DefaultBuildSettings = BuildSettingsVersion.Latest;

		// We need ICU to be able to convert between charsets for the Server API
		if (Target.bCompileICU)
		{
			PrivateDependencyModuleNames.Add( "ICU" );
		}

		PublicDependencyModuleNames.AddRange( new string[] { 
			"Core", 
			"CoreUObject", 
			"CoreOnline",
			"Engine",
            "InputCore",
			"Landscape",
			"UMG",
			"Slate",
			"SlateCore",
            "AppFramework",
            "AkAudio",
            "WwiseSoundEngine",
            "Foliage",
			"AIModule",
            "Chaos",
            "ChaosSolverEngine",
			"ChaosVehicles",
			"ChaosVehiclesCore",
            "PhysicsCore",
            "Projects",
			"AssetRegistry",
			"Json",
			"JsonUtilities",
            "HTTP",
            "MoviePlayer",
            "NavigationSystem",
			"Sockets",
            "SignificanceManager",
			"EngineSettings",
            "RHI",
			"AnimGraphRuntime",
			"RenderCore",
			"DeveloperSettings",
			"NetCore",
			"CinematicCamera",
			"GameplayCameras",
			"TemplateSequence",
			"IntelISPC",
			"Niagara",
			"EnhancedInput",
			"AbstractInstance",
			"GeometryCollectionEngine",
			"GeometryCore",
			"FieldSystemEngine",
			"ChaosCore",
			"DataflowCore",
			"DataflowEngine",
			"Voronoi",
			"PlanarCut",
			"GeometryCollectionNodes",  
			"InstancedSplinesComponent", 
			"SignificanceISPC",
			"GameplayTags",
			"OnlineSubsystemUtils",
			"OnlineSubsystem",
			"OnlineIntegration",
			"FieldNotification",
			"GameplayEvents",
			"ModelViewViewModel",
			"MovieScene",
			"ReliableMessaging",
			"GeometryFramework",
		} );

		if (Target.Type == TargetType.Server)
		{
			// If the target is a server we define upscaler macros to 0 so we can use them even on server
			PublicDefinitions.Add("USE_XESS=0");
			PublicDefinitions.Add("WITH_DLSS=0");	
		}
		else
		{
			// If the target is not a server we add upscaler modules to the PublicDependencyModuleNames
			// PublicDependencyModuleNames.AddRange( new string[] { 
			// 	"XeSSBlueprint",
			// 	"DLSSBlueprint",
			// 	"StreamlineBlueprint",
			// } );
		}


		//<FL>[KonradA] Binding SceLibPad for windows to use dual sense speakers — not applicable for modding build.
		//</FL>

		// <FL> [PfaffN] EOS does not compile on PS5 or XSX, they are not compatible
		// Game EOS: 4.27
		// PS5 & XSX EOS: 5.1.1
		// [ZolotukhinN:24/01/2024] Exclude EOSShared on dedicated server, it's not enabled as a plugin for the dedicated server so we cannot depend on it
		if (Target.Type != TargetType.Server)
		{
			PublicDependencyModuleNames.AddRange(new string[] {
				"OnlineSubsystemSteam",
			});
		}
		// <FL> [WuttkeP] Additional dependencies for XSX — not applicable for modding build. 
		// <FL> [KohnhorstT] Include DebugUI plugin in non-shipping builds.
		// [PfaffN] Exclude DebugUI from server
		if (Target.Configuration != UnrealTargetConfiguration.Shipping &&
			Target.Type != TargetType.Server)
		{
			// PrivateDependencyModuleNames.AddRange(new string[] {
			// 	"DebugUI",
			// });
		}
		// </FL>
		// <FL> [PfaffN] PS5 WITH_SHOWVAR — not applicable for modding build. 

		PrivateDependencyModuleNames.AddRange( new string[] { 
            "ReplicationGraph",
			"ImageWriteQueue",
			"LevelSequence",
			"ApplicationCore",
			"OpenSSL", 
			"ModularGameplay",
			"OnlineServicesInterface",
			"FactoryPreEarlyLoadingScreen", 
			"PreLoadScreen",
			"OnlineServicesInterface",
			"Niagara",
			"OpenSSL", 
			"SSL", 
			"Gauntlet"
		});

		// Only depend on XESS and DLSS blueprint module for non-dedicated servers
		if (Target.Type != TargetType.Server)
		{
			// PrivateDependencyModuleNames.AddRange( new string[]
			// {
			// 	"XeSSBlueprint",
			// 	"DLSSBlueprint",
			// 	"StreamlineBlueprint",
			// } );
		}

		// <FL> [PfaffN] Disable telemetry for consoles — always disabled for modding build.
		PrivateDefinitions.Add("WITH_TELEMETRY=0");

		bool isPublicBuild = true; // MODDING EDIT: we always target public builds
		string isPublicBuildVersion = System.Environment.GetEnvironmentVariable("IS_PUBLIC_BUILD");
		if( isPublicBuildVersion != null && isPublicBuildVersion.Length > 0 )
		{
			isPublicBuildVersion = isPublicBuildVersion.ToLower();
			if( isPublicBuildVersion == "true" || isPublicBuildVersion == "1" )
			{
				isPublicBuild = true;
			}
		}
		
		// Determine whenever this build should have cheats and public build define set to zero or not
		var publicBuildString = isPublicBuild ? "1" : "0";
		var withCheatsString = !isPublicBuild ? "1" : "0";
		
		PrivateDefinitions.Add($"IS_PUBLIC_BUILD={publicBuildString}");
		PrivateDefinitions.Add($"WITH_CHEATS={withCheatsString}");
		Target.Logger.LogInformation("[{0}] FactoryGame: IsPublicBuild: {1} WithCheats: {2}", Target.Name, publicBuildString, withCheatsString);
		
		if( Target.bBuildEditor == true )
		{
			PublicDependencyModuleNames.AddRange( new string[] {
				"UnrealEd",
				"InputEditor", // <FL>
				"FoliageEdit",
				"AssetTools",
                "ViewportInteraction",
				"SourceControl", // <FL>
				"WwiseResourceCooker",
				"WwiseProjectDatabase",
				"WwiseResourceLoader",
				"DesktopPlatform",
            } );
        }
		
		// MODDING EDIT
		PublicDependencyModuleNames.AddRange(new[] {
			"DummyHeaders",
		});
		
		// MODDING EDIT: Define PLATFORM_PS5=0 and PLATFORM_XSX=0 for modding, we don't have these modules
		PublicDefinitions.Add("PLATFORM_PS5=0");
		PublicDefinitions.Add("PLATFORM_XSX=0");
	}
}
