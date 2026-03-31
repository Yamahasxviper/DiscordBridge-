// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "BanPlayerLookup.h"
#include "BanEnforcementSubsystem.h"
#include "EOSConnectSubsystem.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HttpModule.h"
#include "Interfaces/IHttpResponse.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanDiscord, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// Constants
// ─────────────────────────────────────────────────────────────────────────────

const FString UBanDiscordSubsystem::DiscordGatewayUrl =
	TEXT("wss://gateway.discord.gg/?v=10&encoding=json");

const FString UBanDiscordSubsystem::DiscordApiBase =
	TEXT("https://discord.com/api/v10");

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load (or auto-create) the config from DefaultBanSystem.ini.
	Config = FBanDiscordConfig::LoadOrCreate();

	if (Config.DiscordChannelId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: DiscordChannelId not configured — "
		            "Discord ban commands disabled until a channel ID is set in "
		            "Mods/BanSystem/Config/DefaultBanSystem.ini."));
	}

	// Standalone mode: connect to Discord Gateway with our own bot token when one
	// is provided and no external provider has been injected yet.
	if (!Config.BotToken.IsEmpty() && ExternalProvider == nullptr)
	{
		Connect();
	}
	else if (Config.BotToken.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: BotToken not configured — running in "
		            "shared-connection mode.  Waiting for an external provider "
		            "(e.g. DiscordBridge) to call SetCommandProvider()."));
	}
}

void UBanDiscordSubsystem::Deinitialize()
{
	// Remove any external provider subscription.
	SetCommandProvider(nullptr);

	// Disconnect the standalone Gateway connection.
	Disconnect();

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// External provider registration
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::SetCommandProvider(IBanDiscordCommandProvider* InProvider)
{
	// If the same non-null provider is already registered, nothing to do.
	// Without this guard, calling SetCommandProvider twice with the same provider
	// would add a second MESSAGE_CREATE subscription, causing every Discord
	// message to be dispatched to the command handler twice.
	if (InProvider && InProvider == ExternalProvider)
	{
		return;
	}

	// Unsubscribe from the old external provider.
	if (ExternalProvider && ExternalProvider != InProvider)
	{
		ExternalProvider->UnsubscribeDiscordMessages(ExternalMessageSubscriptionHandle);
		ExternalMessageSubscriptionHandle.Reset();
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Disconnected from external Discord provider."));
	}

	ExternalProvider = InProvider;

	if (!ExternalProvider)
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: External provider cleared."));

		// If we have a standalone bot token, reconnect the built-in Gateway —
		// but only when no WebSocket is already connecting or connected.
		// Without the !WebSocketClient guard, clearing and immediately
		// re-setting a provider would call Connect() while a previous
		// connection was still in progress, potentially creating two sockets.
		if (!Config.BotToken.IsEmpty() && !bGatewayReady && !WebSocketClient)
		{
			Connect();
		}
		return;
	}

	// Pause the built-in standalone connection — the external provider takes over.
	if (WebSocketClient)
	{
		Disconnect();
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Paused standalone Gateway — "
		            "external provider will handle Discord connectivity."));
	}

	// Subscribe to raw message events from the external provider.
	TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
	ExternalMessageSubscriptionHandle = ExternalProvider->SubscribeDiscordMessages(
		[WeakThis](const TSharedPtr<FJsonObject>& MsgObj)
		{
			if (UBanDiscordSubsystem* Self = WeakThis.Get())
			{
				Self->OnDiscordMessageReceived(MsgObj);
			}
		});

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: External provider registered. "
	            "Listening on channel '%s'."),
	       *Config.DiscordChannelId);
}

// ─────────────────────────────────────────────────────────────────────────────
// IBanDiscordCommandProvider — standalone implementation
// ─────────────────────────────────────────────────────────────────────────────

FDelegateHandle UBanDiscordSubsystem::SubscribeDiscordMessages(
	TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
	// When an external provider is active, delegate to it.
	if (ExternalProvider)
	{
		return ExternalProvider->SubscribeDiscordMessages(MoveTemp(Callback));
	}
	// Otherwise subscribe to the built-in multicast.
	return OnRawDiscordMessage.AddLambda(MoveTemp(Callback));
}

void UBanDiscordSubsystem::UnsubscribeDiscordMessages(FDelegateHandle Handle)
{
	if (ExternalProvider)
	{
		ExternalProvider->UnsubscribeDiscordMessages(Handle);
		return;
	}
	OnRawDiscordMessage.Remove(Handle);
}

void UBanDiscordSubsystem::SendDiscordChannelMessage(const FString& ChannelId,
                                                      const FString& Message)
{
	if (ExternalProvider)
	{
		ExternalProvider->SendDiscordChannelMessage(ChannelId, Message);
		return;
	}
	SendMessageToChannelInternal(ChannelId, Message);
}

const FString& UBanDiscordSubsystem::GetGuildOwnerId() const
{
	if (ExternalProvider)
	{
		return ExternalProvider->GetGuildOwnerId();
	}
	return GuildOwnerId;
}

// ─────────────────────────────────────────────────────────────────────────────
// Standalone Gateway — connection lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Connect()
{
	if (Config.BotToken.IsEmpty())
	{
		return;
	}

	if (!WebSocketClient)
	{
		WebSocketClient = USMLWebSocketClient::CreateWebSocketClient(this);
		WebSocketClient->bAutoReconnect = true;

		WebSocketClient->OnConnected.AddDynamic(
			this, &UBanDiscordSubsystem::OnWebSocketConnected);
		WebSocketClient->OnMessage.AddDynamic(
			this, &UBanDiscordSubsystem::OnWebSocketMessage);
		WebSocketClient->OnClosed.AddDynamic(
			this, &UBanDiscordSubsystem::OnWebSocketClosed);
		WebSocketClient->OnError.AddDynamic(
			this, &UBanDiscordSubsystem::OnWebSocketError);
	}

	WebSocketClient->Connect(DiscordGatewayUrl, {}, {});
	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: Connecting to Discord Gateway..."));
}

void UBanDiscordSubsystem::Disconnect()
{
	// Stop heartbeat timer.
	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
	}

	// Cancel any pending one-shot re-identify/resume ticker so it does not
	// fire on a disconnected or GC'd object.
	if (PendingReidentifyHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingReidentifyHandle);
		PendingReidentifyHandle.Reset();
	}

	if (WebSocketClient)
	{
		// Unbind all dynamic delegates BEFORE calling Close() — Close() may
		// fire OnClosed synchronously (or very shortly after on the game
		// thread), which would otherwise invoke a delegate on an object that
		// is already being torn down, causing a crash or use-after-free when
		// WebSocketClient has been set to nullptr.
		WebSocketClient->OnConnected.RemoveDynamic(this, &UBanDiscordSubsystem::OnWebSocketConnected);
		WebSocketClient->OnMessage.RemoveDynamic(this,   &UBanDiscordSubsystem::OnWebSocketMessage);
		WebSocketClient->OnClosed.RemoveDynamic(this,    &UBanDiscordSubsystem::OnWebSocketClosed);
		WebSocketClient->OnError.RemoveDynamic(this,     &UBanDiscordSubsystem::OnWebSocketError);

		// Prevent the SMLWebSocket auto-reconnect runnable from silently
		// re-opening the connection after we explicitly close it (e.g. when
		// an external provider takes over, or during subsystem deinitialisation).
		WebSocketClient->bAutoReconnect = false;
		WebSocketClient->Close();
		WebSocketClient = nullptr;
	}

	bGatewayReady        = false;
	bPendingHeartbeatAck = false;
	LastSequenceNumber   = -1;
	BotUserId.Empty();
	GuildOwnerId.Empty();
	SessionId.Empty();
	ResumeGatewayUrl.Empty();
}

// ─────────────────────────────────────────────────────────────────────────────
// Standalone Gateway — WebSocket event handlers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::OnWebSocketConnected()
{
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: WebSocket connected to Discord Gateway."));
}

void UBanDiscordSubsystem::OnWebSocketMessage(const FString& RawJson)
{
	HandleGatewayPayload(RawJson);
}

void UBanDiscordSubsystem::OnWebSocketClosed(int32 StatusCode, const FString& Reason)
{
	UE_LOG(LogBanDiscord, Warning,
	       TEXT("BanDiscordSubsystem: Gateway connection closed (%d: %s)."),
	       StatusCode, *Reason);

	// Detect Discord-specific terminal close codes that indicate permanent failure.
	// For these codes auto-reconnect must be stopped; retrying will always fail
	// with the same error and produce an endless reconnect-reject loop.
	bool bTerminal = false;
	switch (StatusCode)
	{
	case 4004:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Authentication failed (4004). "
		            "Verify BotToken in Mods/BanSystem/Config/DefaultBanSystem.ini. "
		            "Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4010:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Invalid shard (4010). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4011:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Sharding required (4011). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4012:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Invalid API version (4012). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4013:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Invalid intent(s) (4013). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4014:
		UE_LOG(LogBanDiscord, Error,
		       TEXT("BanDiscordSubsystem: Disallowed intent(s) (4014). "
		            "Enable Server Members Intent and Message Content Intent "
		            "in the Discord Developer Portal. Auto-reconnect disabled."));
		bTerminal = true;
		break;
	default:
		break;
	}

	if (bTerminal && WebSocketClient)
	{
		// Call Close() to set bUserInitiatedClose on the I/O thread, which
		// causes the SMLWebSocket reconnect loop to exit permanently after
		// this close event.  Without this, the runnable's own copy of
		// ReconnectCfg.bAutoReconnect is still true and it will keep
		// reconnecting; Discord rejects with the same terminal code again
		// and the bot loops endlessly (e.g. with an invalid token for 4004).
		// Calling Close() is safe here: EnqueueClose() sets bUserInitiatedClose
		// atomically (checked in the runnable's 100 ms back-off sleep); the
		// queued Close frame itself is never sent because the connected inner
		// loop has already exited after the server close frame.
		WebSocketClient->Close(1000,
			FString::Printf(TEXT("Terminal Discord close code %d"), StatusCode));
	}

	bGatewayReady        = false;
	bPendingHeartbeatAck = false;

	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
	}
}

void UBanDiscordSubsystem::OnWebSocketError(const FString& ErrorMessage)
{
	UE_LOG(LogBanDiscord, Warning,
	       TEXT("BanDiscordSubsystem: Gateway WebSocket error: %s"), *ErrorMessage);
}

// ─────────────────────────────────────────────────────────────────────────────
// Standalone Gateway — protocol handling
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleGatewayPayload(const FString& RawJson)
{
	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		return;
	}

	double OpcodeD = -1.0;
	if (!Root->TryGetNumberField(TEXT("op"), OpcodeD))
	{
		return;
	}
	const int32 Opcode = static_cast<int32>(OpcodeD);

	switch (Opcode)
	{
	case 10: // Hello
	{
		const TSharedPtr<FJsonObject>* DataObj = nullptr;
		if (Root->TryGetObjectField(TEXT("d"), DataObj) && DataObj)
		{
			HandleHello(*DataObj);
		}
		break;
	}
	case 0: // Dispatch
	{
		FString EventType;
		Root->TryGetStringField(TEXT("t"), EventType);
		double SeqD = -1.0;
		if (Root->TryGetNumberField(TEXT("s"), SeqD) && SeqD >= 0.0)
		{
			LastSequenceNumber = static_cast<int32>(SeqD);
		}
		const int32 Seq = LastSequenceNumber;
		const TSharedPtr<FJsonObject>* DataObj = nullptr;
		if (Root->TryGetObjectField(TEXT("d"), DataObj) && DataObj)
		{
			HandleDispatch(EventType, Seq, *DataObj);
		}
		break;
	}
	case 11: // HeartbeatAck
		HandleHeartbeatAck();
		break;
	case 7: // Reconnect
		// Per Discord spec: keep SessionId/ResumeGatewayUrl for resume attempt.
		// Do NOT call Disconnect()/Close() here — that sets bUserInitiatedClose
		// inside the SMLWebSocket runnable, permanently disabling auto-reconnect.
		if (HeartbeatTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
			HeartbeatTickerHandle.Reset();
		}
		bPendingHeartbeatAck = false;
		bGatewayReady        = false;
		BotUserId.Empty();
		GuildOwnerId.Empty();
		if (WebSocketClient)
		{
			const FString ConnectUrl = ResumeGatewayUrl.IsEmpty()
				? DiscordGatewayUrl
				: (ResumeGatewayUrl + TEXT("/?v=10&encoding=json"));
			WebSocketClient->Connect(ConnectUrl, {}, {});
		}
		break;
	case 9: // Invalid Session
	{
		bool bResumable = false;
		Root->TryGetBoolField(TEXT("d"), bResumable);
		HandleInvalidSession(bResumable);
		break;
	}
	default:
		break;
	}
}

void UBanDiscordSubsystem::HandleHello(const TSharedPtr<FJsonObject>& DataObj)
{
	double HeartbeatMs = 41250.0;
	DataObj->TryGetNumberField(TEXT("heartbeat_interval"), HeartbeatMs);
	HeartbeatIntervalSeconds = static_cast<float>(HeartbeatMs) / 1000.0f;

	// Start the heartbeat ticker.
	// Per Discord spec: jitter the FIRST heartbeat by a random [0, interval]
	// delay to prevent thundering-herd reconnect spikes.  After the first
	// beat, switch to the full repeating interval.
	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	}
	bPendingHeartbeatAck = false;

	const float JitterSeconds = FMath::FRandRange(0.0f, HeartbeatIntervalSeconds);
	HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			SendHeartbeat();
			// Re-register at the full interval for subsequent beats.
			HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UBanDiscordSubsystem::HeartbeatTick),
				HeartbeatIntervalSeconds);
			return false; // one-shot jitter tick
		}),
		JitterSeconds);

	// Attempt Resume if we have a session; otherwise Identify fresh.
	if (!SessionId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Gateway Hello received (resume path). "
		            "Sending Resume for session %s. Heartbeat interval %.1f s."),
		       *SessionId, HeartbeatIntervalSeconds);
		SendResume();
	}
	else
	{
		SendIdentify();
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Gateway Hello received — "
		            "heartbeat interval %.1f s, Identify sent."),
		       HeartbeatIntervalSeconds);
	}
}

void UBanDiscordSubsystem::HandleDispatch(const FString& EventType, int32 Sequence,
                                           const TSharedPtr<FJsonObject>& DataObj)
{
	if (EventType == TEXT("READY"))
	{
		HandleReady(DataObj);
	}
	else if (EventType == TEXT("RESUMED"))
	{
		HandleResumed();
	}
	else if (EventType == TEXT("MESSAGE_CREATE"))
	{
		HandleMessageCreate(DataObj);
	}
	else if (EventType == TEXT("GUILD_CREATE"))
	{
		HandleGuildCreate(DataObj);
	}
}

void UBanDiscordSubsystem::HandleHeartbeatAck()
{
	bPendingHeartbeatAck = false;
}

void UBanDiscordSubsystem::HandleReady(const TSharedPtr<FJsonObject>& DataObj)
{
	const TSharedPtr<FJsonObject>* UserObj = nullptr;
	if (DataObj->TryGetObjectField(TEXT("user"), UserObj) && UserObj)
	{
		(*UserObj)->TryGetStringField(TEXT("id"), BotUserId);
	}

	// Capture session_id and resume_gateway_url for future reconnects.
	DataObj->TryGetStringField(TEXT("session_id"), SessionId);
	DataObj->TryGetStringField(TEXT("resume_gateway_url"), ResumeGatewayUrl);

	bGatewayReady = true;
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Gateway READY — bot user ID: %s"), *BotUserId);
}

void UBanDiscordSubsystem::HandleResumed()
{
	bGatewayReady = true;
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Gateway session resumed successfully."));
}

void UBanDiscordSubsystem::HandleGuildCreate(const TSharedPtr<FJsonObject>& DataObj)
{
	DataObj->TryGetStringField(TEXT("owner_id"), GuildOwnerId);
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: GUILD_CREATE received — guild owner ID: %s"),
	       *GuildOwnerId);
}

void UBanDiscordSubsystem::HandleMessageCreate(const TSharedPtr<FJsonObject>& DataObj)
{
	// Broadcast to external API subscribers (e.g. other mods that called
	// SubscribeDiscordMessages() directly on this subsystem).
	if (OnRawDiscordMessage.IsBound())
	{
		OnRawDiscordMessage.Broadcast(DataObj);
	}

	// Always route to the internal command handler in standalone mode.
	// Without this call every ban command typed in Discord is silently
	// dropped because OnDiscordMessageReceived is only wired up via
	// SetCommandProvider() in the external-provider path.
	OnDiscordMessageReceived(DataObj);
}

void UBanDiscordSubsystem::SendIdentify()
{
#if PLATFORM_WINDOWS
	static const FString Os = TEXT("windows");
#elif PLATFORM_LINUX
	static const FString Os = TEXT("linux");
#else
	static const FString Os = TEXT("unknown");
#endif

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetStringField(TEXT("os"),      Os);
	Props->SetStringField(TEXT("browser"), TEXT("satisfactory_ban_system"));
	Props->SetStringField(TEXT("device"),  TEXT("satisfactory_ban_system"));

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("token"),   Config.BotToken);
	// Intents: GUILDS(1) | GUILD_MEMBERS(2) | GUILD_MESSAGES(512) | MESSAGE_CONTENT(32768) = 33283
	Data->SetNumberField(TEXT("intents"), 33283);
	Data->SetObjectField(TEXT("properties"), Props);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), 2); // Identify
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);
}

void UBanDiscordSubsystem::SendResume()
{
	if (SessionId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Warning,
		       TEXT("BanDiscordSubsystem: SendResume called with empty SessionId — "
		            "falling back to Identify."));
		SendIdentify();
		return;
	}

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("token"),      Config.BotToken);
	Data->SetStringField(TEXT("session_id"), SessionId);
	Data->SetNumberField(TEXT("seq"),        LastSequenceNumber);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), 6); // Resume
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Resume sent (session_id=%s, seq=%d)."),
	       *SessionId, LastSequenceNumber);
}

void UBanDiscordSubsystem::HandleInvalidSession(bool bResumable)
{
	bGatewayReady = false;
	BotUserId.Empty();
	GuildOwnerId.Empty(); // stale owner ID must not survive into the new session

	// Stop the heartbeat — it belongs to the invalidated session and must not
	// fire while we wait for the deferred re-identify/resume below.
	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
	}

	// Cancel any previously scheduled re-identify ticker before creating a new one.
	if (PendingReidentifyHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(PendingReidentifyHandle);
		PendingReidentifyHandle.Reset();
	}

	if (bResumable && !SessionId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Invalid session (resumable) — "
		            "scheduling Resume in 2 s."));
		TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
		PendingReidentifyHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
				{
					Self->PendingReidentifyHandle.Reset();
					Self->SendResume();
				}
				return false;
			}),
			2.0f);
	}
	else
	{
		// Session not resumable — clear session and re-identify.
		SessionId.Empty();
		ResumeGatewayUrl.Empty();
		LastSequenceNumber = -1;
		UE_LOG(LogBanDiscord, Warning,
		       TEXT("BanDiscordSubsystem: Invalid session (not resumable) — "
		            "re-identifying in 2 s."));
		TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
		PendingReidentifyHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
				{
					Self->PendingReidentifyHandle.Reset();
					Self->SendIdentify();
				}
				return false;
			}),
			2.0f);
	}
}

void UBanDiscordSubsystem::SendHeartbeat()
{
	if (bPendingHeartbeatAck)
	{
		// Zombie connection — force reconnect but keep session for resume.
		UE_LOG(LogBanDiscord, Warning,
		       TEXT("BanDiscordSubsystem: Heartbeat not acknowledged — "
		            "reconnecting to Discord Gateway."));
		if (HeartbeatTickerHandle.IsValid())
		{
			FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
			HeartbeatTickerHandle.Reset();
		}
		bPendingHeartbeatAck = false;
		bGatewayReady        = false;
		BotUserId.Empty();
		GuildOwnerId.Empty();
		if (WebSocketClient)
		{
			const FString ConnectUrl = ResumeGatewayUrl.IsEmpty()
				? DiscordGatewayUrl
				: (ResumeGatewayUrl + TEXT("/?v=10&encoding=json"));
			WebSocketClient->Connect(ConnectUrl, {}, {});
		}
		return;
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), 1); // Heartbeat
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

bool UBanDiscordSubsystem::HeartbeatTick(float DeltaTime)
{
	SendHeartbeat();
	return true; // Keep ticking.
}

void UBanDiscordSubsystem::SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload)
{
	if (!WebSocketClient || !WebSocketClient->IsConnected())
	{
		UE_LOG(LogBanDiscord, Warning,
		       TEXT("BanDiscordSubsystem: SendGatewayPayload called but WebSocket is not connected — payload dropped."));
		return;
	}

	FString JsonString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonString);
	FJsonSerializer::Serialize(Payload.ToSharedRef(), Writer);

	WebSocketClient->SendText(JsonString);
}

void UBanDiscordSubsystem::SendMessageToChannelInternal(const FString& ChannelId,
                                                         const FString& Message)
{
	if (Config.BotToken.IsEmpty() || ChannelId.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"), Message);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s/messages"), *DiscordApiBase, *ChannelId);

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
				UE_LOG(LogBanDiscord, Warning,
				       TEXT("BanDiscordSubsystem: HTTP request failed for message '%s'."),
				       *Message);
				return;
			}
			if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				UE_LOG(LogBanDiscord, Warning,
				       TEXT("BanDiscordSubsystem: Discord REST API returned %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord message handling
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::OnDiscordMessageReceived(const TSharedPtr<FJsonObject>& MessageObj)
{
	if (!MessageObj.IsValid())
	{
		return;
	}

	// Only process messages from the configured ban commands channel.
	if (Config.DiscordChannelId.IsEmpty())
	{
		return;
	}

	FString MsgChannelId;
	if (!MessageObj->TryGetStringField(TEXT("channel_id"), MsgChannelId))
	{
		return;
	}
	if (MsgChannelId != Config.DiscordChannelId)
	{
		return;
	}

	// Extract author info.
	const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("author"), AuthorPtr) || !AuthorPtr)
	{
		return;
	}

	FString AuthorId;
	(*AuthorPtr)->TryGetStringField(TEXT("id"), AuthorId);

	// Ignore messages from bots (including our own bot).
	bool bIsBot = false;
	(*AuthorPtr)->TryGetBoolField(TEXT("bot"), bIsBot);
	if (bIsBot)
	{
		return;
	}

	// Get the display name (server nickname > global name > username).
	FString DisplayName;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
	{
		(*MemberPtr)->TryGetStringField(TEXT("nick"), DisplayName);
	}
	if (DisplayName.IsEmpty())
	{
		if (!(*AuthorPtr)->TryGetStringField(TEXT("global_name"), DisplayName) ||
		    DisplayName.IsEmpty())
		{
			(*AuthorPtr)->TryGetStringField(TEXT("username"), DisplayName);
		}
	}
	if (DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Discord User");
	}

	// Extract message content.
	FString Content;
	MessageObj->TryGetStringField(TEXT("content"), Content);
	Content = Content.TrimStartAndEnd();
	if (Content.IsEmpty())
	{
		return;
	}

	// ── Check for permission-gated commands ──────────────────────────────────
	// Compute permission once; used by multiple branches below.
	bool bHasPermission = HasCommandPermission(MessageObj, AuthorId);

	// ── Command routing ───────────────────────────────────────────────────────
	// Check prefixes from longest to shortest to avoid ambiguous prefix matches
	// (e.g. "!steambanlist" must be checked before "!steamban").

	// Helper: check prefix and split rest of content.
	auto TryCommand = [&](const FString& Prefix, bool bRequiresAuth,
	                      TFunction<void(const FString&)> Handler) -> bool
	{
		if (Prefix.IsEmpty())
		{
			return false; // Command disabled via config.
		}
		if (!Content.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			return false;
		}
		// Ensure the prefix is followed by whitespace or end of string
		// so that "!steamban" doesn't match "!steambanlist".
		const int32 PrefixLen = Prefix.Len();
		if (PrefixLen < Content.Len() && !FChar::IsWhitespace(Content[PrefixLen]))
		{
			return false;
		}
		if (bRequiresAuth && !bHasPermission)
		{
			Reply(MsgChannelId,
			      TEXT(":no_entry: You do not have permission to use BanSystem commands."));
			return true; // Consumed (even though we rejected it).
		}
		const FString Args = Content.Mid(PrefixLen).TrimStartAndEnd();
		Handler(Args);
		return true;
	};

	// List commands (no destructive action, still require permission).
	if (TryCommand(Config.SteamBanListCommandPrefix, true, [&](const FString&)
	               { HandleSteamBanListCommand(MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSBanListCommandPrefix, true, [&](const FString&)
	               { HandleEOSBanListCommand(MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.PlayerIdsCommandPrefix, true, [&](const FString& Args)
	               { HandlePlayerIdsCommand(Args, MsgChannelId); }))
	{
		return;
	}

	// Unban commands.
	if (TryCommand(Config.SteamUnbanCommandPrefix, true, [&](const FString& Args)
	               { HandleSteamUnbanCommand(Args, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSUnbanCommandPrefix, true, [&](const FString& Args)
	               { HandleEOSUnbanCommand(Args, MsgChannelId); }))
	{
		return;
	}

	// Ban commands (checked after unban so "!steamunban" prefix wins over "!steamban").
	if (TryCommand(Config.SteamBanCommandPrefix, true, [&](const FString& Args)
	               { HandleSteamBanCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSBanCommandPrefix, true, [&](const FString& Args)
	               { HandleEOSBanCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.BanByNameCommandPrefix, true, [&](const FString& Args)
	               { HandleBanByNameCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.CheckBanCommandPrefix, true, [&](const FString& Args)
	               { HandleCheckBanCommand(Args, MsgChannelId); }))
	{
		return;
	}
}

bool UBanDiscordSubsystem::HasCommandPermission(const TSharedPtr<FJsonObject>& MessageObj,
                                                 const FString&                  AuthorId) const
{
	// Guild owner always has permission.
	const FString& OwnerId = GetGuildOwnerId();
	if (!OwnerId.IsEmpty() && AuthorId == OwnerId)
	{
		return true;
	}

	// When no role is configured, only the guild owner may run commands.
	if (Config.DiscordCommandRoleId.IsEmpty())
	{
		return false;
	}

	// Check whether the member holds the required role.
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) || !MemberPtr)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
	{
		for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
		{
			FString RoleId;
			if (RoleVal->TryGetString(RoleId) && RoleId == Config.DiscordCommandRoleId)
			{
				return true;
			}
		}
	}

	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Command handlers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleSteamBanCommand(const FString& Args,
                                                  const FString& IssuedBy,
                                                  const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id|PlayerName> [duration_minutes] [reason]`"),
		                      *Config.SteamBanCommandPrefix));
		return;
	}

	const FString Input = Parts[0];

	// Resolve Steam64 ID.
	FString Steam64Id;
	if (USteamBanSubsystem::IsValidSteam64Id(Input))
	{
		Steam64Id = Input;
	}
	else
	{
		// Attempt name lookup.
		UWorld* World = GetWorld();
		if (!World)
		{
			Reply(ChannelId, TEXT(":x: World not available — cannot resolve player name."));
			return;
		}
		FResolvedBanId Ids;
		FString PlayerName;
		TArray<FString> Ambiguous;
		if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
		{
			if (Ambiguous.Num() > 1)
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
				                      *Input, *FString::Join(Ambiguous, TEXT(", "))));
			}
			else
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":x: `%s` is not a valid Steam64 ID and no online player "
				                          "with that name was found."),
				                      *Input));
			}
			return;
		}
		if (!Ids.HasSteamId())
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":yellow_circle: Player `%s` has no Steam ID. "
			                          "They may be an Epic-only player — use `%s` instead."),
			                      *PlayerName, *Config.EOSBanCommandPrefix));
			return;
		}
		Reply(ChannelId,
		      FString::Printf(TEXT(":mag: Resolved `%s` → Steam64 ID: `%s`"),
		                      *PlayerName, *Ids.Steam64Id));
		Steam64Id = Ids.Steam64Id;
	}

	// Parse optional duration and reason from the remaining arguments.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (Parts.Num() > 1)
	{
		if (Parts[1].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[1]);
			if (Parts.Num() > 2)
			{
				TArray<FString> ReasonParts;
				for (int32 i = 2; i < Parts.Num(); ++i)
				{
					ReasonParts.Add(Parts[i]);
				}
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				ReasonParts.Add(Parts[i]);
			}
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	if (!Bans->BanPlayer(Steam64Id, Reason, Duration, IssuedBy))
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Failed to ban Steam ID `%s` — the ID may be invalid."),
		                      *Steam64Id));
		return;
	}

	FString Msg = Config.SteamBanResponseMessage;
	Msg.ReplaceInline(TEXT("%PlayerId%"),  *Steam64Id, ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Reason%"),    *Reason,    ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%BannedBy%"),  *IssuedBy,  ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Duration%"),  *FormatDuration(Duration), ESearchCase::CaseSensitive);
	Reply(ChannelId, Msg);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Steam ban issued by '%s' — ID: %s, Duration: %d min, Reason: %s"),
	       *IssuedBy, *Steam64Id, Duration, *Reason);

	// Cross-platform: asynchronously look up the linked EOS PUID via EOSSystem
	// and apply an EOS ban with the same reason/duration when the result arrives.
	// UBanEnforcementSubsystem checks the sync cache first and falls back to an
	// async EOS query when the PUID is not yet cached.
	if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
	{
		Enforcement->PropagateToEOSAsync(Steam64Id, Reason, Duration, IssuedBy);
	}
}

void UBanDiscordSubsystem::HandleSteamUnbanCommand(const FString& Args,
                                                    const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id>`"),
		                      *Config.SteamUnbanCommandPrefix));
		return;
	}

	const FString Steam64Id = Parts[0];
	if (!USteamBanSubsystem::IsValidSteam64Id(Steam64Id))
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: `%s` is not a valid Steam64 ID "
		                          "(must be 17 digits starting with 7656119)."),
		                      *Steam64Id));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	if (Bans->UnbanPlayer(Steam64Id))
	{
		FString Msg = Config.SteamUnbanResponseMessage;
		Msg.ReplaceInline(TEXT("%PlayerId%"), *Steam64Id, ESearchCase::CaseSensitive);
		Reply(ChannelId, Msg);

		// Cross-platform: remove any linked EOS ban.
		if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
		{
			Enforcement->PropagateUnbanToEOSAsync(Steam64Id);
		}
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: Steam64 ID `%s` was not on the ban list."),
		                      *Steam64Id));
	}
}

void UBanDiscordSubsystem::HandleSteamBanListCommand(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	const TArray<FBanEntry> AllBans = Bans->GetAllBans();
	if (AllBans.Num() == 0)
	{
		Reply(ChannelId, TEXT(":scroll: **Steam Ban List** — No active Steam bans."));
		return;
	}

	FString ListMsg = FString::Printf(
		TEXT(":scroll: **Steam Ban List** — %d active ban(s):\n"), AllBans.Num());

	for (const FBanEntry& Entry : AllBans)
	{
		const FString Line = FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);

		// Discord message length limit is 2000 chars; truncate at line boundaries.
		if (ListMsg.Len() + Line.Len() > 1900)
		{
			ListMsg += TEXT("… (list truncated — too many bans to display)");
			break;
		}
		ListMsg += Line;
	}

	Reply(ChannelId, ListMsg);
}

void UBanDiscordSubsystem::HandleEOSBanCommand(const FString& Args,
                                                const FString& IssuedBy,
                                                const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <EOSProductUserId|PlayerName> [duration_minutes] [reason]`"),
		                      *Config.EOSBanCommandPrefix));
		return;
	}

	const FString Input = Parts[0];

	// Resolve EOS Product User ID.
	FString EOSPUID;
	if (UEOSBanSubsystem::IsValidEOSProductUserId(Input))
	{
		// Input passes the 32-hex-char format check — accept it directly.
		EOSPUID = Input.ToLower();
	}
	else
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			Reply(ChannelId, TEXT(":x: World not available — cannot resolve player name."));
			return;
		}
		FResolvedBanId Ids;
		FString PlayerName;
		TArray<FString> Ambiguous;
		if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
		{
			if (Ambiguous.Num() > 1)
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
				                      *Input, *FString::Join(Ambiguous, TEXT(", "))));
			}
			else
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":x: `%s` is not a valid EOS Product User ID and no "
				                          "online player with that name was found."),
				                      *Input));
			}
			return;
		}
		if (!Ids.HasEOSPuid())
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":yellow_circle: Player `%s` has no EOS Product User ID. "
			                          "They may be a Steam-only player — use `%s` instead."),
			                      *PlayerName, *Config.SteamBanCommandPrefix));
			return;
		}
		Reply(ChannelId,
		      FString::Printf(TEXT(":mag: Resolved `%s` → EOS PUID: `%s`"),
		                      *PlayerName, *Ids.EOSProductUserId));
		EOSPUID = Ids.EOSProductUserId;
	}

	// Parse optional duration and reason.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (Parts.Num() > 1)
	{
		if (Parts[1].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[1]);
			if (Parts.Num() > 2)
			{
				TArray<FString> ReasonParts;
				for (int32 i = 2; i < Parts.Num(); ++i)
				{
					ReasonParts.Add(Parts[i]);
				}
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				ReasonParts.Add(Parts[i]);
			}
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	if (!Bans->BanPlayer(EOSPUID, Reason, Duration, IssuedBy))
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Failed to ban EOS PUID `%s` — the ID may be invalid."),
		                      *EOSPUID));
		return;
	}

	FString Msg = Config.EOSBanResponseMessage;
	Msg.ReplaceInline(TEXT("%PlayerId%"), *EOSPUID,            ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Reason%"),   *Reason,             ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%BannedBy%"), *IssuedBy,           ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Duration%"), *FormatDuration(Duration), ESearchCase::CaseSensitive);
	Reply(ChannelId, Msg);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: EOS ban issued by '%s' — ID: %s, Duration: %d min, Reason: %s"),
	       *IssuedBy, *EOSPUID, Duration, *Reason);

	// Cross-platform: asynchronously look up the linked Steam64 ID via EOSSystem
	// and apply a Steam ban with the same reason/duration when the result arrives.
	// UBanEnforcementSubsystem checks the sync cache first and falls back to an
	// async EOS reverse lookup when the Steam64 ID is not yet cached.
	if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
	{
		Enforcement->PropagateToSteamAsync(EOSPUID, Reason, Duration, IssuedBy);
	}
}

void UBanDiscordSubsystem::HandleEOSUnbanCommand(const FString& Args,
                                                  const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <EOSProductUserId>`"),
		                      *Config.EOSUnbanCommandPrefix));
		return;
	}

	const FString EOSPUID = Parts[0].ToLower();
	if (!UEOSBanSubsystem::IsValidEOSProductUserId(EOSPUID))
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: `%s` is not a valid EOS Product User ID "
		                          "(must be 32 lowercase hex chars)."),
		                      *Parts[0]));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	if (Bans->UnbanPlayer(EOSPUID))
	{
		FString Msg = Config.EOSUnbanResponseMessage;
		Msg.ReplaceInline(TEXT("%PlayerId%"), *EOSPUID, ESearchCase::CaseSensitive);
		Reply(ChannelId, Msg);

		// Cross-platform: remove any linked Steam ban.
		if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
		{
			Enforcement->PropagateUnbanToSteamAsync(EOSPUID);
		}
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: EOS Product User ID `%s` was not on the ban list."),
		                      *EOSPUID));
	}
}

void UBanDiscordSubsystem::HandleEOSBanListCommand(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	const TArray<FBanEntry> AllBans = Bans->GetAllBans();
	if (AllBans.Num() == 0)
	{
		Reply(ChannelId, TEXT(":scroll: **EOS Ban List** — No active EOS bans."));
		return;
	}

	FString ListMsg = FString::Printf(
		TEXT(":scroll: **EOS Ban List** — %d active ban(s):\n"), AllBans.Num());

	for (const FBanEntry& Entry : AllBans)
	{
		const FString Line = FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);

		// Discord message length limit is 2000 chars; truncate at line boundaries.
		if (ListMsg.Len() + Line.Len() > 1900)
		{
			ListMsg += TEXT("… (list truncated — too many bans to display)");
			break;
		}
		ListMsg += Line;
	}

	Reply(ChannelId, ListMsg);
}

void UBanDiscordSubsystem::HandleBanByNameCommand(const FString& Args,
                                                   const FString& IssuedBy,
                                                   const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <PlayerName> [duration_minutes] [reason]`\n"
		                           "Tip: wrap multi-word names in quotes — `%s \"John Doe\" 60 reason`"),
		                      *Config.BanByNameCommandPrefix, *Config.BanByNameCommandPrefix));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: World not available — cannot look up player."));
		return;
	}

	// ── Resolve the player name from Parts ───────────────────────────────────
	//
	// Two approaches are tried in order:
	//
	// 1. Quoted names (recommended): The admin wraps the name in double quotes,
	//    e.g. !banbyname "John Doe" 60 reason.  SplitArgs already stripped the
	//    quotes, so Parts[0] is the full name and Parts[1..] hold duration/reason.
	//
	// 2. First-numeric heuristic (unquoted fallback): The first all-digit token
	//    is treated as the start of the optional duration.  All Parts before it
	//    are joined as the player name.  If no digit token exists, every Part is
	//    the name (no duration or reason).

	int32 DurationPartIdx = -1;
	for (int32 i = 1; i < Parts.Num(); ++i) // start at 1 — Part[0] is always part of the name
	{
		if (Parts[i].IsNumeric())
		{
			DurationPartIdx = i;
			break;
		}
	}

	FString NameQuery;
	int32   PartsStartForDuration;
	if (DurationPartIdx < 0)
	{
		// No numeric part — all parts form the player name.
		NameQuery             = FString::Join(Parts, TEXT(" "));
		PartsStartForDuration = Parts.Num();
	}
	else
	{
		// Join everything before the first numeric part as the name.
		TArray<FString> NameParts;
		for (int32 i = 0; i < DurationPartIdx; ++i) NameParts.Add(Parts[i]);
		NameQuery             = FString::Join(NameParts, TEXT(" "));
		PartsStartForDuration = DurationPartIdx;
	}

	FResolvedBanId Ids;
	FString PlayerName;
	TArray<FString> Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, NameQuery, Ids, PlayerName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
			                      *NameQuery, *FString::Join(Ambiguous, TEXT(", "))));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":x: No online player found matching `%s`."), *NameQuery));
		}
		return;
	}

	if (!Ids.IsValid())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Player `%s` has no resolvable platform IDs."), *PlayerName));
		return;
	}

	// Parse optional duration and reason from Parts starting at PartsStartForDuration.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (PartsStartForDuration < Parts.Num())
	{
		if (Parts[PartsStartForDuration].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[PartsStartForDuration]);
			if (PartsStartForDuration + 1 < Parts.Num())
			{
				TArray<FString> ReasonParts;
				for (int32 i = PartsStartForDuration + 1; i < Parts.Num(); ++i)
					ReasonParts.Add(Parts[i]);
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = PartsStartForDuration; i < Parts.Num(); ++i)
				ReasonParts.Add(Parts[i]);
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	TArray<FString> BannedOn;

	UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr;

	if (Ids.HasSteamId())
	{
		USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
		if (SteamBans && SteamBans->BanPlayer(Ids.Steam64Id, Reason, Duration, IssuedBy))
		{
			BannedOn.Add(FString::Printf(TEXT("Steam (`%s`)"), *Ids.Steam64Id));

			// Cross-platform: if the EOS PUID was not immediately available,
			// propagate the ban asynchronously via EOSSystem so linked Epic
			// accounts are also banned.
			if (!Ids.HasEOSPuid() && Enforcement)
			{
				Enforcement->PropagateToEOSAsync(Ids.Steam64Id, Reason, Duration, IssuedBy);
			}
		}
	}

	if (Ids.HasEOSPuid())
	{
		UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
		if (EOSBans && EOSBans->BanPlayer(Ids.EOSProductUserId, Reason, Duration, IssuedBy))
		{
			BannedOn.Add(FString::Printf(TEXT("EOS (`%s`)"), *Ids.EOSProductUserId));

			// Cross-platform: if the Steam64 ID was not immediately available,
			// propagate the ban asynchronously via EOSSystem so linked Steam
			// accounts are also banned.
			if (!Ids.HasSteamId() && Enforcement)
			{
				Enforcement->PropagateToSteamAsync(Ids.EOSProductUserId, Reason, Duration, IssuedBy);
			}
		}
	}

	if (BannedOn.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Player `%s` could not be banned — "
		                          "no ban subsystem is available."),
		                      *PlayerName));
		return;
	}

	Reply(ChannelId,
	      FString::Printf(TEXT(":hammer: **BanSystem** — `%s` banned by **%s** on %s. "
	                           "Duration: %s. Reason: %s"),
	                      *PlayerName,
	                      *IssuedBy,
	                      *FString::Join(BannedOn, TEXT(" and ")),
	                      *FormatDuration(Duration),
	                      *Reason));

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: banbyname '%s' issued by '%s' — platforms: %s, "
	            "Duration: %d min, Reason: %s"),
	       *PlayerName, *IssuedBy, *FString::Join(BannedOn, TEXT(", ")), Duration, *Reason);
}

void UBanDiscordSubsystem::HandlePlayerIdsCommand(const FString& Args,
                                                   const FString& ChannelId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: World not available."));
		return;
	}

	const FString NameFilter = Args.TrimStartAndEnd();
	const TArray<TPair<FString, FResolvedBanId>> AllPlayers =
		FBanPlayerLookup::GetAllConnectedPlayers(World);

	if (AllPlayers.Num() == 0)
	{
		Reply(ChannelId, TEXT(":busts_in_silhouette: No players are currently connected."));
		return;
	}

	FString ListMsg;
	int32 MatchCount = 0;
	bool  bTruncated = false;

	for (const TPair<FString, FResolvedBanId>& KV : AllPlayers)
	{
		const FString& Name = KV.Key;
		const FResolvedBanId& Ids = KV.Value;

		// When a name filter is provided, skip non-matching players.
		if (!NameFilter.IsEmpty() &&
		    !Name.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString IdLine = FString::Printf(TEXT("• **%s**"), *Name);
		if (Ids.HasSteamId())
		{
			IdLine += FString::Printf(TEXT(" — Steam: `%s`"), *Ids.Steam64Id);
		}
		if (Ids.HasEOSPuid())
		{
			IdLine += FString::Printf(TEXT(" — EOS: `%s`"), *Ids.EOSProductUserId);
		}
		if (!Ids.IsValid())
		{
			IdLine += TEXT(" — *no platform IDs resolved*");
		}
		IdLine += TEXT("\n");

		// Discord message limit: truncate at line boundaries, not mid-line.
		if (ListMsg.Len() + IdLine.Len() > 1800)
		{
			bTruncated = true;
			break;
		}
		ListMsg += IdLine;
		++MatchCount;
	}

	if (ListMsg.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":busts_in_silhouette: No connected players match `%s`."),
		                      *NameFilter));
		return;
	}

	const FString Header = NameFilter.IsEmpty()
		? FString::Printf(TEXT(":id: **Connected Players (%d):**\n"), MatchCount)
		: FString::Printf(TEXT(":id: **Players matching `%s` (%d):**\n"), *NameFilter, MatchCount);

	FString FullMsg = Header + ListMsg;
	if (bTruncated)
	{
		FullMsg += TEXT("… (list truncated)");
	}

	Reply(ChannelId, FullMsg);
}

void UBanDiscordSubsystem::HandleCheckBanCommand(const FString& Args,
                                                  const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id|EOSProductUserId|PlayerName>`"),
		                      *Config.CheckBanCommandPrefix));
		return;
	}

	// For raw IDs (Steam64 / EOS PUID) the first token is the whole ID.
	// For player name lookups we join all tokens so that multi-word names
	// such as "John Doe" work without requiring quotes, matching the
	// behaviour of the in-game /checkban command.
	const FString Input = (Parts.Num() == 1)
		? Parts[0]
		: FString::Join(Parts, TEXT(" "));
	UGameInstance* GI = GetGameInstance();

	// ── Try Steam64 ID ────────────────────────────────────────────────────────
	if (USteamBanSubsystem::IsValidSteam64Id(Input))
	{
		USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
		FBanEntry Entry;
		if (SteamBans && SteamBans->CheckPlayerBan(Input, Entry) == EBanCheckResult::Banned)
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":hammer: Steam ID `%s` is **banned**.\n"
			                          "• Reason: %s\n• Expires: %s\n• Banned by: %s"),
			                      *Input, *Entry.Reason,
			                      *Entry.GetExpiryString(), *Entry.BannedBy));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":white_check_mark: Steam ID `%s` is **not banned**."), *Input));
		}
		return;
	}

	// ── Try EOS PUID ──────────────────────────────────────────────────────────
	if (UEOSBanSubsystem::IsValidEOSProductUserId(Input))
	{
		const FString EOSPUID = Input.ToLower();
		UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
		FBanEntry Entry;
		if (EOSBans && EOSBans->CheckPlayerBan(EOSPUID, Entry) == EBanCheckResult::Banned)
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":hammer: EOS PUID `%s` is **banned**.\n"
			                          "• Reason: %s\n• Expires: %s\n• Banned by: %s"),
			                      *EOSPUID, *Entry.Reason,
			                      *Entry.GetExpiryString(), *Entry.BannedBy));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":white_check_mark: EOS PUID `%s` is **not banned**."), *EOSPUID));
		}
		return;
	}

	// ── Try player name (online players only) ─────────────────────────────────
	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: `%s` is not a valid Steam64 ID or EOS PUID, "
		                          "and the world is unavailable to resolve a player name."),
		                      *Input));
		return;
	}

	FResolvedBanId   Ids;
	FString          PlayerName;
	TArray<FString>  Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
			                      *Input, *FString::Join(Ambiguous, TEXT(", "))));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":x: `%s` is not a valid Steam64 ID or EOS PUID, "
			                          "and no online player with that name was found.\n"
			                          "Tip: use a raw ID to check offline players."),
			                      *Input));
		}
		return;
	}

	// Player found — check every platform they have an ID for.
	FString Response = FString::Printf(TEXT(":id: Ban status for **%s**:\n"), *PlayerName);

	USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	UEOSBanSubsystem*   EOSBans   = GI ? GI->GetSubsystem<UEOSBanSubsystem>()   : nullptr;

	if (Ids.HasSteamId() && SteamBans)
	{
		FBanEntry Entry;
		if (SteamBans->CheckPlayerBan(Ids.Steam64Id, Entry) == EBanCheckResult::Banned)
		{
			Response += FString::Printf(
				TEXT("• Steam `%s`: :hammer: **BANNED** — %s | Expires: %s | By: %s\n"),
				*Ids.Steam64Id, *Entry.Reason, *Entry.GetExpiryString(), *Entry.BannedBy);
		}
		else
		{
			Response += FString::Printf(
				TEXT("• Steam `%s`: :white_check_mark: not banned\n"), *Ids.Steam64Id);
		}
	}

	if (Ids.HasEOSPuid() && EOSBans)
	{
		FBanEntry Entry;
		if (EOSBans->CheckPlayerBan(Ids.EOSProductUserId, Entry) == EBanCheckResult::Banned)
		{
			Response += FString::Printf(
				TEXT("• EOS `%s`: :hammer: **BANNED** — %s | Expires: %s | By: %s\n"),
				*Ids.EOSProductUserId, *Entry.Reason, *Entry.GetExpiryString(), *Entry.BannedBy);
		}
		else
		{
			Response += FString::Printf(
				TEXT("• EOS `%s`: :white_check_mark: not banned\n"), *Ids.EOSProductUserId);
		}
	}

	if (!Ids.IsValid())
	{
		Response += TEXT("• *No platform IDs resolved — cannot check ban status.*\n");
	}

	Reply(ChannelId, Response.TrimEnd());
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Reply(const FString& ChannelId, const FString& Message)
{
	if (!ChannelId.IsEmpty())
	{
		// Discord message content limit is 2000 characters.  Truncate here so
		// that the REST call never receives a payload that would be rejected with
		// a 400 error; the admin can always retrieve full details from the server
		// logs or the ban-list command.
		const int32 MaxLen = 2000;
		if (Message.Len() > MaxLen)
		{
			static const FString Ellipsis = TEXT(" … (truncated)");
			const FString Truncated = Message.Left(MaxLen - Ellipsis.Len()) + Ellipsis;
			SendDiscordChannelMessage(ChannelId, Truncated);
		}
		else
		{
			// SendDiscordChannelMessage is a virtual method on this subsystem — it
			// routes to the external provider when one is active, otherwise uses the
			// built-in standalone HTTP implementation.
			SendDiscordChannelMessage(ChannelId, Message);
		}
	}
}

FString UBanDiscordSubsystem::FormatDuration(int32 DurationMinutes)
{
	return (DurationMinutes <= 0)
		? TEXT("permanently")
		: FString::Printf(TEXT("%d minute(s)"), DurationMinutes);
}

TArray<FString> UBanDiscordSubsystem::SplitArgs(const FString& Input)
{
	TArray<FString> Parts;
	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty()) return Parts;

	int32 i = 0;
	const int32 Len = Trimmed.Len();
	while (i < Len)
	{
		// Skip leading whitespace.
		while (i < Len && FChar::IsWhitespace(Trimmed[i])) ++i;
		if (i >= Len) break;

		if (Trimmed[i] == TEXT('"'))
		{
			// Quoted token — collect characters until the matching closing quote
			// or the end of the string.  This lets multi-word player names be
			// passed as a single argument, e.g.:
			//   !banbyname "John Doe" 60 reason
			// The escape sequence \" produces a literal double-quote inside the token.
			++i; // skip opening quote
			FString Token;
			while (i < Len && Trimmed[i] != TEXT('"'))
			{
				if (Trimmed[i] == TEXT('\\') && i + 1 < Len && Trimmed[i + 1] == TEXT('"'))
				{
					Token += TEXT('"');
					i += 2;
				}
				else
				{
					Token += Trimmed[i++];
				}
			}
			if (i < Len) ++i; // skip closing quote
			Parts.Add(Token);
		}
		else
		{
			// Unquoted token — collect until the next whitespace character.
			FString Token;
			while (i < Len && !FChar::IsWhitespace(Trimmed[i]))
			{
				Token += Trimmed[i++];
			}
			Parts.Add(Token);
		}
	}
	return Parts;
}
