// Copyright Yamahasxviper. All Rights Reserved.

#include "ServerWhitelistSubsystem.h"
#include "WhitelistManager.h"

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameMode.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "Modules/ModuleManager.h"
#include "FGChatManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogServerWhitelist, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UServerWhitelistSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only run on dedicated servers.
	return IsRunningDedicatedServer();
}

void UServerWhitelistSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// If DiscordBridge is installed it owns the whitelist; we skip enforcement
	// to prevent double-kicking and conflicting in-memory state.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("DiscordBridge")))
	{
		UE_LOG(LogServerWhitelist, Display,
			TEXT("ServerWhitelist: DiscordBridge is loaded – deferring whitelist enforcement to it."));
		return;
	}

	Config = FServerWhitelistConfig::LoadOrCreate();

	FWhitelistManager::Load(Config.bWhitelistEnabled, Config.InitialWhitelistedPlayers);
	UE_LOG(LogServerWhitelist, Display,
		TEXT("ServerWhitelist: Whitelist active = %s (config default = %s)"),
		FWhitelistManager::IsEnabled() ? TEXT("True") : TEXT("False"),
		Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));

	// Subscribe to PostLogin to enforce the whitelist on each player join.
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
		this, &UServerWhitelistSubsystem::OnPostLogin);

	// Start a 1-second periodic ticker to bind to AFGChatManager for
	// in-game command handling.  The ticker stops once binding succeeds.
	ChatManagerBindTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			return !TryBindToChatManager(); // false = stop ticking
		}),
		1.0f);
}

void UServerWhitelistSubsystem::Deinitialize()
{
	// Stop the chat-manager bind ticker if still running.
	FTSTicker::GetCoreTicker().RemoveTicker(ChatManagerBindTickHandle);
	ChatManagerBindTickHandle.Reset();

	// Remove the PostLogin listener.
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();

	// Unbind from the chat manager.
	if (BoundChatManager.IsValid())
	{
		BoundChatManager->OnChatMessageAdded.RemoveDynamic(
			this, &UServerWhitelistSubsystem::OnGameChatMessageAdded);
		BoundChatManager.Reset();
	}

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// PostLogin enforcement
// ─────────────────────────────────────────────────────────────────────────────

void UServerWhitelistSubsystem::OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller)
{
	if (!Controller || Controller->IsLocalController())
	{
		return;
	}

	const APlayerState* PS         = Controller->GetPlayerState<APlayerState>();
	const FString       PlayerName = PS ? PS->GetPlayerName() : FString();

	// In Satisfactory's CSS UE build the player name is populated asynchronously
	// from Epic Online Services after PostLogin fires.  Schedule a deferred retry
	// when enforcement is active and the name is not yet available.
	if (PlayerName.IsEmpty())
	{
		if (!FWhitelistManager::IsEnabled())
		{
			return;
		}

		UE_LOG(LogServerWhitelist, Warning,
			TEXT("ServerWhitelist: player joined with empty name – scheduling deferred check."));

		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AGameModeBase>     WeakGM(GameMode);
			TWeakObjectPtr<APlayerController> WeakPC(Controller);
			TWeakObjectPtr<UWorld>            WeakWorld(World);

			TSharedRef<FTimerHandle> SharedHandle = MakeShared<FTimerHandle>();
			TSharedRef<int32>        RetriesLeft  = MakeShared<int32>(10);

			World->GetTimerManager().SetTimer(*SharedHandle,
				FTimerDelegate::CreateWeakLambda(this,
					[this, WeakGM, WeakPC, WeakWorld, SharedHandle, RetriesLeft]()
				{
					UWorld* W = WeakWorld.Get();

					if (!WeakPC.IsValid())
					{
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						return;
					}

					const APlayerState* RetryPS   = WeakPC->GetPlayerState<APlayerState>();
					const FString       RetryName = RetryPS ? RetryPS->GetPlayerName() : FString();

					if (!RetryName.IsEmpty())
					{
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						OnPostLogin(WeakGM.Get(), WeakPC.Get());
						return;
					}

					--(*RetriesLeft);
					if (*RetriesLeft > 0)
					{
						return;
					}

					// All retries exhausted – kick to enforce whitelist integrity.
					if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
					UE_LOG(LogServerWhitelist, Warning,
						TEXT("ServerWhitelist: player name still empty after deferred check – kicking."));

					AGameModeBase* FallbackGM = WeakGM.IsValid() ? WeakGM.Get() : nullptr;
					if (!FallbackGM && W)
					{
						FallbackGM = W->GetAuthGameMode<AGameModeBase>();
					}
					if (FallbackGM && FallbackGM->GameSession && WeakPC.IsValid())
					{
						FallbackGM->GameSession->KickPlayer(
							WeakPC.Get(),
							FText::FromString(
								TEXT("Unable to verify player identity. Please reconnect.")));
					}
				}),
				0.5f, true);
		}
		return;
	}

	// ── Resolve effective game mode ───────────────────────────────────────────
	AGameModeBase* EffectiveGM = GameMode;
	if (!EffectiveGM || !EffectiveGM->GameSession)
	{
		if (UWorld* W = Controller->GetWorld())
		{
			EffectiveGM = W->GetAuthGameMode<AGameModeBase>();
		}
	}

	UE_LOG(LogServerWhitelist, Log,
		TEXT("ServerWhitelist: player '%s' joined."), *PlayerName);

	// ── Whitelist check ───────────────────────────────────────────────────────
	if (!FWhitelistManager::IsEnabled())
	{
		return;
	}

	if (FWhitelistManager::IsWhitelisted(PlayerName))
	{
		UE_LOG(LogServerWhitelist, Log,
			TEXT("ServerWhitelist: allowing whitelisted player '%s'."), *PlayerName);
		return;
	}

	UE_LOG(LogServerWhitelist, Warning,
		TEXT("ServerWhitelist: kicking non-whitelisted player '%s'."), *PlayerName);

	if (EffectiveGM && EffectiveGM->GameSession)
	{
		const FString KickReason = Config.WhitelistKickReason.IsEmpty()
			? TEXT("You are not on this server's whitelist. Contact the server admin to be added.")
			: Config.WhitelistKickReason;
		EffectiveGM->GameSession->KickPlayer(
			Controller,
			FText::FromString(KickReason));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat manager binding
// ─────────────────────────────────────────────────────────────────────────────

bool UServerWhitelistSubsystem::TryBindToChatManager()
{
	// Skip if in-game commands are disabled.
	if (Config.InGameCommandPrefix.IsEmpty())
	{
		return true; // stop ticking
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	AFGChatManager* ChatMgr = AFGChatManager::Get(World);
	if (!ChatMgr)
	{
		return false;
	}

	ChatMgr->OnChatMessageAdded.AddDynamic(this, &UServerWhitelistSubsystem::OnGameChatMessageAdded);
	BoundChatManager = ChatMgr;

	// Snapshot current messages so we only respond to NEW ones.
	ChatMgr->GetReceivedChatMessages(LastKnownMessages);

	UE_LOG(LogServerWhitelist, Log,
		TEXT("ServerWhitelist: Bound to AFGChatManager::OnChatMessageAdded."));
	return true;
}

void UServerWhitelistSubsystem::OnGameChatMessageAdded()
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AFGChatManager* ChatMgr = AFGChatManager::Get(World);
	if (!ChatMgr)
	{
		return;
	}

	TArray<FChatMessageStruct> CurrentMessages;
	ChatMgr->GetReceivedChatMessages(CurrentMessages);

	for (const FChatMessageStruct& Msg : CurrentMessages)
	{
		if (Msg.MessageType != EFGChatMessageType::CMT_PlayerMessage)
		{
			continue;
		}

		bool bAlreadySeen = false;
		for (const FChatMessageStruct& Known : LastKnownMessages)
		{
			if (Known.ServerTimeStamp == Msg.ServerTimeStamp &&
			    Known.MessageSender.EqualTo(Msg.MessageSender) &&
			    Known.MessageText.EqualTo(Msg.MessageText))
			{
				bAlreadySeen = true;
				break;
			}
		}

		if (!bAlreadySeen)
		{
			const FString SenderName = Msg.MessageSender.ToString().TrimStartAndEnd();
			const FString MessageText = Msg.MessageText.ToString().TrimStartAndEnd();

			if (!Config.InGameCommandPrefix.IsEmpty() &&
			    MessageText.StartsWith(Config.InGameCommandPrefix, ESearchCase::IgnoreCase))
			{
				const FString SubCommand = MessageText.Mid(Config.InGameCommandPrefix.Len()).TrimStartAndEnd();
				HandleWhitelistCommand(SubCommand, SenderName);
			}
		}
	}

	LastKnownMessages = CurrentMessages;
}

// ─────────────────────────────────────────────────────────────────────────────
// In-game command handler
// ─────────────────────────────────────────────────────────────────────────────

void UServerWhitelistSubsystem::HandleWhitelistCommand(const FString& SubCommand,
                                                        const FString& SenderName)
{
	// Admin check: only players listed in AdminPlayerNames may run commands.
	const bool bIsAdmin = Config.AdminPlayerNames.Contains(SenderName.ToLower());
	if (!bIsAdmin)
	{
		UE_LOG(LogServerWhitelist, Log,
			TEXT("ServerWhitelist: ignoring command from non-admin '%s': '%s'"),
			*SenderName, *SubCommand);
		return;
	}

	UE_LOG(LogServerWhitelist, Log,
		TEXT("ServerWhitelist: in-game command from '%s': '%s'"), *SenderName, *SubCommand);

	FString Response;

	FString Verb, Arg;
	if (!SubCommand.Split(TEXT(" "), &Verb, &Arg, ESearchCase::IgnoreCase))
	{
		Verb = SubCommand.TrimStartAndEnd();
		Arg  = TEXT("");
	}
	Verb = Verb.TrimStartAndEnd().ToLower();
	Arg  = Arg.TrimStartAndEnd();

	if (Verb == TEXT("on"))
	{
		FWhitelistManager::SetEnabled(true);
		Response = TEXT("[Whitelist] ENABLED. Only whitelisted players can join.");
	}
	else if (Verb == TEXT("off"))
	{
		FWhitelistManager::SetEnabled(false);
		Response = TEXT("[Whitelist] DISABLED. All players can join freely.");
	}
	else if (Verb == TEXT("add"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("[Whitelist] Usage: !whitelist add <PlayerName>");
		}
		else if (FWhitelistManager::AddPlayer(Arg))
		{
			Response = FString::Printf(TEXT("[Whitelist] %s added to the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT("[Whitelist] %s is already on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("remove"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("[Whitelist] Usage: !whitelist remove <PlayerName>");
		}
		else if (FWhitelistManager::RemovePlayer(Arg))
		{
			Response = FString::Printf(TEXT("[Whitelist] %s removed from the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT("[Whitelist] %s was not on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("list"))
	{
		const TArray<FString> All    = FWhitelistManager::GetAll();
		const FString         Status = FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		if (All.Num() == 0)
		{
			Response = FString::Printf(TEXT("[Whitelist] %s. No players listed."), *Status);
		}
		else
		{
			Response = FString::Printf(
				TEXT("[Whitelist] %s. Players (%d): %s"),
				*Status, All.Num(), *FString::Join(All, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("status"))
	{
		const FString State = FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		Response = FString::Printf(TEXT("[Whitelist] %s"), *State);
	}
	else
	{
		Response = TEXT("[Whitelist] Commands: on, off, add <name>, remove <name>, list, status.");
	}

	SendGameChatMessage(Response);
}

// ─────────────────────────────────────────────────────────────────────────────
// Utility
// ─────────────────────────────────────────────────────────────────────────────

void UServerWhitelistSubsystem::SendGameChatMessage(const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AFGChatManager* ChatManager = AFGChatManager::Get(World);
	if (!ChatManager)
	{
		return;
	}

	FChatMessageStruct ChatMsg;
	ChatMsg.MessageType   = EFGChatMessageType::CMT_CustomMessage;
	ChatMsg.MessageSender = FText::FromString(TEXT("[Server]"));
	ChatMsg.MessageText   = FText::FromString(Message);

	ChatManager->BroadcastChatMessage(ChatMsg);
}
