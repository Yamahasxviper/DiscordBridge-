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

		// If we have a standalone bot token, reconnect the built-in Gateway.
		if (!Config.BotToken.IsEmpty() && !bGatewayReady)
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

	if (WebSocketClient)
	{
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

	int32 Opcode = 0;
	if (!Root->TryGetNumberField(TEXT("op"), Opcode))
	{
		return;
	}

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
		int32 Seq = 0;
		Root->TryGetNumberField(TEXT("s"), Seq);
		if (Seq > 0)
		{
			LastSequenceNumber = Seq;
		}
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
	if (HeartbeatTickerHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	}
	HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateUObject(this, &UBanDiscordSubsystem::HeartbeatTick),
		HeartbeatIntervalSeconds);
	bPendingHeartbeatAck = false;

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
	// Broadcast to internal subscribers (the command handler below).
	if (OnRawDiscordMessage.IsBound())
	{
		OnRawDiscordMessage.Broadcast(DataObj);
	}
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

	if (bResumable && !SessionId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Invalid session (resumable) — "
		            "scheduling Resume in 2 s."));
		TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
				{
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
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateLambda([WeakThis](float) -> bool
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
				{
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

	Bans->BanPlayer(Steam64Id, Reason, Duration, IssuedBy);

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
	const FString Steam64Id = Args.TrimStartAndEnd();
	if (Steam64Id.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id>`"),
		                      *Config.SteamUnbanCommandPrefix));
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
		ListMsg += FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);
	}

	// Discord message length limit is 2000 chars; truncate with a notice if needed.
	if (ListMsg.Len() > 1900)
	{
		ListMsg = ListMsg.Left(1900) + TEXT("\n… (list truncated — too many bans to display)");
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

	Bans->BanPlayer(EOSPUID, Reason, Duration, IssuedBy);

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
	const FString EOSPUID = Args.TrimStartAndEnd();
	if (EOSPUID.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <EOSProductUserId>`"),
		                      *Config.EOSUnbanCommandPrefix));
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
			Enforcement->PropagateUnbanToSteamAsync(EOSPUID.ToLower());
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
		ListMsg += FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);
	}

	if (ListMsg.Len() > 1900)
	{
		ListMsg = ListMsg.Left(1900) + TEXT("\n… (list truncated — too many bans to display)");
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
		      FString::Printf(TEXT(":warning: Usage: `%s <PlayerName> [duration_minutes] [reason]`"),
		                      *Config.BanByNameCommandPrefix));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: World not available — cannot look up player."));
		return;
	}

	const FString NameQuery = Parts[0];
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
		ListMsg += IdLine + TEXT("\n");
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
	if (FullMsg.Len() > 1900)
	{
		FullMsg = FullMsg.Left(1900) + TEXT("\n… (list truncated)");
	}

	Reply(ChannelId, FullMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Reply(const FString& ChannelId, const FString& Message)
{
	if (!ChannelId.IsEmpty())
	{
		// SendDiscordChannelMessage is a virtual method on this subsystem — it
		// routes to the external provider when one is active, otherwise uses the
		// built-in standalone HTTP implementation.
		SendDiscordChannelMessage(ChannelId, Message);
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
	Input.TrimStartAndEnd().ParseIntoArrayWS(Parts);
	return Parts;
}
