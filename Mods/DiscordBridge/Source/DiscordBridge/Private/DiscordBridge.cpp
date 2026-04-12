// Copyright Yamahasxviper. All Rights Reserved.

#include "DiscordBridge.h"
#include "DiscordBridgeChatCommands.h"
#include "Modules/ModuleManager.h"
#include "Engine/World.h"
#include "Command/ChatCommandLibrary.h"
#include "Engine/Engine.h"

void FDiscordBridgeModule::StartupModule()
{
	// Register in-game slash commands (/verify, /discord, /ingamewhitelist) once
	// the server world has fully initialised its actors (same pattern as BanChatCommands).
	WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda(
		[](const UWorld::FActorsInitializedParams& Params)
		{
			UWorld* World = Params.World;
			if (!World) return;

			const ENetMode NetMode = World->GetNetMode();
			if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer)
				return;

			AChatCommandSubsystem* CmdSys = AChatCommandSubsystem::Get(World);
			if (!CmdSys)
			{
				UE_LOG(LogTemp, Warning,
					TEXT("DiscordBridge: AChatCommandSubsystem not available — /verify, /discord, /ingamewhitelist, /commands not registered."));
				return;
			}

			CmdSys->RegisterCommand(TEXT("DiscordBridge"), AVerifyDiscordChatCommand::StaticClass());
			CmdSys->RegisterCommand(TEXT("DiscordBridge"), ADiscordInviteChatCommand::StaticClass());
			CmdSys->RegisterCommand(TEXT("DiscordBridge"), AInGameWhitelistChatCommand::StaticClass());
			CmdSys->RegisterCommand(TEXT("DiscordBridge"), ADiscordCommandsListChatCommand::StaticClass());

			UE_LOG(LogTemp, Log,
				TEXT("DiscordBridge: Registered in-game slash commands: /verify, /discord, /ingamewhitelist, /commands."));
		});
}

void FDiscordBridgeModule::ShutdownModule()
{
	FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
	WorldInitHandle.Reset();
}

IMPLEMENT_MODULE(FDiscordBridgeModule, DiscordBridge)
