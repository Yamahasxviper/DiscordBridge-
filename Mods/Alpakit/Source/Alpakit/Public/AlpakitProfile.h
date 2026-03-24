#pragma once
#include "AlpakitSettings.h"

struct FAlpakitProfileGameInfo {
	FAlpakitProfileGameInfo() {}
	FAlpakitProfileGameInfo(bool bInCopyToGame, FDirectoryPath InGamePath, bool bInStartGame, EAlpakitStartGameType InStartGameType):
		bCopyToGame(bInCopyToGame), GamePath(InGamePath), bStartGame(bInStartGame), StartGameType(InStartGameType) {}
	
	bool bCopyToGame{false};
	FDirectoryPath GamePath;
	bool bStartGame{false};
	EAlpakitStartGameType StartGameType{};
	FString CustomLaunchPath;
	FString CustomLaunchArgs;
};

struct FAlpakitProfile {
	explicit FAlpakitProfile(FString InPluginName): BuildConfiguration(EBuildConfiguration::Shipping), PluginName(InPluginName) {}
	
	bool bBuild{false};
	EBuildConfiguration BuildConfiguration;
	TArray<FString> CookedPlatforms;
	FString PluginName;
	/** Full absolute path to the .uplugin file (e.g. C:/Project/Mods/SML/SML.uplugin).
	 *  When set, this path is passed to -DLCName so RunUAT resolves the plugin directly
	 *  rather than searching standard plugin directories. */
	FString PluginPath;
	TMap<FString, FAlpakitProfileGameInfo> PlatformGameInfo;
	bool bMergeArchive{false};

	FString MakeUATCommandLine();
private:
	FString MakeUATPlatformArgs();
};
