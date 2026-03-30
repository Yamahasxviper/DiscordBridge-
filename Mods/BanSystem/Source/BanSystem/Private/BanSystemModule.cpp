// Copyright Yamahasxviper. All Rights Reserved.

#include "BanSystemModule.h"

// SML
#include "Command/ChatCommandLibrary.h"

// BanSystem commands
#include "Commands/BanCommands.h"

// UE
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanSystem, Log, All);

#define LOCTEXT_NAMESPACE "FBanSystemModule"

// ─────────────────────────────────────────────────────────────────────────────
//  Module startup
// ─────────────────────────────────────────────────────────────────────────────
void FBanSystemModule::StartupModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module starting up."));

    // ── Ban enforcement ───────────────────────────────────────────────────
    // PreLogin + PostLogin ban enforcement is handled by UBanEnforcementSubsystem
    // (a UGameInstanceSubsystem that initialises automatically).  It subscribes
    // to FGameModeEvents::GameModePreLoginEvent and GameModePostLoginEvent and
    // also uses UEOSConnectSubsystem (from EOSSystem) for async PUID lookups and
    // cross-platform ban propagation.  No module-level hooks are needed here.

    // ── Register admin chat commands once each world is ready ─────────────
    //
    // Hook OnWorldInitializedActors so that AChatCommandSubsystem (an SML
    // world actor) is already spawned and reachable before we call
    // RegisterCommand().  SML registers its own world actors in the same
    // delegate but earlier (SML loads before BanSystem), so the subsystem
    // is available by the time our lambda fires.
    WorldInitHandle = FWorldDelegates::OnWorldInitializedActors.AddLambda(
        [](const UWorld::FActorsInitializedParams& Params)
        {
            UWorld* World = Params.World;
            if (!World) return;

            // Only register commands on server/listen-server worlds.
            const ENetMode NetMode = World->GetNetMode();
            if (NetMode != NM_DedicatedServer && NetMode != NM_ListenServer)
                return;

            AChatCommandSubsystem* CmdSys = AChatCommandSubsystem::Get(World);
            if (!CmdSys)
            {
                UE_LOG(LogBanSystem, Warning,
                    TEXT("BanSystem: AChatCommandSubsystem not available — commands not registered."));
                return;
            }

            // Steam commands — independent set
            CmdSys->RegisterCommand(TEXT("BanSystem"), ASteamBanCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), ASteamUnbanCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), ASteamBanListCommand::StaticClass());

            // EOS commands — independent set
            CmdSys->RegisterCommand(TEXT("BanSystem"), AEOSBanCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), AEOSUnbanCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), AEOSBanListCommand::StaticClass());

            // Cross-platform name-based commands
            CmdSys->RegisterCommand(TEXT("BanSystem"), ABanByNameCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), APlayerIdsCommand::StaticClass());
            CmdSys->RegisterCommand(TEXT("BanSystem"), ACheckBanCommand::StaticClass());

            UE_LOG(LogBanSystem, Log,
                TEXT("BanSystem: Registered 9 ban commands (3 Steam + 3 EOS + banbyname + playerids + checkban)."));
        }
    );

    UE_LOG(LogBanSystem, Log,
        TEXT("BanSystem module started. Command registration scheduled. "
             "Ban enforcement handled by UBanEnforcementSubsystem."));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module shutdown
// ─────────────────────────────────────────────────────────────────────────────
void FBanSystemModule::ShutdownModule()
{
    // Unregister world delegate for command registration.
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    // PreLogin/PostLogin ban enforcement is handled by UBanEnforcementSubsystem
    // which unregisters its own delegates in Deinitialize().  No module-level
    // cleanup is required here.

    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
