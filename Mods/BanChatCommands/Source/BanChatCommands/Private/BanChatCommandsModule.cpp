// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsModule.h"
#include "Command/ChatCommandLibrary.h"
#include "Commands/BanChatCommands.h"
#include "Engine/World.h"

#define LOCTEXT_NAMESPACE "FBanChatCommandsModule"

void FBanChatCommandsModule::StartupModule()
{
    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module starting up."));

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
                UE_LOG(LogBanChatCommands, Warning,
                    TEXT("BanChatCommands: AChatCommandSubsystem not available — commands not registered."));
                return;
            }

            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ATempBanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnbanChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanCheckChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ABanListChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), ALinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AUnlinkBansChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), APlayerHistoryChatCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanChatCommands"), AWhoAmIChatCommand::StaticClass());

            UE_LOG(LogBanChatCommands, Log,
                TEXT("BanChatCommands: Registered 9 commands (ban, tempban, unban, bancheck, banlist, linkbans, unlinkbans, playerhistory, whoami)."));
        }
    );

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module started."));
}

void FBanChatCommandsModule::ShutdownModule()
{
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    UE_LOG(LogBanChatCommands, Log, TEXT("BanChatCommands module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanChatCommandsModule, BanChatCommands)
