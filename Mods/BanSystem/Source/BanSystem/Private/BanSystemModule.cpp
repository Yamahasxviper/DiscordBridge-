// Copyright Yamahasxviper. All Rights Reserved.

#include "BanSystemModule.h"

// SML
#include "Patching/NativeHookManager.h"
#include "Command/ChatCommandLibrary.h"

// FactoryGame
#include "FGGameMode.h"
#include "FGGameSession.h"

// BanSystem
#include "BanIdResolver.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "Commands/BanCommands.h"

// UE
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameSession.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/Engine.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanSystem, Log, All);

#define LOCTEXT_NAMESPACE "FBanSystemModule"

// ─────────────────────────────────────────────────────────────────────────────
//  Module startup
// ─────────────────────────────────────────────────────────────────────────────
void FBanSystemModule::StartupModule()
{
    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module starting up."));

    // ── 1. Install PostLogin after-hook ────────────────────────────────────
    //
    // Fires on the server after every player has successfully connected and
    // had their PlayerController/PlayerState created.
    //
    // ID lookup strategy (see BanIdResolver.h for full documentation):
    //
    //   STEAM:  FUniqueNetIdRepl::GetType() == "Steam"
    //           → ToString() returns the 17-digit Steam64 decimal ID.
    //           → Checked against USteamBanSubsystem.
    //
    //   EOS:    EOSId::GetProductUserId() extracts the 32-char hex PUID from
    //           the opaque UE5 OnlineServices FAccountId embedded in the
    //           FUniqueNetId.  This works for:
    //             • Pure EOS / Epic Games Launcher players
    //             • Steam players whose account is linked to an Epic account
    //               (CSS Satisfactory links Steam + Epic for crossplay)
    //           → Checked against UEOSBanSubsystem.
    //
    //   Both checks run independently — a dual-platform (Steam+EOS) player
    //   can be banned by either their Steam ID or their EOS PUID.
    //
    PostLoginHookHandle = SUBSCRIBE_METHOD_VIRTUAL_AFTER(
        AFGGameMode::PostLogin,
        GetMutableDefault<AFGGameMode>(),
        [](AFGGameMode* Self, APlayerController* NewPlayer)
        {
            if (!NewPlayer || !NewPlayer->PlayerState) return;

            UGameInstance* GI = Self->GetGameInstance();
            if (!GI) return;

            const FUniqueNetIdRepl UniqueId = NewPlayer->PlayerState->GetUniqueNetId();
            if (!UniqueId.IsValid()) return;

            // Resolve platform-specific IDs from the connecting player's
            // UniqueNetId.  Both fields may be populated simultaneously for
            // Steam players who have linked their account to Epic.
            const FResolvedBanId ResolvedId = FBanIdResolver::Resolve(UniqueId);
            if (!ResolvedId.IsValid())
            {
                UE_LOG(LogBanSystem, Verbose,
                    TEXT("PostLogin: no ban-relevant platform ID for type '%s' — skipping ban check."),
                    *FBanIdResolver::GetIdTypeName(UniqueId));
                return;
            }

            // ── Steam ban check (completely independent) ───────────────────
            if (ResolvedId.HasSteamId())
            {
                USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
                if (SteamBans)
                {
                    FString Reason;
                    if (SteamBans->IsPlayerBanned(ResolvedId.Steam64Id, Reason))
                    {
                        UE_LOG(LogBanSystem, Log,
                            TEXT("Kicking Steam-banned player %s — Reason: %s"),
                            *ResolvedId.Steam64Id, *Reason);

                        if (AGameSession* Session = Self->GameSession)
                        {
                            Session->KickPlayer(
                                NewPlayer,
                                FText::FromString(
                                    FString::Printf(TEXT("[Steam Ban] %s"), *Reason)));
                        }
                        // Player is being kicked for a Steam ban.
                        // Still fall through to EOS check — the ban message has
                        // already been delivered; the kick itself is asynchronous.
                    }
                }
            }

            // ── EOS ban check (completely independent) ─────────────────────
            if (ResolvedId.HasEOSPuid())
            {
                UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
                if (EOSBans)
                {
                    FString Reason;
                    if (EOSBans->IsPlayerBanned(ResolvedId.EOSProductUserId, Reason))
                    {
                        UE_LOG(LogBanSystem, Log,
                            TEXT("Kicking EOS-banned player %s — Reason: %s"),
                            *ResolvedId.EOSProductUserId, *Reason);

                        if (AGameSession* Session = Self->GameSession)
                        {
                            Session->KickPlayer(
                                NewPlayer,
                                FText::FromString(
                                    FString::Printf(TEXT("[EOS Ban] %s"), *Reason)));
                        }
                    }
                }
            }
        }
    );

    // ── 2. Register admin chat commands once each world is ready ──────────
    //
    // We hook OnWorldInitializedActors so that AChatCommandSubsystem (an SML
    // world actor) is already spawned and reachable before we call
    // RegisterCommand().  SML registers its own world actors in the same
    // delegate but earlier (SML loads before BanSystem), so the subsystem
    // is available by the time our lambda fires.
    //
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

            UE_LOG(LogBanSystem, Log,
                TEXT("BanSystem: Registered 8 ban commands (3 Steam + 3 EOS + banbyname + playerids)."));
        }
    );

    UE_LOG(LogBanSystem, Log,
        TEXT("BanSystem module started. PostLogin hook active. Command registration scheduled."));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module shutdown
// ─────────────────────────────────────────────────────────────────────────────
void FBanSystemModule::ShutdownModule()
{
    // Unregister world delegate
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    // Unsubscribe PostLogin hook
    UNSUBSCRIBE_METHOD(AFGGameMode::PostLogin, PostLoginHookHandle);

    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
