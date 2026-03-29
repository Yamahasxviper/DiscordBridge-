// Copyright Yamahasxviper. All Rights Reserved.

#include "BanSystemModule.h"

// SML
#include "Command/ChatCommandLibrary.h"

// UE GameMode events
#include "GameFramework/GameModeBase.h"

// BanSystem
#include "BanIdResolver.h"
#include "BanDiscordSubsystem.h"
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

    // ── 1. Install PreLogin ban-check hook (primary enforcement) ──────────
    //
    // Fires on the server BEFORE a player's connection is accepted — i.e.
    // before any PlayerController or PlayerState is created.  Setting
    // ErrorMessage to a non-empty string here causes the engine to reject
    // the connection entirely, so the banned player never enters the game.
    //
    // On a dedicated server this is the cleanest enforcement point:
    //   • The client sees a proper rejection error instead of a kick.
    //   • No world state is created for the banned player.
    //   • No race condition between "player loads in" and "player gets kicked".
    //
    // ID resolution is performed directly on the FUniqueNetIdRepl supplied
    // by the engine at this stage.  If resolution fails here (e.g. because
    // the EOS PUID is not yet available at PreLogin time on some platforms),
    // the PostLogin fallback hook below will still catch the player.
    //
    // FGameModeEvents::GameModePreLoginEvent is a native UE delegate:
    //   DECLARE_EVENT_ThreeParams(AGameModeBase, FGameModePreLoginEvent,
    //       AGameModeBase*, const FUniqueNetIdRepl&, FString&)
    PreLoginHookHandle = FGameModeEvents::GameModePreLoginEvent.AddLambda(
        [](AGameModeBase* GameMode, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
        {
            // Another plugin may have already rejected this connection.
            // Preserve that error message so the player sees the original reason.
            if (!ErrorMessage.IsEmpty()) return;

            if (!GameMode) return;

            const FResolvedBanId ResolvedId = FBanIdResolver::Resolve(UniqueId);
            if (!ResolvedId.IsValid())
            {
                UE_LOG(LogBanSystem, Verbose,
                    TEXT("PreLogin: no ban-relevant platform ID for type '%s' — skipping."),
                    *FBanIdResolver::GetIdTypeName(UniqueId));
                return;
            }

            UGameInstance* GI = GameMode->GetGameInstance();
            if (!GI) return;

            // ── Steam ban check ────────────────────────────────────────────
            if (ResolvedId.HasSteamId())
            {
                USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>();
                if (SteamBans)
                {
                    FString Reason;
                    if (SteamBans->IsPlayerBanned(ResolvedId.Steam64Id, Reason))
                    {
                        UE_LOG(LogBanSystem, Log,
                            TEXT("PreLogin: rejecting Steam-banned player %s — Reason: %s"),
                            *ResolvedId.Steam64Id, *Reason);

                        FString KickMsg;
                        UBanDiscordSubsystem* BanDiscord = GI->GetSubsystem<UBanDiscordSubsystem>();
                        if (BanDiscord && !BanDiscord->GetConfig().SteamBanKickReason.IsEmpty())
                            KickMsg = BanDiscord->GetConfig().SteamBanKickReason;
                        else
                            KickMsg = FString::Printf(TEXT("[Steam Ban] %s"), *Reason);

                        ErrorMessage = KickMsg;

                        if (BanDiscord)
                        {
                            const FString& NotifyFmt = BanDiscord->GetConfig().BanKickDiscordMessage;
                            const FString& ChannelId = BanDiscord->GetConfig().DiscordChannelId;
                            if (!NotifyFmt.IsEmpty() && !ChannelId.IsEmpty())
                            {
                                FString Notice = NotifyFmt;
                                Notice = Notice.Replace(TEXT("%PlayerId%"), *ResolvedId.Steam64Id);
                                Notice = Notice.Replace(TEXT("%Reason%"),   *Reason);
                                BanDiscord->SendDiscordChannelMessage(ChannelId, Notice);
                            }
                        }
                        return;
                    }
                }
            }

            // ── EOS ban check ──────────────────────────────────────────────
            if (ResolvedId.HasEOSPuid())
            {
                UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>();
                if (EOSBans)
                {
                    FString Reason;
                    if (EOSBans->IsPlayerBanned(ResolvedId.EOSProductUserId, Reason))
                    {
                        UE_LOG(LogBanSystem, Log,
                            TEXT("PreLogin: rejecting EOS-banned player %s — Reason: %s"),
                            *ResolvedId.EOSProductUserId, *Reason);

                        FString KickMsg;
                        UBanDiscordSubsystem* BanDiscord = GI->GetSubsystem<UBanDiscordSubsystem>();
                        if (BanDiscord && !BanDiscord->GetConfig().EOSBanKickReason.IsEmpty())
                            KickMsg = BanDiscord->GetConfig().EOSBanKickReason;
                        else
                            KickMsg = FString::Printf(TEXT("[EOS Ban] %s"), *Reason);

                        ErrorMessage = KickMsg;

                        if (BanDiscord)
                        {
                            const FString& NotifyFmt = BanDiscord->GetConfig().BanKickDiscordMessage;
                            const FString& ChannelId = BanDiscord->GetConfig().DiscordChannelId;
                            if (!NotifyFmt.IsEmpty() && !ChannelId.IsEmpty())
                            {
                                FString Notice = NotifyFmt;
                                Notice = Notice.Replace(TEXT("%PlayerId%"), *ResolvedId.EOSProductUserId);
                                Notice = Notice.Replace(TEXT("%Reason%"),   *Reason);
                                BanDiscord->SendDiscordChannelMessage(ChannelId, Notice);
                            }
                        }
                        return;
                    }
                }
            }
        }
    );

    // ── 2. Install PostLogin after-hook (secondary fallback) ──────────────
    //
    // Fires on the server after every player has successfully connected and
    // had their PlayerController/PlayerState created.
    //
    // This serves as a safety net for cases where the player's platform IDs
    // were not available or could not be resolved during PreLogin (e.g. when
    // the EOS PUID becomes available only after full authentication).
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
    // FGameModeEvents::GameModePostLoginEvent is a native UE delegate that fires
    // after every AGameModeBase::PostLogin call without going through funchook,
    // so it works even when the AFGGameMode::PostLogin body is too short to hook
    // directly (which causes "Too short instructions" in the editor build).
    PostLoginHookHandle = FGameModeEvents::GameModePostLoginEvent.AddLambda(
        [](AGameModeBase* GameMode, APlayerController* NewPlayer)
        {
            if (!GameMode || !NewPlayer || !NewPlayer->PlayerState) return;

            UGameInstance* GI = GameMode->GetGameInstance();
            if (!GI) return;

            const FUniqueNetIdRepl UniqueId = NewPlayer->PlayerState->GetUniqueId();
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

                        // Determine kick message (config override or default)
                        FString KickMsg;
                        UBanDiscordSubsystem* BanDiscord = GI->GetSubsystem<UBanDiscordSubsystem>();
                        if (BanDiscord && !BanDiscord->GetConfig().SteamBanKickReason.IsEmpty())
                        {
                            KickMsg = BanDiscord->GetConfig().SteamBanKickReason;
                        }
                        else
                        {
                            KickMsg = FString::Printf(TEXT("[Steam Ban] %s"), *Reason);
                        }

                        if (AGameSession* Session = GameMode->GameSession)
                        {
                            Session->KickPlayer(NewPlayer, FText::FromString(KickMsg));
                        }

                        // Post Discord notification if configured
                        if (BanDiscord)
                        {
                            const FString& NotifyFmt = BanDiscord->GetConfig().BanKickDiscordMessage;
                            const FString& ChannelId = BanDiscord->GetConfig().DiscordChannelId;
                            if (!NotifyFmt.IsEmpty() && !ChannelId.IsEmpty())
                            {
                                FString Notice = NotifyFmt;
                                Notice = Notice.Replace(TEXT("%PlayerId%"), *ResolvedId.Steam64Id);
                                Notice = Notice.Replace(TEXT("%Reason%"),   *Reason);
                                BanDiscord->SendDiscordChannelMessage(ChannelId, Notice);
                            }
                        }
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

                        // Determine kick message (config override or default)
                        FString KickMsg;
                        UBanDiscordSubsystem* BanDiscord = GI->GetSubsystem<UBanDiscordSubsystem>();
                        if (BanDiscord && !BanDiscord->GetConfig().EOSBanKickReason.IsEmpty())
                        {
                            KickMsg = BanDiscord->GetConfig().EOSBanKickReason;
                        }
                        else
                        {
                            KickMsg = FString::Printf(TEXT("[EOS Ban] %s"), *Reason);
                        }

                        if (AGameSession* Session = GameMode->GameSession)
                        {
                            Session->KickPlayer(NewPlayer, FText::FromString(KickMsg));
                        }

                        // Post Discord notification if configured
                        if (BanDiscord)
                        {
                            const FString& NotifyFmt = BanDiscord->GetConfig().BanKickDiscordMessage;
                            const FString& ChannelId = BanDiscord->GetConfig().DiscordChannelId;
                            if (!NotifyFmt.IsEmpty() && !ChannelId.IsEmpty())
                            {
                                FString Notice = NotifyFmt;
                                Notice = Notice.Replace(TEXT("%PlayerId%"), *ResolvedId.EOSProductUserId);
                                Notice = Notice.Replace(TEXT("%Reason%"),   *Reason);
                                BanDiscord->SendDiscordChannelMessage(ChannelId, Notice);
                            }
                        }
                    }
                }
            }
        }
    );

    // ── 3. Register admin chat commands once each world is ready ──────────
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
        TEXT("BanSystem module started. PreLogin + PostLogin hooks active. Command registration scheduled."));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Module shutdown
// ─────────────────────────────────────────────────────────────────────────────
void FBanSystemModule::ShutdownModule()
{
    // Unregister world delegate
    FWorldDelegates::OnWorldInitializedActors.Remove(WorldInitHandle);
    WorldInitHandle.Reset();

    // Unsubscribe PostLogin delegate
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHookHandle);
    PostLoginHookHandle.Reset();

    // Unsubscribe PreLogin delegate
    FGameModeEvents::GameModePreLoginEvent.Remove(PreLoginHookHandle);
    PreLoginHookHandle.Reset();

    UE_LOG(LogBanSystem, Log, TEXT("BanSystem module shut down."));
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FBanSystemModule, BanSystem)
