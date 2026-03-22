// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridgeSubsystem.h"
#include "TicketSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "BanDiscordSubsystem.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerState.h"
#include "FGChatManager.h"
#include "TimerManager.h"
#include "WhitelistManager.h"
#include "BanManager.h"

// Discord Gateway endpoint (v10, JSON encoding)
static const FString DiscordGatewayUrl = TEXT("wss://gateway.discord.gg/?v=10&encoding=json");
// Discord REST API base URL
static const FString DiscordApiBase    = TEXT("https://discord.com/api/v10");

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// Only create this subsystem on dedicated servers.
	// This prevents the bot from running on client or listen-server builds,
	// meaning players do not need this mod (or SML) installed on their own machine.
	return IsRunningDedicatedServer();
}

void UDiscordBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Subscribe to PostLogin to enforce the whitelist and ban list on each player join.
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
		this, &UDiscordBridgeSubsystem::OnPostLogin);

	// Wire up the Discord→game relay once here so it is never double-bound
	// across reconnect cycles (Connect() may be called multiple times).
	OnDiscordMessageReceived.AddDynamic(this, &UDiscordBridgeSubsystem::RelayDiscordMessageToGame);

	// Start a 1-second periodic ticker that tries to find AFGChatManager and
	// bind to its OnChatMessageAdded delegate.  The ticker stops as soon as
	// binding succeeds (TryBindToChatManager returns true).
	ChatManagerBindTickHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			return !TryBindToChatManager(); // false = stop ticking
		}),
		1.0f);

	// Load (or auto-create) the JSON config file from Configs/DiscordBridge.cfg.
	Config = FDiscordBridgeConfig::LoadOrCreate();

	// Load (or create) the whitelist JSON from disk, using the config value as
	// the default only on the very first server start (when no JSON file exists).
	// After the first start the enabled/disabled state is saved in the JSON and
	// survives restarts, so runtime !whitelist on / !whitelist off changes persist.
	// To force-reset to this config value: delete ServerWhitelist.json and restart.
	FWhitelistManager::Load(Config.bWhitelistEnabled);
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Whitelist active = %s (WhitelistEnabled config = %s)"),
	       FWhitelistManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));

	// Load the ban list AFTER the config so we can pass BanSystemEnabled as the
	// authoritative enabled state.  The ban list (players) is read from
	// ServerBanlist.json, but the enabled/disabled state always comes from the
	// INI config so operators can toggle it by editing the file and restarting.
	FBanManager::Load(Config.bBanSystemEnabled);
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: BanSystem active = %s (BanSystemEnabled config = %s)"),
	       FBanManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bBanSystemEnabled ? TEXT("True") : TEXT("False"));

	if (Config.BotToken.IsEmpty() || Config.ChannelId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: BotToken or ChannelId is not configured. "
		            "Edit Mods/DiscordBridge/Config/DefaultDiscordBridge.ini to enable the bridge."));
		return;
	}

	// Log active format strings so operators can verify they were loaded correctly.
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: ServerName           = \"%s\""), *Config.ServerName);
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: GameToDiscordFormat  = \"%s\""), *Config.GameToDiscordFormat);
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: DiscordToGameFormat  = \"%s\""), *Config.DiscordToGameFormat);

	Connect();

	// Inject ourselves as the Discord provider into TicketSubsystem (if installed).
	// TicketSystem is an optional dependency: DiscordBridge works without it, but
	// when present it powers the ticket panel.  Any mod that implements
	// IDiscordBridgeProvider can do the same by calling SetProvider on the subsystem.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("TicketSystem")))
	{
		Collection.InitializeDependency<UTicketSubsystem>();
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UTicketSubsystem* Tickets = GI->GetSubsystem<UTicketSubsystem>())
			{
				CachedTicketSubsystem = Tickets;
				Tickets->SetProvider(this);
			}
		}
	}

	// Register ourselves as the notification provider for BanSystem (if installed)
	// so that ban/unban events issued by its in-game commands are forwarded to
	// Discord.  BanSystem is an optional dependency: DiscordBridge works without
	// it, but when present it mirrors every platform ban/unban as a Discord
	// message using the configurable BanSystemSteam*/BanSystemEOS* templates.
	if (FModuleManager::Get().IsModuleLoaded(TEXT("BanSystem")))
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			Collection.InitializeDependency<USteamBanSubsystem>();
			Collection.InitializeDependency<UEOSBanSubsystem>();
			Collection.InitializeDependency<UBanDiscordSubsystem>();

			if (USteamBanSubsystem* SteamBans = GI->GetSubsystem<USteamBanSubsystem>())
			{
				CachedSteamBanSubsystem = SteamBans;
				SteamBans->SetNotificationProvider(this);
				UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Registered as BanSystem Steam notification provider."));
			}

			if (UEOSBanSubsystem* EOSBans = GI->GetSubsystem<UEOSBanSubsystem>())
			{
				CachedEOSBanSubsystem = EOSBans;
				EOSBans->SetNotificationProvider(this);
				UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Registered as BanSystem EOS notification provider."));
			}

			// Inject ourselves as the Discord command provider into UBanDiscordSubsystem
			// so that BanSystem's Discord ban commands (!steamban, !eosban, etc.) work.
			if (UBanDiscordSubsystem* BanDiscord = GI->GetSubsystem<UBanDiscordSubsystem>())
			{
				CachedBanDiscordSubsystem = BanDiscord;
				BanDiscord->SetCommandProvider(this);
				UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Registered as BanSystem Discord command provider."));
			}
		}
	}
}

void UDiscordBridgeSubsystem::Deinitialize()
{
	// Detach from TicketSubsystem so it stops processing interactions after we shut down.
	if (UTicketSubsystem* Tickets = CachedTicketSubsystem.Get())
	{
		Tickets->SetProvider(nullptr);
	}
	CachedTicketSubsystem.Reset();

	// Clear ourselves as the BanSystem notification provider so no stale
	// callbacks fire after this subsystem is destroyed.
	if (USteamBanSubsystem* SteamBans = CachedSteamBanSubsystem.Get())
	{
		SteamBans->SetNotificationProvider(nullptr);
	}
	CachedSteamBanSubsystem.Reset();

	if (UEOSBanSubsystem* EOSBans = CachedEOSBanSubsystem.Get())
	{
		EOSBans->SetNotificationProvider(nullptr);
	}
	CachedEOSBanSubsystem.Reset();

	// Clear ourselves as the BanSystem Discord command provider so no stale
	// Discord message subscriptions fire after this subsystem is destroyed.
	if (UBanDiscordSubsystem* BanDiscord = CachedBanDiscordSubsystem.Get())
	{
		BanDiscord->SetCommandProvider(nullptr);
	}
	CachedBanDiscordSubsystem.Reset();

	// Stop the chat-manager bind ticker if it is still running.
	FTSTicker::GetCoreTicker().RemoveTicker(ChatManagerBindTickHandle);
	ChatManagerBindTickHandle.Reset();

	// Remove the whitelist PostLogin listener.
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();

	// Unbind from the chat manager's delegate so no stale callbacks fire
	// after this subsystem is destroyed.
	if (BoundChatManager.IsValid())
	{
		BoundChatManager->OnChatMessageAdded.RemoveDynamic(
			this, &UDiscordBridgeSubsystem::OnGameChatMessageAdded);
		BoundChatManager.Reset();
	}

	Disconnect();
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Connection management
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::Connect()
{
	if (WebSocketClient && WebSocketClient->IsConnected())
	{
		return; // Already connected.
	}

	WebSocketClient = USMLWebSocketClient::CreateWebSocketClient(this);

	// Configure auto-reconnect; Discord may close the connection at any time.
	WebSocketClient->bAutoReconnect              = true;
	WebSocketClient->ReconnectInitialDelaySeconds = 2.0f;
	WebSocketClient->MaxReconnectDelaySeconds     = 30.0f;
	WebSocketClient->MaxReconnectAttempts         = 0; // infinite

	// Bind WebSocket delegates.
	WebSocketClient->OnConnected.AddDynamic(this,  &UDiscordBridgeSubsystem::OnWebSocketConnected);
	WebSocketClient->OnMessage.AddDynamic(this,    &UDiscordBridgeSubsystem::OnWebSocketMessage);
	WebSocketClient->OnClosed.AddDynamic(this,     &UDiscordBridgeSubsystem::OnWebSocketClosed);
	WebSocketClient->OnError.AddDynamic(this,      &UDiscordBridgeSubsystem::OnWebSocketError);
	WebSocketClient->OnReconnecting.AddDynamic(this, &UDiscordBridgeSubsystem::OnWebSocketReconnecting);

	UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Connecting to Discord Gateway…"));
	WebSocketClient->Connect(DiscordGatewayUrl, {}, {});
}

void UDiscordBridgeSubsystem::Disconnect()
{
	// Stop heartbeat ticker.
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();

	// Stop player count presence ticker.
	FTSTicker::GetCoreTicker().RemoveTicker(PlayerCountTickerHandle);
	PlayerCountTickerHandle.Reset();

	// Signal offline status and post the server-offline notification while
	// the WebSocket is still open so Discord receives both before we close.
	if (bGatewayReady)
	{
		// Setting presence to "invisible" makes the bot appear offline to
		// users immediately, without waiting for Discord to detect the
		// WebSocket disconnection.
		SendUpdatePresence(TEXT("invisible"));

		if (!Config.ServerOfflineMessage.IsEmpty())
		{
			SendStatusMessageToDiscord(Config.ServerOfflineMessage);
		}
	}

	bGatewayReady            = false;
	bPendingHeartbeatAck     = false;
	bServerOnlineMessageSent = false;
	LastSequenceNumber       = -1;
	BotUserId.Empty();
	GuildId.Empty();
	GuildOwnerId.Empty();

	if (WebSocketClient)
	{
		WebSocketClient->Close(1000, TEXT("Client shutting down"));
		WebSocketClient = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket event handlers (game thread)
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::OnWebSocketConnected()
{
	UE_LOG(LogTemp, Log, TEXT("DiscordBridge: WebSocket connection established. Awaiting Hello…"));
	// Discord will send op=10 (Hello) next; we send Identify in response.
}

void UDiscordBridgeSubsystem::OnWebSocketMessage(const FString& RawJson)
{
	HandleGatewayPayload(RawJson);
}

void UDiscordBridgeSubsystem::OnWebSocketClosed(int32 StatusCode, const FString& Reason)
{
	UE_LOG(LogTemp, Warning,
	       TEXT("DiscordBridge: Gateway connection closed (code=%d, reason='%s')."),
	       StatusCode, *Reason);

	// Detect Discord-specific close codes that indicate a permanent error.
	// For these codes reconnecting with the same credentials will never succeed,
	// so we signal the WebSocket client to stop auto-reconnecting.
	bool bTerminal = false;
	switch (StatusCode)
	{
	case 4004:
		UE_LOG(LogTemp, Error,
		       TEXT("DiscordBridge: Authentication failed (4004). "
		            "Verify BotToken in Mods/DiscordBridge/Config/DefaultDiscordBridge.ini. "
		            "Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4010:
		UE_LOG(LogTemp, Error, TEXT("DiscordBridge: Invalid shard sent (4010). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4011:
		UE_LOG(LogTemp, Error, TEXT("DiscordBridge: Sharding required (4011). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4012:
		UE_LOG(LogTemp, Error, TEXT("DiscordBridge: Invalid Gateway API version (4012). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4013:
		UE_LOG(LogTemp, Error, TEXT("DiscordBridge: Invalid intent(s) (4013). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4014:
		UE_LOG(LogTemp, Error,
		       TEXT("DiscordBridge: Disallowed intent(s) (4014). "
		            "Enable Server Members Intent and Message Content Intent "
		            "in the Discord Developer Portal. Auto-reconnect disabled."));
		bTerminal = true;
		break;
	default:
		break;
	}

	if (bTerminal && WebSocketClient)
	{
		// Calling Close() sets bUserInitiatedClose on the background thread,
		// which causes the reconnect loop to exit without retrying.
		WebSocketClient->Close(1000, FString::Printf(TEXT("Terminal Discord close code %d"), StatusCode));
	}

	// Cancel heartbeat; it will be restarted on the next successful connection.
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;

	const bool bWasReady = bGatewayReady;
	bGatewayReady = false;

	if (bWasReady)
	{
		OnDiscordDisconnected.Broadcast(
			FString::Printf(TEXT("Connection closed (code %d): %s"), StatusCode, *Reason));
	}
}

void UDiscordBridgeSubsystem::OnWebSocketError(const FString& ErrorMessage)
{
	UE_LOG(LogTemp, Error, TEXT("DiscordBridge: WebSocket error: %s"), *ErrorMessage);

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;

	if (bGatewayReady)
	{
		bGatewayReady = false;
		OnDiscordDisconnected.Broadcast(FString::Printf(TEXT("WebSocket error: %s"), *ErrorMessage));
	}
}

void UDiscordBridgeSubsystem::OnWebSocketReconnecting(int32 AttemptNumber, float DelaySeconds)
{
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Reconnecting to Discord Gateway (attempt %d, delay %.1fs)…"),
	       AttemptNumber, DelaySeconds);

	// Reset Gateway state; we'll re-identify once the WebSocket reconnects.
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord Gateway protocol
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleGatewayPayload(const FString& RawJson)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTemp, Warning, TEXT("DiscordBridge: Failed to parse Gateway JSON: %s"), *RawJson);
		return;
	}

	const int32 OpCode = Root->GetIntegerField(TEXT("op"));

	switch (OpCode)
	{
	case EDiscordGatewayOpcode::Dispatch:
	{
		// Update the sequence number first; it is used in heartbeats.
		// TryGetNumberField only accepts double& in UE5's FJsonObject API.
		double Seq = -1.0;
		if (Root->TryGetNumberField(TEXT("s"), Seq))
		{
			LastSequenceNumber = static_cast<int32>(Seq);
		}

		FString EventType;
		Root->TryGetStringField(TEXT("t"), EventType);

		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		Root->TryGetObjectField(TEXT("d"), DataPtr);

		HandleDispatch(EventType, LastSequenceNumber,
		               DataPtr ? *DataPtr : MakeShared<FJsonObject>());
		break;
	}
	case EDiscordGatewayOpcode::Hello:
	{
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (Root->TryGetObjectField(TEXT("d"), DataPtr) && DataPtr)
		{
			HandleHello(*DataPtr);
		}
		break;
	}
	case EDiscordGatewayOpcode::HeartbeatAck:
		HandleHeartbeatAck();
		break;

	case EDiscordGatewayOpcode::Heartbeat:
		// Server explicitly requested a heartbeat right now.
		SendHeartbeat();
		break;

	case EDiscordGatewayOpcode::Reconnect:
		HandleReconnect();
		break;

	case EDiscordGatewayOpcode::InvalidSession:
	{
		bool bResumable = false;
		Root->TryGetBoolField(TEXT("d"), bResumable);
		HandleInvalidSession(bResumable);
		break;
	}
	default:
		UE_LOG(LogTemp, VeryVerbose,
		       TEXT("DiscordBridge: Unhandled opcode %d"), OpCode);
		break;
	}
}

void UDiscordBridgeSubsystem::HandleHello(const TSharedPtr<FJsonObject>& DataObj)
{
	// Discord sends the heartbeat interval in milliseconds.
	double HeartbeatMs = 41250.0; // sensible default
	DataObj->TryGetNumberField(TEXT("heartbeat_interval"), HeartbeatMs);
	HeartbeatIntervalSeconds = static_cast<float>(HeartbeatMs) / 1000.0f;

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Hello received. Heartbeat interval: %.2f s"),
	       HeartbeatIntervalSeconds);

	// Start heartbeating with a random jitter so that all bots don't hammer the
	// Gateway simultaneously on mass-reconnects (Discord "thundering herd" concern).
	// Strategy: one-shot ticker after a random [0, interval] delay fires the first
	// heartbeat and then installs the regular repeating ticker.
	// HeartbeatTickerHandle tracks whichever ticker is active so Disconnect() can
	// always cancel it with a single RemoveTicker() call.
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	bPendingHeartbeatAck = false;

	const float JitterSeconds = FMath::FRandRange(0.0f, HeartbeatIntervalSeconds);
	HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			SendHeartbeat();
			// Replace the one-shot handle with the regular repeating ticker.
			HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UDiscordBridgeSubsystem::HeartbeatTick),
				HeartbeatIntervalSeconds);
			return false; // one-shot – do not repeat
		}),
		JitterSeconds);

	// Send Identify so Discord authenticates us.
	SendIdentify();
}

void UDiscordBridgeSubsystem::HandleDispatch(const FString& EventType, int32 Sequence,
                                              const TSharedPtr<FJsonObject>& DataObj)
{
	if (EventType == TEXT("READY"))
	{
		HandleReady(DataObj);
	}
	else if (EventType == TEXT("MESSAGE_CREATE"))
	{
		HandleMessageCreate(DataObj);
	}
	else if (EventType == TEXT("GUILD_CREATE"))
	{
		// Capture the guild owner ID so extension modules (e.g. TicketSystem)
		// can authorise guild-owner actions.
		DataObj->TryGetStringField(TEXT("owner_id"), GuildOwnerId);
	}
	else if (EventType == TEXT("INTERACTION_CREATE"))
	{
		// Fire the native delegate so extension modules can handle button clicks
		// and modal submits without modifying DiscordBridge directly.
		if (OnDiscordInteractionReceived.IsBound())
		{
			OnDiscordInteractionReceived.Broadcast(DataObj);
		}
	}
	else if (EventType == TEXT("GUILD_MEMBER_ADD") ||
	         EventType == TEXT("GUILD_MEMBER_UPDATE"))
	{
		// Keep the Discord role member cache up to date so the whitelist
		// check in OnPostLogin stays accurate without a full re-fetch.
		if (!Config.WhitelistRoleId.IsEmpty())
		{
			UpdateWhitelistRoleMemberEntry(DataObj);
		}
	}
	else if (EventType == TEXT("GUILD_MEMBER_REMOVE"))
	{
		// Member left the guild – remove from cache unconditionally.
		if (!Config.WhitelistRoleId.IsEmpty())
		{
			UpdateWhitelistRoleMemberEntry(DataObj, /*bRemoved=*/true);
		}
	}
	// Other events are received but not processed by this bridge.
}

void UDiscordBridgeSubsystem::HandleHeartbeatAck()
{
	UE_LOG(LogTemp, VeryVerbose, TEXT("DiscordBridge: Heartbeat acknowledged."));
	bPendingHeartbeatAck = false;
}

void UDiscordBridgeSubsystem::HandleReconnect()
{
	UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Server requested reconnect."));

	// Reset Gateway state; we'll re-identify once the WebSocket reconnects.
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady        = false;
	LastSequenceNumber   = -1;
	BotUserId.Empty();
	GuildId.Empty();
	GuildOwnerId.Empty();

	// Restart the WebSocket connection by calling Connect() on the existing
	// client.  Do NOT call Close() here: Close() → EnqueueClose() sets
	// bUserInitiatedClose = true inside FSMLWebSocketRunnable, which exits
	// the reconnect loop permanently and leaves the bot offline.
	// Connect() stops the current thread and starts a fresh runnable with
	// auto-reconnect still enabled.
	if (WebSocketClient)
	{
		WebSocketClient->Connect(DiscordGatewayUrl, {}, {});
	}
}

void UDiscordBridgeSubsystem::HandleInvalidSession(bool bResumable)
{
	UE_LOG(LogTemp, Warning,
	       TEXT("DiscordBridge: Invalid session (resumable=%s). Re-identifying in 2s…"),
	       bResumable ? TEXT("true") : TEXT("false"));

	// Per Discord spec, wait 1–5 seconds before re-identifying.
	// Use a one-shot FTSTicker so the game thread is never blocked.
	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			SendIdentify();
			return false; // one-shot – do not repeat
		}),
		2.0f);
}

void UDiscordBridgeSubsystem::HandleReady(const TSharedPtr<FJsonObject>& DataObj)
{
	// Extract the bot user ID so we can filter out self-sent messages.
	const TSharedPtr<FJsonObject>* UserPtr = nullptr;
	if (DataObj->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
	{
		(*UserPtr)->TryGetStringField(TEXT("id"), BotUserId);
	}

	// Extract the guild (server) ID from the first entry in the guilds array.
	// This is needed for Discord REST role-management API calls.
	const TArray<TSharedPtr<FJsonValue>>* GuildsArray = nullptr;
	if (DataObj->TryGetArrayField(TEXT("guilds"), GuildsArray) && GuildsArray && GuildsArray->Num() > 0)
	{
		const TSharedPtr<FJsonObject>* FirstGuild = nullptr;
		if ((*GuildsArray)[0]->TryGetObject(FirstGuild) && FirstGuild)
		{
			(*FirstGuild)->TryGetStringField(TEXT("id"), GuildId);
		}
	}

	bGatewayReady = true;

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Gateway ready. Bot user ID: %s, Guild ID: %s"),
	       *BotUserId, *GuildId);

	// Set bot presence. When the player-count feature is enabled, send the first
	// update immediately and start the periodic refresh ticker.  Otherwise just
	// set the bot to "online" with no activity.
	FTSTicker::GetCoreTicker().RemoveTicker(PlayerCountTickerHandle);
	PlayerCountTickerHandle.Reset();

	if (Config.bShowPlayerCountInPresence)
	{
		UpdatePlayerCountPresence();

		const float Interval = FMath::Max(Config.PlayerCountUpdateIntervalSeconds, 15.0f);
		PlayerCountTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateUObject(this, &UDiscordBridgeSubsystem::PlayerCountTick),
			Interval);
	}
	else
	{
		SendUpdatePresence(TEXT("online"));
	}

	// Post the server-online notification message the first time only.
	// Discord periodically forces bots to reconnect, which triggers a fresh
	// READY event even though the game server never went offline.  Guard with
	// bServerOnlineMessageSent so we don't spam the channel every ~hour.
	if (!Config.ServerOnlineMessage.IsEmpty() && !bServerOnlineMessageSent)
	{
		SendStatusMessageToDiscord(Config.ServerOnlineMessage);
		bServerOnlineMessageSent = true;
	}

	// Populate the Discord role member cache so that players who hold
	// WhitelistRoleId are not kicked by the game-server whitelist even when
	// they are not listed in ServerWhitelist.json.
	if (!Config.WhitelistRoleId.IsEmpty())
	{
		FetchWhitelistRoleMembers();
	}

	OnDiscordConnected.Broadcast();
}

void UDiscordBridgeSubsystem::HandleMessageCreate(const TSharedPtr<FJsonObject>& DataObj)
{
	// Forward the full message JSON to any external subscribers (e.g. TicketSystem).
	// This is done before channel filtering so subscribers can process messages
	// from any channel (e.g. the !ticket-panel command from an admin channel).
	if (OnDiscordRawMessageReceived.IsBound())
	{
		OnDiscordRawMessageReceived.Broadcast(DataObj);
	}

	// Accept messages from the main channel OR the dedicated whitelist channel.
	FString MsgChannelId;
	if (!DataObj->TryGetStringField(TEXT("channel_id"), MsgChannelId))
	{
		return;
	}

	const bool bIsMainChannel      = (MsgChannelId == Config.ChannelId);
	const bool bIsWhitelistChannel = (!Config.WhitelistChannelId.IsEmpty() &&
	                                  MsgChannelId == Config.WhitelistChannelId);
	const bool bIsBanChannel       = (!Config.BanChannelId.IsEmpty() &&
	                                  MsgChannelId == Config.BanChannelId);

	if (!bIsMainChannel && !bIsWhitelistChannel && !bIsBanChannel)
	{
		return;
	}

	// Extract the author object.
	const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
	if (!DataObj->TryGetObjectField(TEXT("author"), AuthorPtr) || !AuthorPtr)
	{
		return;
	}
	const TSharedPtr<FJsonObject>& Author = *AuthorPtr;

	// Optionally ignore bot messages (including our own) to prevent echo loops.
	if (Config.bIgnoreBotMessages)
	{
		bool bIsBot = false;
		Author->TryGetBoolField(TEXT("bot"), bIsBot);
		if (bIsBot)
		{
			return;
		}
	}
	// Always ignore this bot's own messages regardless of the config flag.
	FString AuthorId;
	Author->TryGetStringField(TEXT("id"), AuthorId);
	if (!BotUserId.IsEmpty() && AuthorId == BotUserId)
	{
		return;
	}

	// Display name priority: server nickname > global display name > username.
	// The member object is included in MESSAGE_CREATE events for guild messages
	// when the GUILD_MEMBERS intent is enabled.
	FString Username;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (DataObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
	{
		(*MemberPtr)->TryGetStringField(TEXT("nick"), Username);
	}
	if (Username.IsEmpty())
	{
		if (!Author->TryGetStringField(TEXT("global_name"), Username) || Username.IsEmpty())
		{
			Author->TryGetStringField(TEXT("username"), Username);
		}
	}
	// Final safety fallback: every Discord user has a username, but guard against
	// unexpected API responses that omit all name fields.
	if (Username.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: Could not extract display name from Discord message author; using 'Discord User'."));
		Username = TEXT("Discord User");
	}

	// For the dedicated whitelist channel: only relay to game when the sender
	// holds the configured whitelist role (if WhitelistRoleId is set).
	if (bIsWhitelistChannel && !Config.WhitelistRoleId.IsEmpty())
	{
		bool bHasRole = false;
		if (MemberPtr)
		{
			const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
			if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
			{
				for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
				{
					FString RoleId;
					if (RoleVal->TryGetString(RoleId) && RoleId == Config.WhitelistRoleId)
					{
						bHasRole = true;
						break;
					}
				}
			}
		}
		if (!bHasRole)
		{
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Ignoring whitelist-channel message from '%s' – sender lacks whitelist role."),
			       *Username);
			return;
		}
	}

	FString Content;
	DataObj->TryGetStringField(TEXT("content"), Content);
	Content = Content.TrimStartAndEnd();

	if (Content.IsEmpty())
	{
		return; // Embeds-only, sticker-only, or whitespace-only messages; skip.
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Discord message received from '%s' (channel %s): %s"),
	       *Username, *MsgChannelId, *Content);

	// Helper: returns true if the Discord member holds the given required role.
	// When RequiredRoleId is empty the command is disabled entirely – nobody
	// can run it until a role ID is configured.  Note: WhitelistCommandRoleId
	// does NOT grant game-join bypass; only WhitelistRoleId holders receive that
	// via the role-member cache checked in OnPostLogin.
	auto HasRequiredRole = [&](const FString& RequiredRoleId) -> bool
	{
		if (RequiredRoleId.IsEmpty())
		{
			return false; // No role configured – command is disabled.
		}
		if (!MemberPtr)
		{
			return false;
		}
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
		{
			for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
			{
				FString RoleId;
				if (RoleVal->TryGetString(RoleId) && RoleId == RequiredRoleId)
				{
					return true;
				}
			}
		}
		return false;
	};

	// Check whether this message is a whitelist management command.
	if (!Config.WhitelistCommandPrefix.IsEmpty() &&
	    Content.StartsWith(Config.WhitelistCommandPrefix, ESearchCase::IgnoreCase))
	{
		if (!HasRequiredRole(Config.WhitelistCommandRoleId))
		{
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Ignoring whitelist command from '%s' – sender lacks WhitelistCommandRoleId."),
			       *Username);
			SendMessageToChannel(MsgChannelId, TEXT(":no_entry: You do not have permission to use whitelist commands."));
			return;
		}
		// Extract everything after the prefix as the sub-command (trimmed).
		const FString SubCommand = Content.Mid(Config.WhitelistCommandPrefix.Len()).TrimStartAndEnd();
		HandleWhitelistCommand(SubCommand, Username, MsgChannelId);
		return; // Do not relay whitelist commands to in-game chat.
	}

	// Check whether this message is a ban management command.
	if (!Config.BanCommandPrefix.IsEmpty() &&
	    Content.StartsWith(Config.BanCommandPrefix, ESearchCase::IgnoreCase))
	{
		if (!Config.bBanCommandsEnabled)
		{
			// Ban commands are disabled via config – silently ignore (ban enforcement is unaffected).
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Ignoring ban command from '%s' – BanCommandsEnabled=False."),
			       *Username);
			return;
		}
		if (!HasRequiredRole(Config.BanCommandRoleId))
		{
			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Ignoring ban command from '%s' – sender lacks BanCommandRoleId."),
			       *Username);
			// Reply in the channel where the command was typed.
			const FString ReplyChannel = bIsBanChannel ? Config.BanChannelId : Config.ChannelId;
			SendMessageToChannel(ReplyChannel,
			                     TEXT(":no_entry: You do not have permission to use ban commands."));
			return;
		}
		// Extract everything after the prefix as the sub-command (trimmed).
		const FString SubCommand = Content.Mid(Config.BanCommandPrefix.Len()).TrimStartAndEnd();
		// Route the response back to the channel where the command was issued.
		const FString ResponseChannel = bIsBanChannel ? Config.BanChannelId : Config.ChannelId;
		HandleBanCommand(SubCommand, Username, ResponseChannel);
		return; // Do not relay ban commands to in-game chat.
	}

	// Messages from the dedicated ban channel that are not ban commands are
	// silently ignored – the ban channel is admin-only and not bridged to game chat.
	if (bIsBanChannel)
	{
		return;
	}

	OnDiscordMessageReceived.Broadcast(Username, Content);
}

// ─────────────────────────────────────────────────────────────────────────────
// Sending Gateway payloads
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::SendIdentify()
{
	// Build the connection properties object.
	// The "os" property is informational; Discord uses it to identify the client
	// platform.  Use the actual compile-time platform so it is accurate for both
	// the Windows and Linux dedicated-server Alpakit targets.
#if PLATFORM_WINDOWS
	static const FString DiscordOs = TEXT("windows");
#elif PLATFORM_LINUX
	static const FString DiscordOs = TEXT("linux");
#else
	static const FString DiscordOs = TEXT("unknown");
#endif
	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetStringField(TEXT("os"),      DiscordOs);
	Props->SetStringField(TEXT("browser"), TEXT("satisfactory_discord_bridge"));
	Props->SetStringField(TEXT("device"),  TEXT("satisfactory_discord_bridge"));

	// Set the initial presence so the bot appears online immediately upon
	// authentication, before a separate UpdatePresence op is sent.
	TSharedPtr<FJsonObject> InitialPresence = MakeShared<FJsonObject>();
	InitialPresence->SetField(TEXT("since"), MakeShared<FJsonValueNull>());
	InitialPresence->SetArrayField(TEXT("activities"), TArray<TSharedPtr<FJsonValue>>());
	InitialPresence->SetStringField(TEXT("status"), TEXT("online"));
	InitialPresence->SetBoolField(TEXT("afk"), false);

	// Build the Identify data object.
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("token"),   Config.BotToken);
	Data->SetNumberField(TEXT("intents"), EDiscordGatewayIntent::All);
	Data->SetObjectField(TEXT("properties"), Props);
	Data->SetObjectField(TEXT("presence"), InitialPresence);

	// Wrap in the Gateway payload envelope.
	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), EDiscordGatewayOpcode::Identify);
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Identify sent (intents=%d)."),
	       EDiscordGatewayIntent::All);
}

void UDiscordBridgeSubsystem::SendHeartbeat()
{
	// Zombie-connection detection (per Discord Gateway documentation):
	// If the previous heartbeat was never acknowledged, the connection is a
	// zombie – packets are being sent locally but not reaching Discord.
	// Discord has already marked the bot offline.  Force a fresh connection
	// by calling Connect() so USMLWebSocketClient's auto-reconnect remains enabled.
	if (bPendingHeartbeatAck)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: Heartbeat not acknowledged – zombie connection detected. "
		            "Reconnecting."));

		// Cancel the heartbeat ticker before reconnecting so no further heartbeats
		// are sent on the dead socket.  HandleHello will restart it on the new connection.
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
		bPendingHeartbeatAck = false;

		// Reset Gateway state; we'll re-identify once the WebSocket reconnects.
		bGatewayReady      = false;
		LastSequenceNumber = -1;
		BotUserId.Empty();
		GuildId.Empty();
		GuildOwnerId.Empty();

		if (WebSocketClient)
		{
			// Use Connect() instead of Close() to force a fresh connection.
			// Close() → EnqueueClose() sets bUserInitiatedClose = true inside
			// FSMLWebSocketRunnable, which exits the reconnect loop permanently
			// and leaves the bot offline.  Connect() stops the current thread
			// and starts a new runnable with auto-reconnect still enabled.
			WebSocketClient->Connect(DiscordGatewayUrl, {}, {});
		}
		return;
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), EDiscordGatewayOpcode::Heartbeat);

	// The heartbeat data field must be the last received sequence number, or
	// a JSON null if no dispatch has been received yet.
	if (LastSequenceNumber >= 0)
	{
		Payload->SetNumberField(TEXT("d"), LastSequenceNumber);
	}
	else
	{
		Payload->SetField(TEXT("d"), MakeShared<FJsonValueNull>());
	}

	SendGatewayPayload(Payload);
	bPendingHeartbeatAck = true;
}

void UDiscordBridgeSubsystem::SendUpdatePresence(const FString& Status)
{
	// Build the presence data object (Discord Gateway op=3).
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetField(TEXT("since"),      MakeShared<FJsonValueNull>());
	Data->SetArrayField(TEXT("activities"), TArray<TSharedPtr<FJsonValue>>());
	Data->SetStringField(TEXT("status"), Status);
	Data->SetBoolField(TEXT("afk"),    false);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), EDiscordGatewayOpcode::UpdatePresence);
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);

	UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Presence updated to '%s'."), *Status);
}

void UDiscordBridgeSubsystem::SendStatusMessageToDiscord(const FString& Message)
{
	if (!Config.bServerStatusMessagesEnabled)
	{
		return;
	}

	// Use the dedicated status channel when configured; fall back to the main channel.
	const FString& TargetChannelId = Config.StatusChannelId.IsEmpty()
		? Config.ChannelId
		: Config.StatusChannelId;
	SendMessageToChannel(TargetChannelId, Message);
}

void UDiscordBridgeSubsystem::SendMessageToChannel(const FString& TargetChannelId,
                                                    const FString& Message)
{
	if (Config.BotToken.IsEmpty() || TargetChannelId.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"), Message);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s/messages"), *DiscordApiBase, *TargetChannelId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[Message](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: HTTP request failed for status message '%s'."),
				       *Message);
				return;
			}
			if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: Discord REST API returned %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Extension API – getters and REST helpers used by external modules
// ─────────────────────────────────────────────────────────────────────────────

const FString& UDiscordBridgeSubsystem::GetBotToken() const
{
	return Config.BotToken;
}

const FString& UDiscordBridgeSubsystem::GetGuildId() const
{
	return GuildId;
}

const FString& UDiscordBridgeSubsystem::GetGuildOwnerId() const
{
	return GuildOwnerId;
}

void UDiscordBridgeSubsystem::SendDiscordChannelMessage(const FString& TargetChannelId,
                                                        const FString& Message)
{
	SendMessageToChannel(TargetChannelId, Message);
}

void UDiscordBridgeSubsystem::RespondToInteraction(const FString& InteractionId,
                                                   const FString& InteractionToken,
                                                   int32 ResponseType,
                                                   const FString& Content,
                                                   bool bEphemeral)
{
	if (Config.BotToken.IsEmpty() || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	if (ResponseType == 4 && !Content.IsEmpty())
	{
		ResponseData->SetStringField(TEXT("content"), Content);
		if (bEphemeral)
		{
			// Discord flags bitmask: 64 = EPHEMERAL
			ResponseData->SetNumberField(TEXT("flags"), 64);
		}
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), ResponseType);
	if (ResponseData->Values.Num() > 0)
	{
		Body->SetObjectField(TEXT("data"), ResponseData);
	}

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/interactions/%s/%s/callback"),
		*DiscordApiBase, *InteractionId, *InteractionToken);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindLambda(
		[InteractionId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: Interaction callback request failed (id=%s)."),
				       *InteractionId);
				return;
			}
			if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 204)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: Interaction callback returned HTTP %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

void UDiscordBridgeSubsystem::RespondWithModal(const FString& InteractionId,
                                               const FString& InteractionToken,
                                               const FString& ModalCustomId,
                                               const FString& Title,
                                               const FString& Placeholder)
{
	if (Config.BotToken.IsEmpty() || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	// Build the single paragraph text-input component.
	TSharedPtr<FJsonObject> TextInput = MakeShared<FJsonObject>();
	TextInput->SetNumberField(TEXT("type"),        4); // TEXT_INPUT
	TextInput->SetStringField(TEXT("custom_id"),   TEXT("ticket_reason"));
	TextInput->SetNumberField(TEXT("style"),       2); // PARAGRAPH (multi-line)
	TextInput->SetStringField(TEXT("label"),       TEXT("Reason"));
	TextInput->SetStringField(TEXT("placeholder"), Placeholder);
	TextInput->SetBoolField  (TEXT("required"),    false);
	TextInput->SetNumberField(TEXT("max_length"),  1000);

	TSharedPtr<FJsonObject> ActionRow = MakeShared<FJsonObject>();
	ActionRow->SetNumberField(TEXT("type"), 1); // ACTION_ROW
	ActionRow->SetArrayField(TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(TextInput) });

	TSharedPtr<FJsonObject> ModalData = MakeShared<FJsonObject>();
	ModalData->SetStringField(TEXT("custom_id"),  ModalCustomId);
	ModalData->SetStringField(TEXT("title"),      Title);
	ModalData->SetArrayField (TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(ActionRow) });

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), 9); // MODAL
	Body->SetObjectField(TEXT("data"), ModalData);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/interactions/%s/%s/callback"),
		*DiscordApiBase, *InteractionId, *InteractionToken);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindLambda(
		[InteractionId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: RespondWithModal request failed (id=%s)."),
				       *InteractionId);
				return;
			}
			if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 204)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: RespondWithModal returned HTTP %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

void UDiscordBridgeSubsystem::SendMessageBodyToChannel(const FString& TargetChannelId,
                                                       const TSharedPtr<FJsonObject>& MessageBody)
{
	if (Config.BotToken.IsEmpty() || TargetChannelId.IsEmpty() || !MessageBody.IsValid())
	{
		return;
	}

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(MessageBody.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s/messages"), *DiscordApiBase, *TargetChannelId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindLambda(
		[TargetChannelId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: SendMessageBodyToChannel request failed (channel=%s)."),
				       *TargetChannelId);
				return;
			}
			if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: SendMessageBodyToChannel returned HTTP %d (channel=%s): %s"),
				       Resp->GetResponseCode(), *TargetChannelId,
				       *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

void UDiscordBridgeSubsystem::DeleteDiscordChannel(const FString& ChannelId)
{
	if (Config.BotToken.IsEmpty() || ChannelId.IsEmpty())
	{
		return;
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Deleting channel %s."), *ChannelId);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s"), *DiscordApiBase, *ChannelId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("DELETE"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));

	Request->OnProcessRequestComplete().BindLambda(
		[ChannelId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: DeleteDiscordChannel request failed (channel=%s)."),
				       *ChannelId);
				return;
			}
			if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: DeleteDiscordChannel returned HTTP %d (channel=%s): %s"),
				       Resp->GetResponseCode(), *ChannelId, *Resp->GetContentAsString());
			}
			else
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Channel %s deleted successfully."), *ChannelId);
			}
		});

	Request->ProcessRequest();
}

void UDiscordBridgeSubsystem::CreateDiscordGuildTextChannel(
	const FString& ChannelName,
	const FString& CategoryId,
	const TArray<TSharedPtr<FJsonValue>>& PermissionOverwrites,
	TFunction<void(const FString& NewChannelId)> OnCreated)
{
	if (Config.BotToken.IsEmpty() || GuildId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: CreateDiscordGuildTextChannel – BotToken or GuildId empty."));
		if (OnCreated)
		{
			OnCreated(TEXT(""));
		}
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), ChannelName);
	Body->SetNumberField(TEXT("type"), 0); // GUILD_TEXT
	Body->SetArrayField(TEXT("permission_overwrites"), PermissionOverwrites);
	if (!CategoryId.IsEmpty())
	{
		Body->SetStringField(TEXT("parent_id"), CategoryId);
	}

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/guilds/%s/channels"), *DiscordApiBase, *GuildId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[OnCreated = MoveTemp(OnCreated), ChannelName]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected) mutable
		{
			if (!bConnected || !Resp.IsValid() ||
			    (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 201))
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: CreateDiscordGuildTextChannel failed for '%s'. HTTP %d."),
				       *ChannelName,
				       (Resp.IsValid() ? Resp->GetResponseCode() : 0));
				if (OnCreated)
				{
					OnCreated(TEXT(""));
				}
				return;
			}

			TSharedPtr<FJsonObject> ChannelObj;
			TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Resp->GetContentAsString());
			FString NewChannelId;
			if (FJsonSerializer::Deserialize(Reader, ChannelObj) && ChannelObj.IsValid())
			{
				ChannelObj->TryGetStringField(TEXT("id"), NewChannelId);
			}

			if (OnCreated)
			{
				OnCreated(NewChannelId);
			}
		});

	Request->ProcessRequest();
}

void UDiscordBridgeSubsystem::SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload)
{
	if (!WebSocketClient || !WebSocketClient->IsConnected())
	{
		return;
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

	WebSocketClient->SendText(JsonString);
}

// ─────────────────────────────────────────────────────────────────────────────
// Heartbeat timer
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::HeartbeatTick(float /*DeltaTime*/)
{
	SendHeartbeat();
	return true; // keep ticking
}

// ─────────────────────────────────────────────────────────────────────────────
// Player count presence
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::PlayerCountTick(float /*DeltaTime*/)
{
	UpdatePlayerCountPresence();
	return true; // keep ticking
}

void UDiscordBridgeSubsystem::UpdatePlayerCountPresence()
{
	if (!bGatewayReady || !Config.bShowPlayerCountInPresence)
	{
		return;
	}

	// Count connected players using the game state's player array.
	int32 PlayerCount = 0;
	if (UWorld* World = GetWorld())
	{
		if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
		{
			PlayerCount = GS->PlayerArray.Num();
		}
	}

	// Apply configured format placeholders.
	FString ActivityText = Config.PlayerCountPresenceFormat;
	ActivityText = ActivityText.Replace(TEXT("%PlayerCount%"), *FString::FromInt(PlayerCount));
	ActivityText = ActivityText.Replace(TEXT("%ServerName%"),  *Config.ServerName);
	ActivityText = ActivityText.TrimStartAndEnd();

	// If the user left the format blank, fall back to just the player count number
	// so Discord never receives an empty activity name.
	if (ActivityText.IsEmpty())
	{
		ActivityText = FString::FromInt(PlayerCount);
	}

	// Build a Discord activity object using the configured activity type.
	// Common types: 0=Playing, 2=Listening to, 3=Watching, 5=Competing in.
	TSharedPtr<FJsonObject> Activity = MakeShared<FJsonObject>();
	Activity->SetNumberField(TEXT("type"), Config.PlayerCountActivityType);
	Activity->SetStringField(TEXT("name"), ActivityText);

	TArray<TSharedPtr<FJsonValue>> Activities;
	Activities.Add(MakeShared<FJsonValueObject>(Activity));

	// Build the presence update payload (op=3).
	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetField(TEXT("since"),      MakeShared<FJsonValueNull>());
	Data->SetArrayField(TEXT("activities"), Activities);
	Data->SetStringField(TEXT("status"), TEXT("online"));
	Data->SetBoolField(TEXT("afk"),    false);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), EDiscordGatewayOpcode::UpdatePresence);
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Player count presence updated (%d players) – activity: \"%s\""),
	       PlayerCount, *ActivityText);
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat manager binding (Game → Discord)
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::TryBindToChatManager()
{
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

	ChatMgr->OnChatMessageAdded.AddDynamic(this, &UDiscordBridgeSubsystem::OnGameChatMessageAdded);
	BoundChatManager = ChatMgr;

	// Snapshot the current messages so we only forward NEW ones going forward.
	ChatMgr->GetReceivedChatMessages(LastKnownMessages);

	UE_LOG(LogTemp, Log, TEXT("DiscordBridge: Bound to AFGChatManager::OnChatMessageAdded."));
	return true;
}

void UDiscordBridgeSubsystem::OnGameChatMessageAdded()
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

	// Identify messages present in CurrentMessages but absent from LastKnownMessages.
	// Equality is determined by (ServerTimeStamp, MessageSender, MessageText) so
	// that the diff is correct even when the rolling buffer wraps around.
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
			HandleIncomingChatMessage(
				Msg.MessageSender.ToString().TrimStartAndEnd(),
				Msg.MessageText.ToString().TrimStartAndEnd());
		}
	}

	LastKnownMessages = CurrentMessages;
}

// ─────────────────────────────────────────────────────────────────────────────
// Chat-manager hook handler (Game → Discord)
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleIncomingChatMessage(const FString& PlayerName,
                                                         const FString& MessageText)
{
	// Discord relay messages are broadcast as CMT_CustomMessage, which the diff
	// loop in OnGameChatMessageAdded ignores (it only processes CMT_PlayerMessage).
	// Therefore no explicit echo-prevention bookkeeping is required here.

	if (MessageText.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: Skipping player message with empty text from '%s'."),
		       *PlayerName);
		return;
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Player message detected. Sender: '%s', Text: '%s'"),
	       *PlayerName, *MessageText);

	// Check whether this message is an in-game whitelist management command.
	if (!Config.InGameWhitelistCommandPrefix.IsEmpty() &&
	    MessageText.StartsWith(Config.InGameWhitelistCommandPrefix, ESearchCase::IgnoreCase))
	{
		const FString SubCommand = MessageText.Mid(Config.InGameWhitelistCommandPrefix.Len()).TrimStartAndEnd();
		HandleInGameWhitelistCommand(SubCommand);
		return; // Do not forward commands to Discord.
	}

	// Check whether this message is an in-game ban management command.
	if (!Config.InGameBanCommandPrefix.IsEmpty() &&
	    MessageText.StartsWith(Config.InGameBanCommandPrefix, ESearchCase::IgnoreCase))
	{
		if (!Config.bBanCommandsEnabled)
		{
			// In-game ban commands are disabled via config – suppress silently.
			return;
		}
		const FString SubCommand = MessageText.Mid(Config.InGameBanCommandPrefix.Len()).TrimStartAndEnd();
		HandleInGameBanCommand(SubCommand);
		return; // Do not forward commands to Discord.
	}

	SendGameMessageToDiscord(PlayerName, MessageText);
}

// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::SendGameMessageToDiscord(const FString& PlayerName,
                                                        const FString& Message)
{
	if (Config.BotToken.IsEmpty() || Config.ChannelId.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: Cannot send message – BotToken or ChannelId not configured."));
		return;
	}

	// Apply the configurable format string.
	// Fall back to a plain "Name: Message" string when the format is empty so
	// the message is never silently discarded due to a misconfigured INI.
	const FString EffectivePlayerName = PlayerName.IsEmpty() ? TEXT("Unknown") : PlayerName;

	FString FormattedContent = Config.GameToDiscordFormat;
	FormattedContent = FormattedContent.Replace(TEXT("%ServerName%"),  *Config.ServerName);
	FormattedContent = FormattedContent.Replace(TEXT("%PlayerName%"), *EffectivePlayerName);
	FormattedContent = FormattedContent.Replace(TEXT("%Message%"),    *Message);

	if (FormattedContent.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: GameToDiscordFormat produced an empty string for player '%s'. "
		            "Check the GameToDiscordFormat setting in DefaultDiscordBridge.ini."),
		       *EffectivePlayerName);
		return;
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Sending to Discord: %s"), *FormattedContent);

	// Build the JSON body: {"content": "…"}
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"), FormattedContent);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	// Helper lambda to POST the formatted content to a given Discord channel.
	auto PostToChannel = [this, BodyString, FormattedContent, EffectivePlayerName](const FString& TargetChannelId)
	{
		const FString Url = FString::Printf(
			TEXT("%s/channels/%s/messages"), *DiscordApiBase, *TargetChannelId);

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
			FHttpModule::Get().CreateRequest();

		Request->SetURL(Url);
		Request->SetVerb(TEXT("POST"));
		Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
		Request->SetHeader(TEXT("Authorization"),
		                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
		Request->SetContentAsString(BodyString);

		Request->OnProcessRequestComplete().BindWeakLambda(
			this,
			[EffectivePlayerName](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
			{
				if (!bConnected || !Resp.IsValid())
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("DiscordBridge: HTTP request failed for player '%s'."),
					       *EffectivePlayerName);
					return;
				}
				if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
				{
					UE_LOG(LogTemp, Warning,
					       TEXT("DiscordBridge: Discord REST API returned %d: %s"),
					       Resp->GetResponseCode(), *Resp->GetContentAsString());
				}
			});

		Request->ProcessRequest();
	};

	// POST to the main chat channel.
	PostToChannel(Config.ChannelId);

	// When a dedicated whitelist channel is configured, also post there for
	// players who are on the whitelist (so whitelisted members have their own
	// channel view of whitelisted player activity).
	if (!Config.WhitelistChannelId.IsEmpty() &&
	    Config.WhitelistChannelId != Config.ChannelId &&
	    FWhitelistManager::IsWhitelisted(EffectivePlayerName))
	{
		PostToChannel(Config.WhitelistChannelId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord → Game chat relay
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::RelayDiscordMessageToGame(const FString& Username,
                                                         const FString& Message)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: No world available – cannot relay Discord message to game chat."));
		return;
	}

	AFGChatManager* ChatManager = AFGChatManager::Get(World);
	if (!ChatManager)
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: ChatManager not found – cannot relay Discord message to game chat."));
		return;
	}

	// Apply the configurable DiscordToGameFormat string to produce the full
	// in-game chat line.  Placeholders: %Username% / %PlayerName% (alias), %Message%.
	// Use a fallback if the format is empty so the message is never silently
	// dropped due to a misconfigured INI.
	FString FormattedMessage = Config.DiscordToGameFormat;
	FormattedMessage = FormattedMessage.Replace(TEXT("%Username%"),   *Username);
	FormattedMessage = FormattedMessage.Replace(TEXT("%PlayerName%"), *Username);
	FormattedMessage = FormattedMessage.Replace(TEXT("%Message%"),    *Message);

	if (FormattedMessage.IsEmpty())
	{
		// Format produced an empty result – fall back to the raw message so the
		// content is always visible rather than silently dropped.
		FormattedMessage = Message;
	}

	FChatMessageStruct ChatMsg;
	// Use CMT_CustomMessage so the game's chat widget renders the message body
	// (MessageText) without requiring a real player controller.  The sender column
	// (MessageSender) is left blank because DiscordToGameFormat now controls the
	// full line of text, including the Discord username prefix.
	// CMT_CustomMessage also means the Game→Discord diff loop (which only processes
	// CMT_PlayerMessage entries) will naturally ignore these relay messages.
	ChatMsg.MessageType   = EFGChatMessageType::CMT_CustomMessage;
	ChatMsg.MessageSender = FText::GetEmpty();
	ChatMsg.MessageText   = FText::FromString(FormattedMessage);

	ChatManager->BroadcastChatMessage(ChatMsg);

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Relayed to game chat – text: '%s'"),
	       *FormattedMessage);
}

// ─────────────────────────────────────────────────────────────────────────────
// Whitelist and ban enforcement
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller)
{
	if (!Controller || Controller->IsLocalController())
	{
		return;
	}

	const APlayerState* PS = Controller->GetPlayerState<APlayerState>();
	const FString PlayerName = PS ? PS->GetPlayerName() : FString();

	// In Satisfactory's CSS UE build, the player name is populated asynchronously
	// from Epic Online Services (via Server_SetPlayerNames RPC) after PostLogin fires.
	// When enforcement is active and the name is not yet available, schedule a
	// one-shot deferred retry rather than silently allowing the player through.
	if (PlayerName.IsEmpty())
	{
		const bool bEnforcementActive = FWhitelistManager::IsEnabled() || FBanManager::IsEnabled();
		if (bEnforcementActive)
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("DiscordBridge: player joined with an empty name – scheduling deferred whitelist/ban check."));

			if (UWorld* World = GetWorld())
			{
				TWeakObjectPtr<AGameModeBase>     WeakGM(GameMode);
				TWeakObjectPtr<APlayerController> WeakPC(Controller);
				TWeakObjectPtr<UWorld>            WeakWorld(World);

				// Retry every 0.5 s, up to MaxRetries times.  A shared handle
				// lets the lambda cancel the repeating timer once done.
				TSharedRef<FTimerHandle> SharedHandle = MakeShared<FTimerHandle>();
				TSharedRef<int32>        RetriesLeft  = MakeShared<int32>(10);

				World->GetTimerManager().SetTimer(*SharedHandle,
					FTimerDelegate::CreateWeakLambda(this,
						[this, WeakGM, WeakPC, WeakWorld, SharedHandle, RetriesLeft]()
					{
						UWorld* W = WeakWorld.Get();

						if (!WeakPC.IsValid())
						{
							// Player already disconnected – stop the timer.
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						const APlayerState* RetryPS   = WeakPC->GetPlayerState<APlayerState>();
						const FString       RetryName = RetryPS ? RetryPS->GetPlayerName() : FString();

						if (!RetryName.IsEmpty())
						{
							// Name is now available – run the whitelist/ban check and stop the timer.
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							OnPostLogin(WeakGM.Get(), WeakPC.Get());
							return;
						}

						--(*RetriesLeft);
						if (*RetriesLeft > 0)
						{
							return; // Name not yet populated – try again next tick.
						}

						// All retries exhausted; kick to enforce whitelist/ban integrity (fail-closed).
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						UE_LOG(LogTemp, Warning,
						       TEXT("DiscordBridge: player name still empty after deferred check – kicking to enforce whitelist/ban."));

						AGameModeBase* FallbackGM = WeakGM.IsValid() ? WeakGM.Get() : nullptr;
						if (!FallbackGM && W)
						{
							FallbackGM = W->GetAuthGameMode<AGameModeBase>();
						}
						if (FallbackGM && FallbackGM->GameSession)
						{
							FallbackGM->GameSession->KickPlayer(
								WeakPC.Get(),
								FText::FromString(
									TEXT("Unable to verify player identity. Please reconnect.")));
						}
					}),
					0.5f, true);
			}
		}
		else
		{
			UE_LOG(LogTemp, Warning,
			       TEXT("DiscordBridge: player joined with an empty name – skipping check (enforcement disabled)."));
		}
		return;
	}

	// ── Resolve the authoritative game mode ──────────────────────────────────
	// When OnPostLogin is called from the deferred-name retry lambda the GameMode
	// parameter may be null (weak pointer expired).  Fall back to the world's
	// auth game mode so the kick can always be issued.
	AGameModeBase* EffectiveGameMode = GameMode;
	if (!EffectiveGameMode || !EffectiveGameMode->GameSession)
	{
		if (UWorld* W = Controller->GetWorld())
		{
			EffectiveGameMode = W->GetAuthGameMode<AGameModeBase>();
		}
	}

	// ── Ban check (takes priority over whitelist) ─────────────────────────────
	if (FBanManager::IsEnabled() && FBanManager::IsBanned(PlayerName))
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge BanSystem: kicking banned player '%s'"), *PlayerName);

		if (EffectiveGameMode && EffectiveGameMode->GameSession)
		{
			const FString KickReason = Config.BanKickReason.IsEmpty()
				? TEXT("You are banned from this server.")
				: Config.BanKickReason;
			EffectiveGameMode->GameSession->KickPlayer(
				Controller,
				FText::FromString(KickReason));
		}

		// Notify Discord so admins can see the ban kick in the bridge channel.
		if (!Config.BanKickDiscordMessage.IsEmpty())
		{
			FString Notice = Config.BanKickDiscordMessage;
			Notice = Notice.Replace(TEXT("%PlayerName%"), *PlayerName);
			SendMessageToChannel(Config.ChannelId, Notice);
			// Also notify the dedicated ban channel (if configured) so admins
			// have a focused audit log of all ban-related events.
			if (!Config.BanChannelId.IsEmpty() && Config.BanChannelId != Config.ChannelId)
			{
				SendMessageToChannel(Config.BanChannelId, Notice);
			}
		}
		return;
	}

	// ── Whitelist check ───────────────────────────────────────────────────────
	if (!FWhitelistManager::IsEnabled())
	{
		return;
	}

	if (FWhitelistManager::IsWhitelisted(PlayerName))
	{
		return;
	}

	// Secondary pass: if WhitelistRoleId is configured, also allow any player
	// whose in-game name matches a Discord display name (nick / global_name /
	// username) of a guild member who holds the whitelist role.  The cache is
	// populated asynchronously after the bot connects; players who join before
	// it is ready follow the JSON-only path above.
	if (!Config.WhitelistRoleId.IsEmpty() &&
	    WhitelistRoleMemberNames.Contains(PlayerName.ToLower()))
	{
		UE_LOG(LogTemp, Log,
		       TEXT("DiscordBridge Whitelist: allowing '%s' – matches a Discord member with WhitelistRoleId."),
		       *PlayerName);
		return;
	}

	UE_LOG(LogTemp, Warning,
	       TEXT("DiscordBridge Whitelist: kicking non-whitelisted player '%s'"), *PlayerName);

	if (EffectiveGameMode && EffectiveGameMode->GameSession)
	{
		const FString KickReason = Config.WhitelistKickReason.IsEmpty()
			? TEXT("You are not on this server's whitelist. Contact the server admin to be added.")
			: Config.WhitelistKickReason;
		EffectiveGameMode->GameSession->KickPlayer(
			Controller,
			FText::FromString(KickReason));
	}

	// Notify Discord so admins can see the kick in the bridge channel.
	if (!Config.WhitelistKickDiscordMessage.IsEmpty())
	{
		FString Notice = Config.WhitelistKickDiscordMessage;
		Notice = Notice.Replace(TEXT("%PlayerName%"), *PlayerName);
		SendMessageToChannel(Config.ChannelId, Notice);
	}
}

void UDiscordBridgeSubsystem::HandleWhitelistCommand(const FString& SubCommand,
                                                      const FString& DiscordUsername,
                                                      const FString& ResponseChannelId)
{
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Whitelist command from '%s': '%s'"), *DiscordUsername, *SubCommand);

	FString Response;

	// Split sub-command into verb + optional argument.
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
		Response = FString::Printf(
			TEXT(":white_check_mark: Whitelist **enabled**. Only whitelisted players can join.\n"
			     ":information_source: Ban system is **%s** (independent — use `%s on/off` to toggle it)."),
			FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"),
			*Config.BanCommandPrefix);
	}
	else if (Verb == TEXT("off"))
	{
		FWhitelistManager::SetEnabled(false);
		Response = FString::Printf(
			TEXT(":no_entry_sign: Whitelist **disabled**. All players can join freely.\n"
			     ":information_source: Ban system is **%s** (independent — use `%s on/off` to toggle it)."),
			FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"),
			*Config.BanCommandPrefix);
	}
	else if (Verb == TEXT("add"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!whitelist add <PlayerName>`");
		}
		else if (FWhitelistManager::AddPlayer(Arg))
		{
			Response = FString::Printf(TEXT(":green_circle: **%s** has been added to the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT(":yellow_circle: **%s** is already on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("remove"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!whitelist remove <PlayerName>`");
		}
		else if (FWhitelistManager::RemovePlayer(Arg))
		{
			Response = FString::Printf(TEXT(":red_circle: **%s** has been removed from the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT(":yellow_circle: **%s** was not on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("list"))
	{
		const TArray<FString> All = FWhitelistManager::GetAll();
		const FString Status = FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		if (All.Num() == 0)
		{
			Response = FString::Printf(TEXT(":scroll: Whitelist is **%s**. No players listed."), *Status);
		}
		else
		{
			Response = FString::Printf(
				TEXT(":scroll: Whitelist is **%s**. Players (%d): %s"),
				*Status, All.Num(), *FString::Join(All, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("status"))
	{
		const FString WhitelistState = FWhitelistManager::IsEnabled()
			? TEXT(":white_check_mark: Whitelist: **ENABLED**")
			: TEXT(":no_entry_sign: Whitelist: **disabled**");
		const FString BanState = FBanManager::IsEnabled()
			? TEXT(":hammer: Ban system: **ENABLED**")
			: TEXT(":unlock: Ban system: **disabled**");
		Response = FString::Printf(
			TEXT("**Server access control (each system is independent):**\n%s\n%s"),
			*WhitelistState, *BanState);
	}
	else if (Verb == TEXT("role"))
	{
		// Sub-sub-command: role add <discord_user_id> / role remove <discord_user_id>
		FString RoleVerb, TargetUserId;
		if (!Arg.Split(TEXT(" "), &RoleVerb, &TargetUserId, ESearchCase::IgnoreCase))
		{
			RoleVerb     = Arg.TrimStartAndEnd();
			TargetUserId = TEXT("");
		}
		RoleVerb     = RoleVerb.TrimStartAndEnd().ToLower();
		TargetUserId = TargetUserId.TrimStartAndEnd();

		if (Config.WhitelistRoleId.IsEmpty())
		{
			Response = TEXT(":warning: `WhitelistRoleId` is not configured in `DefaultDiscordBridge.ini`. "
			                "Set it to the snowflake ID of the whitelist role.");
		}
		else if (GuildId.IsEmpty())
		{
			Response = TEXT(":warning: Guild ID not yet available. Try again in a moment.");
		}
		else if (TargetUserId.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!whitelist role add <discord_user_id>` "
			                "or `!whitelist role remove <discord_user_id>`");
		}
		else if (RoleVerb == TEXT("add"))
		{
			ModifyDiscordRole(TargetUserId, Config.WhitelistRoleId, /*bGrant=*/true);
			Response = FString::Printf(
				TEXT(":green_circle: Granting whitelist role to Discord user `%s`…"), *TargetUserId);
		}
		else if (RoleVerb == TEXT("remove"))
		{
			ModifyDiscordRole(TargetUserId, Config.WhitelistRoleId, /*bGrant=*/false);
			Response = FString::Printf(
				TEXT(":red_circle: Revoking whitelist role from Discord user `%s`…"), *TargetUserId);
		}
		else
		{
			Response = TEXT(":question: Usage: `!whitelist role add <discord_user_id>` "
			                "or `!whitelist role remove <discord_user_id>`");
		}
	}
	else
	{
		Response = TEXT(":question: Unknown whitelist command. Available: `on`, `off`, "
		                "`add <name>`, `remove <name>`, `list`, `status`, "
		                "`role add <discord_id>`, `role remove <discord_id>`.");
	}

	// Send the response back to the channel where the command was received.
	const FString& ReplyChannel = ResponseChannelId.IsEmpty() ? Config.ChannelId : ResponseChannelId;
	SendMessageToChannel(ReplyChannel, Response);
}

void UDiscordBridgeSubsystem::ModifyDiscordRole(const FString& UserId, const FString& RoleId, bool bGrant)
{
	if (RoleId.IsEmpty() || GuildId.IsEmpty() || Config.BotToken.IsEmpty())
	{
		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge: ModifyDiscordRole: missing RoleId, GuildId, or BotToken."));
		return;
	}

	// PUT  = grant the role
	// DELETE = revoke the role
	const FString Verb = bGrant ? TEXT("PUT") : TEXT("DELETE");
	const FString Url = FString::Printf(
		TEXT("%s/guilds/%s/members/%s/roles/%s"),
		*DiscordApiBase, *GuildId, *UserId, *RoleId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));
	// PUT with empty body still needs a Content-Type header to avoid 411.
	Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
	Request->SetContentAsString(TEXT(""));

	const bool bGrantCopy = bGrant;
	const FString UserIdCopy = UserId;
	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[bGrantCopy, UserIdCopy](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: Role %s request failed for user '%s'."),
				       bGrantCopy ? TEXT("grant") : TEXT("revoke"), *UserIdCopy);
				return;
			}
			// 204 No Content is the success response for both PUT and DELETE role endpoints.
			if (Resp->GetResponseCode() != 204)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: Role %s for user '%s' returned HTTP %d: %s"),
				       bGrantCopy ? TEXT("grant") : TEXT("revoke"), *UserIdCopy,
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
			else
			{
				UE_LOG(LogTemp, Log,
				       TEXT("DiscordBridge: Role %s succeeded for user '%s'."),
				       bGrantCopy ? TEXT("grant") : TEXT("revoke"), *UserIdCopy);
			}
		});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord role member cache helpers
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::RebuildWhitelistRoleNameSet()
{
	WhitelistRoleMemberNames.Empty();
	for (const TPair<FString, TArray<FString>>& Pair : RoleMemberIdToNames)
	{
		for (const FString& Name : Pair.Value)
		{
			WhitelistRoleMemberNames.Add(Name);
		}
	}
}

void UDiscordBridgeSubsystem::UpdateWhitelistRoleMemberEntry(
	const TSharedPtr<FJsonObject>& MemberObj, bool bRemoved)
{
	if (!MemberObj.IsValid())
	{
		return;
	}

	// Extract Discord user ID – present in both member objects and remove events.
	const TSharedPtr<FJsonObject>* UserPtr = nullptr;
	if (!MemberObj->TryGetObjectField(TEXT("user"), UserPtr) || !UserPtr)
	{
		return;
	}
	FString UserId;
	if (!(*UserPtr)->TryGetStringField(TEXT("id"), UserId) || UserId.IsEmpty())
	{
		return;
	}

	if (bRemoved)
	{
		// Member left the guild – remove any cached entry and rebuild.
		if (RoleMemberIdToNames.Remove(UserId) > 0)
		{
			RebuildWhitelistRoleNameSet();
		}
		return;
	}

	// Determine whether this member currently holds WhitelistRoleId.
	bool bHasRole = false;
	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	if (MemberObj->TryGetArrayField(TEXT("roles"), Roles) && Roles)
	{
		for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
		{
			FString RoleId;
			if (RoleVal->TryGetString(RoleId) && RoleId == Config.WhitelistRoleId)
			{
				bHasRole = true;
				break;
			}
		}
	}

	if (!bHasRole)
	{
		// Member no longer holds the role – remove from cache if present.
		if (RoleMemberIdToNames.Remove(UserId) > 0)
		{
			RebuildWhitelistRoleNameSet();
		}
		return;
	}

	// Collect all display names for this member (server nick, global name, username).
	TArray<FString> Names;
	FString Nick;
	if (MemberObj->TryGetStringField(TEXT("nick"), Nick) && !Nick.IsEmpty())
	{
		Names.AddUnique(Nick.ToLower());
	}
	FString GlobalName;
	if ((*UserPtr)->TryGetStringField(TEXT("global_name"), GlobalName) && !GlobalName.IsEmpty())
	{
		Names.AddUnique(GlobalName.ToLower());
	}
	FString Username;
	if ((*UserPtr)->TryGetStringField(TEXT("username"), Username) && !Username.IsEmpty())
	{
		Names.AddUnique(Username.ToLower());
	}

	if (Names.Num() == 0)
	{
		return;
	}

	RoleMemberIdToNames.FindOrAdd(UserId) = Names;
	RebuildWhitelistRoleNameSet();

	UE_LOG(LogTemp, Verbose,
	       TEXT("DiscordBridge: Whitelist role cache updated for user '%s' (%s)."),
	       *UserId, *FString::Join(Names, TEXT(", ")));
}

void UDiscordBridgeSubsystem::FetchWhitelistRoleMembers()
{
	if (Config.WhitelistRoleId.IsEmpty() || GuildId.IsEmpty() || Config.BotToken.IsEmpty())
	{
		return;
	}

	// Fetch up to 1000 guild members; sufficient for most gaming communities.
	// Discord's maximum per-request limit for this endpoint is 1000.
	const FString Url = FString::Printf(
		TEXT("%s/guilds/%s/members?limit=1000"),
		*DiscordApiBase, *GuildId);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("GET"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *Config.BotToken));

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: FetchWhitelistRoleMembers request failed."));
				return;
			}
			if (Resp->GetResponseCode() != 200)
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: FetchWhitelistRoleMembers returned HTTP %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
				return;
			}

			// Parse the JSON array of member objects.
			TArray<TSharedPtr<FJsonValue>> Members;
			const TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(Resp->GetContentAsString());
			if (!FJsonSerializer::Deserialize(Reader, Members))
			{
				UE_LOG(LogTemp, Warning,
				       TEXT("DiscordBridge: FetchWhitelistRoleMembers – failed to parse JSON."));
				return;
			}

			// Clear the cache and rebuild it from the full member list.
			RoleMemberIdToNames.Empty();
			for (const TSharedPtr<FJsonValue>& Val : Members)
			{
				const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
				if (Val->TryGetObject(MemberPtr) && MemberPtr)
				{
					UpdateWhitelistRoleMemberEntry(*MemberPtr);
				}
			}

			UE_LOG(LogTemp, Log,
			       TEXT("DiscordBridge: Whitelist role cache built – %d member(s) hold WhitelistRoleId (%d name(s) cached)."),
			       RoleMemberIdToNames.Num(), WhitelistRoleMemberNames.Num());
		});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Ban system command handler
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleBanCommand(const FString& SubCommand,
                                                const FString& DiscordUsername,
                                                const FString& ResponseChannelId)
{
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: Ban command from '%s': '%s'"), *DiscordUsername, *SubCommand);

	FString Response;

	// Split sub-command into verb + optional argument.
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
		FBanManager::SetEnabled(true);
		const int32 Kicked = KickConnectedBannedPlayers();
		Response = FString::Printf(
			TEXT(":hammer: Ban system **enabled**. Banned players will be kicked on join.%s\n"
			     ":information_source: Whitelist is **%s** (independent — use `%s on/off` to toggle it)."),
			Kicked > 0
				? *FString::Printf(TEXT(" Kicked **%d** already-connected banned player(s)."), Kicked)
				: TEXT(""),
			FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"),
			*Config.WhitelistCommandPrefix);
	}
	else if (Verb == TEXT("off"))
	{
		FBanManager::SetEnabled(false);
		Response = FString::Printf(
			TEXT(":unlock: Ban system **disabled**. Banned players can join freely.\n"
			     ":information_source: Whitelist is **%s** (independent — use `%s on/off` to toggle it)."),
			FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"),
			*Config.WhitelistCommandPrefix);
	}
	else if (Verb == TEXT("add"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!ban add <PlayerName>`");
		}
		else if (FBanManager::BanPlayer(Arg))
		{
			if (FBanManager::IsEnabled())
			{
				const int32 Kicked = KickConnectedBannedPlayers(Arg);
				Response = FString::Printf(
					TEXT(":hammer: **%s** has been banned from the server.%s"),
					*Arg,
					Kicked > 0 ? TEXT(" They have been kicked.") : TEXT(""));
			}
			else
			{
				Response = FString::Printf(
					TEXT(":hammer: **%s** has been added to the ban list.\n"
					     ":warning: The ban system is currently **disabled** — run `!ban on` to enforce bans."),
					*Arg);
			}
		}
		else
		{
			Response = FString::Printf(TEXT(":yellow_circle: **%s** is already banned."), *Arg);
		}
	}
	else if (Verb == TEXT("remove"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!ban remove <PlayerName>`");
		}
		else if (FBanManager::UnbanPlayer(Arg))
		{
			Response = FString::Printf(TEXT(":white_check_mark: **%s** has been unbanned."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT(":yellow_circle: **%s** was not on the ban list."), *Arg);
		}
	}
	else if (Verb == TEXT("list"))
	{
		const TArray<FString> All = FBanManager::GetAll();
		const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		if (All.Num() == 0)
		{
			Response = FString::Printf(TEXT(":scroll: Ban system is **%s**. No players banned."), *Status);
		}
		else
		{
			Response = FString::Printf(
				TEXT(":scroll: Ban system is **%s**. Banned players (%d): %s"),
				*Status, All.Num(), *FString::Join(All, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("status"))
	{
		const FString BanState = FBanManager::IsEnabled()
			? TEXT(":hammer: Ban system: **ENABLED**")
			: TEXT(":unlock: Ban system: **disabled**");
		const FString WhitelistState = FWhitelistManager::IsEnabled()
			? TEXT(":white_check_mark: Whitelist: **ENABLED**")
			: TEXT(":no_entry_sign: Whitelist: **disabled**");
		Response = FString::Printf(
			TEXT("**Server access control (each system is independent):**\n%s\n%s"),
			*BanState, *WhitelistState);
	}
	else if (Verb == TEXT("role"))
	{
		// Sub-sub-command: role add <discord_user_id> / role remove <discord_user_id>
		FString RoleVerb, TargetUserId;
		if (!Arg.Split(TEXT(" "), &RoleVerb, &TargetUserId, ESearchCase::IgnoreCase))
		{
			RoleVerb     = Arg.TrimStartAndEnd();
			TargetUserId = TEXT("");
		}
		RoleVerb     = RoleVerb.TrimStartAndEnd().ToLower();
		TargetUserId = TargetUserId.TrimStartAndEnd();

		if (Config.BanCommandRoleId.IsEmpty())
		{
			Response = TEXT(":warning: `BanCommandRoleId` is not configured in `DefaultDiscordBridge.ini`. "
			                "Set it to the snowflake ID of the ban admin role you want to grant/revoke.");
		}
		else if (GuildId.IsEmpty())
		{
			Response = TEXT(":warning: Guild ID not yet available. Try again in a moment.");
		}
		else if (TargetUserId.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!ban role add <discord_user_id>` "
			                "or `!ban role remove <discord_user_id>`");
		}
		else if (RoleVerb == TEXT("add"))
		{
			ModifyDiscordRole(TargetUserId, Config.BanCommandRoleId, /*bGrant=*/true);
			Response = FString::Printf(
				TEXT(":green_circle: Granting ban role to Discord user `%s`…"), *TargetUserId);
		}
		else if (RoleVerb == TEXT("remove"))
		{
			ModifyDiscordRole(TargetUserId, Config.BanCommandRoleId, /*bGrant=*/false);
			Response = FString::Printf(
				TEXT(":red_circle: Revoking ban role from Discord user `%s`…"), *TargetUserId);
		}
		else
		{
			Response = TEXT(":question: Usage: `!ban role add <discord_user_id>` "
			                "or `!ban role remove <discord_user_id>`");
		}
	}
	else
	{
		Response = TEXT(":question: Unknown ban command. Available: `on`, `off`, "
		                "`add <name>`, `remove <name>`, `list`, `status`, "
		                "`role add <discord_id>`, `role remove <discord_id>`.");
	}

	// Send the response back to Discord on the channel where the command was issued.
	// If no explicit response channel was provided, fall back to the main channel.
	const FString EffectiveChannel = ResponseChannelId.IsEmpty() ? Config.ChannelId : ResponseChannelId;
	SendMessageToChannel(EffectiveChannel, Response);
}

// ─────────────────────────────────────────────────────────────────────────────
// In-game chat command helpers
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::SendGameChatStatusMessage(const FString& Message)
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

int32 UDiscordBridgeSubsystem::KickConnectedBannedPlayers(const FString& PlayerName)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return 0;
	}

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS)
	{
		return 0;
	}

	AGameModeBase* GM = World->GetAuthGameMode<AGameModeBase>();
	if (!GM || !GM->GameSession)
	{
		return 0;
	}

	const FString KickReason = Config.BanKickReason.IsEmpty()
		? TEXT("You are banned from this server.")
		: Config.BanKickReason;

	const bool bTargetSpecific = !PlayerName.IsEmpty();
	const FString LowerTarget  = PlayerName.ToLower();

	int32 KickedCount = 0;
	for (APlayerState* PS : GS->PlayerArray)
	{
		if (!PS)
		{
			continue;
		}

		const FString ConnectedName = PS->GetPlayerName();
		if (ConnectedName.IsEmpty())
		{
			continue;
		}

		// When a specific name was requested, only match that player.
		// Otherwise kick anyone whose name is on the ban list.
		const bool bMatch = bTargetSpecific
			? (ConnectedName.ToLower() == LowerTarget)
			: FBanManager::IsBanned(ConnectedName);

		if (!bMatch)
		{
			continue;
		}

		APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
		if (!PC || PC->IsLocalController())
		{
			continue;
		}

		UE_LOG(LogTemp, Warning,
		       TEXT("DiscordBridge BanSystem: kicking connected banned player '%s'"),
		       *ConnectedName);

		GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
		++KickedCount;
	}

	return KickedCount;
}

void UDiscordBridgeSubsystem::HandleInGameWhitelistCommand(const FString& SubCommand)
{
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: In-game whitelist command: '%s'"), *SubCommand);

	FString Response;

	// Split sub-command into verb + optional argument.
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
		Response = FString::Printf(
			TEXT("Whitelist ENABLED. Only whitelisted players can join. "
			     "(Ban system is %s — independent.)"),
			FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"));
	}
	else if (Verb == TEXT("off"))
	{
		FWhitelistManager::SetEnabled(false);
		Response = FString::Printf(
			TEXT("Whitelist DISABLED. All players can join freely. "
			     "(Ban system is %s — independent.)"),
			FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"));
	}
	else if (Verb == TEXT("add"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("Usage: !whitelist add <PlayerName>");
		}
		else if (FWhitelistManager::AddPlayer(Arg))
		{
			Response = FString::Printf(TEXT("%s has been added to the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT("%s is already on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("remove"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("Usage: !whitelist remove <PlayerName>");
		}
		else if (FWhitelistManager::RemovePlayer(Arg))
		{
			Response = FString::Printf(TEXT("%s has been removed from the whitelist."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT("%s was not on the whitelist."), *Arg);
		}
	}
	else if (Verb == TEXT("list"))
	{
		const TArray<FString> All = FWhitelistManager::GetAll();
		const FString Status = FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		if (All.Num() == 0)
		{
			Response = FString::Printf(TEXT("Whitelist is %s. No players listed."), *Status);
		}
		else
		{
			Response = FString::Printf(
				TEXT("Whitelist is %s. Players (%d): %s"),
				*Status, All.Num(), *FString::Join(All, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("status"))
	{
		const FString WhitelistState = FWhitelistManager::IsEnabled()
			? TEXT("ENABLED") : TEXT("disabled");
		const FString BanState = FBanManager::IsEnabled()
			? TEXT("ENABLED") : TEXT("disabled");
		Response = FString::Printf(
			TEXT("Whitelist: %s | Ban system: %s  (each system is independent)"),
			*WhitelistState, *BanState);
	}
	else
	{
		Response = TEXT("Unknown whitelist command. Available: on, off, add <name>, remove <name>, list, status.");
	}

	SendGameChatStatusMessage(Response);
}

void UDiscordBridgeSubsystem::HandleInGameBanCommand(const FString& SubCommand)
{
	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: In-game ban command: '%s'"), *SubCommand);

	FString Response;

	// Split sub-command into verb + optional argument.
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
		FBanManager::SetEnabled(true);
		const int32 Kicked = KickConnectedBannedPlayers();
		FString KickedNote;
		if (Kicked > 0)
		{
			KickedNote = FString::Printf(TEXT(" Kicked %d already-connected banned player(s)."), Kicked);
		}
		Response = FString::Printf(
			TEXT("Ban system ENABLED. Banned players will be kicked on join.%s "
			     "(Whitelist is %s — independent.)"),
			*KickedNote,
			FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"));
	}
	else if (Verb == TEXT("off"))
	{
		FBanManager::SetEnabled(false);
		Response = FString::Printf(
			TEXT("Ban system DISABLED. Banned players can join freely. "
			     "(Whitelist is %s — independent.)"),
			FWhitelistManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled"));
	}
	else if (Verb == TEXT("add"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("Usage: !ban add <PlayerName>");
		}
		else if (FBanManager::BanPlayer(Arg))
		{
			if (FBanManager::IsEnabled())
			{
				const int32 Kicked = KickConnectedBannedPlayers(Arg);
				Response = FString::Printf(
					TEXT("%s has been banned from the server.%s"),
					*Arg,
					Kicked > 0 ? TEXT(" They have been kicked.") : TEXT(""));
			}
			else
			{
				Response = FString::Printf(
					TEXT("%s has been added to the ban list. "
					     "WARNING: ban system is disabled — run !ban on to enforce bans."),
					*Arg);
			}
		}
		else
		{
			Response = FString::Printf(TEXT("%s is already banned."), *Arg);
		}
	}
	else if (Verb == TEXT("remove"))
	{
		if (Arg.IsEmpty())
		{
			Response = TEXT("Usage: !ban remove <PlayerName>");
		}
		else if (FBanManager::UnbanPlayer(Arg))
		{
			Response = FString::Printf(TEXT("%s has been unbanned."), *Arg);
		}
		else
		{
			Response = FString::Printf(TEXT("%s was not on the ban list."), *Arg);
		}
	}
	else if (Verb == TEXT("list"))
	{
		const TArray<FString> All = FBanManager::GetAll();
		const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
		if (All.Num() == 0)
		{
			Response = FString::Printf(TEXT("Ban system is %s. No players banned."), *Status);
		}
		else
		{
			Response = FString::Printf(
				TEXT("Ban system is %s. Banned players (%d): %s"),
				*Status, All.Num(), *FString::Join(All, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("status"))
	{
		const FString BanState = FBanManager::IsEnabled()
			? TEXT("ENABLED") : TEXT("disabled");
		const FString WhitelistState = FWhitelistManager::IsEnabled()
			? TEXT("ENABLED") : TEXT("disabled");
		Response = FString::Printf(
			TEXT("Ban system: %s | Whitelist: %s  (each system is independent)"),
			*BanState, *WhitelistState);
	}
	else
	{
		Response = TEXT("Unknown ban command. Available: on, off, add <name>, remove <name>, list, status.");
	}

	SendGameChatStatusMessage(Response);
}

// ─────────────────────────────────────────────────────────────────────────────
// IBanNotificationProvider – implementation
// Forwards ban/unban events from the BanSystem mod to Discord.
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::OnSteamPlayerBanned(const FString& Steam64Id,
                                                   const FBanEntry& Entry)
{
	if (Config.BanSystemSteamBanDiscordMessage.IsEmpty())
	{
		return;
	}

	FString Notice = Config.BanSystemSteamBanDiscordMessage;
	Notice = Notice.Replace(TEXT("%PlayerId%"), *Steam64Id);
	Notice = Notice.Replace(TEXT("%Reason%"),   *Entry.Reason);
	Notice = Notice.Replace(TEXT("%BannedBy%"), *Entry.BannedBy);

	SendMessageToChannel(Config.ChannelId, Notice);
	if (!Config.BanChannelId.IsEmpty() && Config.BanChannelId != Config.ChannelId)
	{
		SendMessageToChannel(Config.BanChannelId, Notice);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: BanSystem Steam ban notification posted for ID %s."),
	       *Steam64Id);
}

void UDiscordBridgeSubsystem::OnSteamPlayerUnbanned(const FString& Steam64Id)
{
	if (Config.BanSystemSteamUnbanDiscordMessage.IsEmpty())
	{
		return;
	}

	FString Notice = Config.BanSystemSteamUnbanDiscordMessage;
	Notice = Notice.Replace(TEXT("%PlayerId%"), *Steam64Id);

	SendMessageToChannel(Config.ChannelId, Notice);
	if (!Config.BanChannelId.IsEmpty() && Config.BanChannelId != Config.ChannelId)
	{
		SendMessageToChannel(Config.BanChannelId, Notice);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: BanSystem Steam unban notification posted for ID %s."),
	       *Steam64Id);
}

void UDiscordBridgeSubsystem::OnEOSPlayerBanned(const FString& EOSProductUserId,
                                                 const FBanEntry& Entry)
{
	if (Config.BanSystemEOSBanDiscordMessage.IsEmpty())
	{
		return;
	}

	FString Notice = Config.BanSystemEOSBanDiscordMessage;
	Notice = Notice.Replace(TEXT("%PlayerId%"), *EOSProductUserId);
	Notice = Notice.Replace(TEXT("%Reason%"),   *Entry.Reason);
	Notice = Notice.Replace(TEXT("%BannedBy%"), *Entry.BannedBy);

	SendMessageToChannel(Config.ChannelId, Notice);
	if (!Config.BanChannelId.IsEmpty() && Config.BanChannelId != Config.ChannelId)
	{
		SendMessageToChannel(Config.BanChannelId, Notice);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: BanSystem EOS ban notification posted for ID %s."),
	       *EOSProductUserId);
}

void UDiscordBridgeSubsystem::OnEOSPlayerUnbanned(const FString& EOSProductUserId)
{
	if (Config.BanSystemEOSUnbanDiscordMessage.IsEmpty())
	{
		return;
	}

	FString Notice = Config.BanSystemEOSUnbanDiscordMessage;
	Notice = Notice.Replace(TEXT("%PlayerId%"), *EOSProductUserId);

	SendMessageToChannel(Config.ChannelId, Notice);
	if (!Config.BanChannelId.IsEmpty() && Config.BanChannelId != Config.ChannelId)
	{
		SendMessageToChannel(Config.BanChannelId, Notice);
	}

	UE_LOG(LogTemp, Log,
	       TEXT("DiscordBridge: BanSystem EOS unban notification posted for ID %s."),
	       *EOSProductUserId);
}

// ─────────────────────────────────────────────────────────────────────────────
// IDiscordBridgeProvider – subscribe / unsubscribe helpers
// These wrap the native multicast delegates so external modules (TicketSystem
// and any other future mod built against the IDiscordBridgeProvider interface)
// can subscribe without including DiscordBridgeSubsystem.h directly.
// ─────────────────────────────────────────────────────────────────────────────

FDelegateHandle UDiscordBridgeSubsystem::SubscribeInteraction(
TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
return OnDiscordInteractionReceived.AddLambda(MoveTemp(Callback));
}

void UDiscordBridgeSubsystem::UnsubscribeInteraction(FDelegateHandle Handle)
{
OnDiscordInteractionReceived.Remove(Handle);
}

FDelegateHandle UDiscordBridgeSubsystem::SubscribeRawMessage(
TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
return OnDiscordRawMessageReceived.AddLambda(MoveTemp(Callback));
}

void UDiscordBridgeSubsystem::UnsubscribeRawMessage(FDelegateHandle Handle)
{
OnDiscordRawMessageReceived.Remove(Handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// IBanDiscordCommandProvider — implemented here so UBanDiscordSubsystem in the
// BanSystem mod can subscribe to MESSAGE_CREATE events and send channel
// messages without a compile-time dependency on DiscordBridge.
// SubscribeDiscordMessages / UnsubscribeDiscordMessages delegate to the same
// OnDiscordRawMessageReceived multicast that IDiscordBridgeProvider exposes via
// SubscribeRawMessage / UnsubscribeRawMessage, keeping a single subscription
// list for all consumers of raw Discord messages.
// ─────────────────────────────────────────────────────────────────────────────

FDelegateHandle UDiscordBridgeSubsystem::SubscribeDiscordMessages(
	TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
	return OnDiscordRawMessageReceived.AddLambda(MoveTemp(Callback));
}

void UDiscordBridgeSubsystem::UnsubscribeDiscordMessages(FDelegateHandle Handle)
{
	OnDiscordRawMessageReceived.Remove(Handle);
}
