// Copyright Coffee Stain Studios. All Rights Reserved.

#include "TicketDiscordProvider.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY_STATIC(LogTicketDiscordProvider, Log, All);

// Discord Gateway endpoint (v10, JSON encoding)
static const FString TSDiscordGatewayUrl = TEXT("wss://gateway.discord.gg/?v=10&encoding=json");
// Discord REST API base URL
static const FString TSDiscordApiBase    = TEXT("https://discord.com/api/v10");

// ── Discord Gateway opcodes ───────────────────────────────────────────────────
namespace ETSGatewayOpcode
{
	static constexpr int32 Dispatch       = 0;
	static constexpr int32 Heartbeat      = 1;
	static constexpr int32 Identify       = 2;
	static constexpr int32 Reconnect      = 7;
	static constexpr int32 InvalidSession = 9;
	static constexpr int32 Hello         = 10;
	static constexpr int32 HeartbeatAck  = 11;
}

// ── Discord Gateway intents ───────────────────────────────────────────────────
// Guilds(1) + GuildMembers(2) + GuildMessages(512) + MessageContent(32768) = 33283
static constexpr int32 TSGatewayIntents = 33283;

// ─────────────────────────────────────────────────────────────────────────────
// Connection management
// ─────────────────────────────────────────────────────────────────────────────

void UTicketDiscordProvider::Connect(const FString& InBotToken,
                                     const FString& InGuildIdOverride)
{
	BotToken       = InBotToken;
	GuildIdOverride = InGuildIdOverride;

	// Apply the override immediately so callers get a non-empty GuildId even
	// before the READY event is received (some REST calls need it early).
	if (!GuildIdOverride.IsEmpty())
	{
		GuildId = GuildIdOverride;
	}

	if (WebSocketClient && WebSocketClient->IsConnected())
	{
		return; // Already connected.
	}

	WebSocketClient = USMLWebSocketClient::CreateWebSocketClient(this);

	// Configure auto-reconnect; Discord may close the connection at any time.
	WebSocketClient->bAutoReconnect               = true;
	WebSocketClient->ReconnectInitialDelaySeconds = 2.0f;
	WebSocketClient->MaxReconnectDelaySeconds     = 30.0f;
	WebSocketClient->MaxReconnectAttempts         = 0; // infinite

	WebSocketClient->OnConnected.AddDynamic(this,   &UTicketDiscordProvider::OnWebSocketConnected);
	WebSocketClient->OnMessage.AddDynamic(this,     &UTicketDiscordProvider::OnWebSocketMessage);
	WebSocketClient->OnClosed.AddDynamic(this,      &UTicketDiscordProvider::OnWebSocketClosed);
	WebSocketClient->OnError.AddDynamic(this,       &UTicketDiscordProvider::OnWebSocketError);
	WebSocketClient->OnReconnecting.AddDynamic(this, &UTicketDiscordProvider::OnWebSocketReconnecting);

	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Connecting to Discord Gateway…"));
	WebSocketClient->Connect(TSDiscordGatewayUrl, {}, {});
}

void UTicketDiscordProvider::Disconnect()
{
	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();

	bGatewayReady        = false;
	bPendingHeartbeatAck = false;
	LastSequenceNumber   = -1;
	BotUserId.Empty();
	GuildId.Empty();
	GuildOwnerId.Empty();

	if (WebSocketClient)
	{
		WebSocketClient->Close(1000, TEXT("TicketDiscordProvider shutting down"));
		WebSocketClient = nullptr;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// WebSocket event handlers
// ─────────────────────────────────────────────────────────────────────────────

void UTicketDiscordProvider::OnWebSocketConnected()
{
	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: WebSocket connected. Awaiting Hello…"));
}

void UTicketDiscordProvider::OnWebSocketMessage(const FString& RawJson)
{
	HandleGatewayPayload(RawJson);
}

void UTicketDiscordProvider::OnWebSocketClosed(int32 StatusCode, const FString& Reason)
{
	UE_LOG(LogTicketDiscordProvider, Warning,
	       TEXT("TicketDiscordProvider: Gateway closed (code=%d, reason='%s')."),
	       StatusCode, *Reason);

	// Detect Discord-specific terminal close codes that indicate permanent failure.
	bool bTerminal = false;
	switch (StatusCode)
	{
	case 4004:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Authentication failed (4004). "
		            "Verify BotToken in Mods/TicketSystem/Config/DefaultTickets.ini. "
		            "Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4010:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Invalid shard (4010). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4011:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Sharding required (4011). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4012:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Invalid API version (4012). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4013:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Invalid intent(s) (4013). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4014:
		UE_LOG(LogTicketDiscordProvider, Error,
		       TEXT("TicketDiscordProvider: Disallowed intent(s) (4014). "
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

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady        = false;
}

void UTicketDiscordProvider::OnWebSocketError(const FString& ErrorMessage)
{
	UE_LOG(LogTicketDiscordProvider, Error,
	       TEXT("TicketDiscordProvider: WebSocket error: %s"), *ErrorMessage);

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady        = false;
}

void UTicketDiscordProvider::OnWebSocketReconnecting(int32 AttemptNumber, float DelaySeconds)
{
	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Reconnecting (attempt %d, delay %.1fs)…"),
	       AttemptNumber, DelaySeconds);

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady        = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord Gateway protocol
// ─────────────────────────────────────────────────────────────────────────────

void UTicketDiscordProvider::HandleGatewayPayload(const FString& RawJson)
{
	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTicketDiscordProvider, Warning,
		       TEXT("TicketDiscordProvider: Failed to parse Gateway JSON: %s"), *RawJson);
		return;
	}

	const int32 OpCode = Root->GetIntegerField(TEXT("op"));

	switch (OpCode)
	{
	case ETSGatewayOpcode::Dispatch:
	{
		double Seq = -1.0;
		if (Root->TryGetNumberField(TEXT("s"), Seq))
		{
			LastSequenceNumber = static_cast<int32>(Seq);
		}

		FString EventType;
		Root->TryGetStringField(TEXT("t"), EventType);

		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		Root->TryGetObjectField(TEXT("d"), DataPtr);

		HandleDispatch(EventType, DataPtr ? *DataPtr : MakeShared<FJsonObject>());
		break;
	}
	case ETSGatewayOpcode::Hello:
	{
		const TSharedPtr<FJsonObject>* DataPtr = nullptr;
		if (Root->TryGetObjectField(TEXT("d"), DataPtr) && DataPtr)
		{
			HandleHello(*DataPtr);
		}
		break;
	}
	case ETSGatewayOpcode::HeartbeatAck:
		HandleHeartbeatAck();
		break;

	case ETSGatewayOpcode::Heartbeat:
		SendHeartbeat();
		break;

	case ETSGatewayOpcode::Reconnect:
		HandleReconnect();
		break;

	case ETSGatewayOpcode::InvalidSession:
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

void UTicketDiscordProvider::HandleHello(const TSharedPtr<FJsonObject>& DataObj)
{
	double HeartbeatMs = 41250.0;
	DataObj->TryGetNumberField(TEXT("heartbeat_interval"), HeartbeatMs);
	HeartbeatIntervalSeconds = static_cast<float>(HeartbeatMs) / 1000.0f;

	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Hello received. Heartbeat interval: %.2f s"),
	       HeartbeatIntervalSeconds);

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	bPendingHeartbeatAck = false;

	// One-shot ticker with random jitter before the first heartbeat, then start
	// the regular interval ticker – mirrors the DiscordBridge approach.
	const float JitterSeconds = FMath::FRandRange(0.0f, HeartbeatIntervalSeconds);
	HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			SendHeartbeat();
			HeartbeatTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
				FTickerDelegate::CreateUObject(this, &UTicketDiscordProvider::HeartbeatTick),
				HeartbeatIntervalSeconds);
			return false; // one-shot
		}),
		JitterSeconds);

	SendIdentify();
}

void UTicketDiscordProvider::HandleDispatch(const FString& EventType,
                                            const TSharedPtr<FJsonObject>& DataObj)
{
	if (EventType == TEXT("READY"))
	{
		HandleReady(DataObj);
	}
	else if (EventType == TEXT("GUILD_CREATE"))
	{
		DataObj->TryGetStringField(TEXT("owner_id"), GuildOwnerId);
	}
	else if (EventType == TEXT("INTERACTION_CREATE"))
	{
		if (OnInteraction.IsBound())
		{
			OnInteraction.Broadcast(DataObj);
		}
	}
	else if (EventType == TEXT("MESSAGE_CREATE"))
	{
		if (OnRawMessage.IsBound())
		{
			OnRawMessage.Broadcast(DataObj);
		}
	}
}

void UTicketDiscordProvider::HandleReady(const TSharedPtr<FJsonObject>& DataObj)
{
	const TSharedPtr<FJsonObject>* UserPtr = nullptr;
	if (DataObj->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
	{
		(*UserPtr)->TryGetStringField(TEXT("id"), BotUserId);
	}

	// Populate GuildId from the READY guilds array only when no override is set.
	if (GuildIdOverride.IsEmpty())
	{
		const TArray<TSharedPtr<FJsonValue>>* GuildsArray = nullptr;
		if (DataObj->TryGetArrayField(TEXT("guilds"), GuildsArray) &&
		    GuildsArray && GuildsArray->Num() > 0)
		{
			const TSharedPtr<FJsonObject>* FirstGuild = nullptr;
			if ((*GuildsArray)[0]->TryGetObject(FirstGuild) && FirstGuild)
			{
				(*FirstGuild)->TryGetStringField(TEXT("id"), GuildId);
			}
		}
	}

	bGatewayReady = true;

	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Gateway ready. BotUserId=%s, GuildId=%s"),
	       *BotUserId, *GuildId);
}

void UTicketDiscordProvider::HandleHeartbeatAck()
{
	UE_LOG(LogTicketDiscordProvider, VeryVerbose,
	       TEXT("TicketDiscordProvider: Heartbeat acknowledged."));
	bPendingHeartbeatAck = false;
}

void UTicketDiscordProvider::HandleReconnect()
{
	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Server requested reconnect."));

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;
	bGatewayReady        = false;
	LastSequenceNumber   = -1;
	BotUserId.Empty();
	if (GuildIdOverride.IsEmpty())
	{
		GuildId.Empty();
	}
	GuildOwnerId.Empty();

	if (WebSocketClient)
	{
		WebSocketClient->Connect(TSDiscordGatewayUrl, {}, {});
	}
}

void UTicketDiscordProvider::HandleInvalidSession(bool bResumable)
{
	UE_LOG(LogTicketDiscordProvider, Warning,
	       TEXT("TicketDiscordProvider: Invalid session (resumable=%s). Re-identifying in 2s…"),
	       bResumable ? TEXT("true") : TEXT("false"));

	FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			SendIdentify();
			return false;
		}),
		2.0f);
}

void UTicketDiscordProvider::SendIdentify()
{
#if PLATFORM_WINDOWS
	static const FString DiscordOs = TEXT("windows");
#elif PLATFORM_LINUX
	static const FString DiscordOs = TEXT("linux");
#else
	static const FString DiscordOs = TEXT("unknown");
#endif

	TSharedPtr<FJsonObject> Props = MakeShared<FJsonObject>();
	Props->SetStringField(TEXT("os"),      DiscordOs);
	Props->SetStringField(TEXT("browser"), TEXT("satisfactory_ticket_system"));
	Props->SetStringField(TEXT("device"),  TEXT("satisfactory_ticket_system"));

	TSharedPtr<FJsonObject> InitialPresence = MakeShared<FJsonObject>();
	InitialPresence->SetField(TEXT("since"), MakeShared<FJsonValueNull>());
	InitialPresence->SetArrayField(TEXT("activities"), TArray<TSharedPtr<FJsonValue>>());
	InitialPresence->SetStringField(TEXT("status"), TEXT("online"));
	InitialPresence->SetBoolField(TEXT("afk"), false);

	TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
	Data->SetStringField(TEXT("token"),   BotToken);
	Data->SetNumberField(TEXT("intents"), TSGatewayIntents);
	Data->SetObjectField(TEXT("properties"), Props);
	Data->SetObjectField(TEXT("presence"), InitialPresence);

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), ETSGatewayOpcode::Identify);
	Payload->SetObjectField(TEXT("d"),  Data);

	SendGatewayPayload(Payload);

	UE_LOG(LogTicketDiscordProvider, Log,
	       TEXT("TicketDiscordProvider: Identify sent (intents=%d)."), TSGatewayIntents);
}

void UTicketDiscordProvider::SendHeartbeat()
{
	// Zombie-connection detection: if the previous heartbeat was never acked,
	// force a fresh connection (same logic as DiscordBridgeSubsystem).
	if (bPendingHeartbeatAck)
	{
		UE_LOG(LogTicketDiscordProvider, Warning,
		       TEXT("TicketDiscordProvider: Heartbeat not acknowledged – "
		            "zombie connection detected. Reconnecting."));

		FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
		HeartbeatTickerHandle.Reset();
		bPendingHeartbeatAck = false;
		bGatewayReady        = false;
		LastSequenceNumber   = -1;
		BotUserId.Empty();
		if (GuildIdOverride.IsEmpty())
		{
			GuildId.Empty();
		}
		GuildOwnerId.Empty();

		if (WebSocketClient)
		{
			WebSocketClient->Connect(TSDiscordGatewayUrl, {}, {});
		}
		return;
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetNumberField(TEXT("op"), ETSGatewayOpcode::Heartbeat);

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

bool UTicketDiscordProvider::HeartbeatTick(float /*DeltaTime*/)
{
	SendHeartbeat();
	return true; // keep ticking
}

void UTicketDiscordProvider::SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload)
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
// HTTP helpers
// ─────────────────────────────────────────────────────────────────────────────

void UTicketDiscordProvider::DiscordHttp(const FString& Verb, const FString& Url,
                                         const FString& Body)
{
	DiscordHttpWithCallback(Verb, Url, Body, nullptr);
}

void UTicketDiscordProvider::DiscordHttpWithCallback(
	const FString& Verb, const FString& Url, const FString& Body,
	TFunction<void(int32, const FString&)> OnComplete)
{
	if (BotToken.IsEmpty())
	{
		return;
	}

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(Verb);
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *BotToken));

	if (!Body.IsEmpty())
	{
		Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
		Request->SetContentAsString(Body);
	}

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[Url, OnComplete = MoveTemp(OnComplete)]
		(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected) mutable
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTicketDiscordProvider, Warning,
				       TEXT("TicketDiscordProvider: HTTP request failed (%s)."), *Url);
				if (OnComplete)
				{
					OnComplete(0, TEXT(""));
				}
				return;
			}
			const int32 Code = Resp->GetResponseCode();
			if (Code < 200 || Code >= 300)
			{
				UE_LOG(LogTicketDiscordProvider, Warning,
				       TEXT("TicketDiscordProvider: HTTP %d for %s: %s"),
				       Code, *Url, *Resp->GetContentAsString());
			}
			if (OnComplete)
			{
				OnComplete(Code, Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// IDiscordBridgeProvider – getters
// ─────────────────────────────────────────────────────────────────────────────

const FString& UTicketDiscordProvider::GetBotToken() const    { return BotToken;     }
const FString& UTicketDiscordProvider::GetGuildId() const     { return GuildId;      }
const FString& UTicketDiscordProvider::GetGuildOwnerId() const { return GuildOwnerId; }

// ─────────────────────────────────────────────────────────────────────────────
// IDiscordBridgeProvider – delegate subscriptions
// ─────────────────────────────────────────────────────────────────────────────

FDelegateHandle UTicketDiscordProvider::SubscribeInteraction(
	TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
	return OnInteraction.AddLambda(MoveTemp(Callback));
}

void UTicketDiscordProvider::UnsubscribeInteraction(FDelegateHandle Handle)
{
	OnInteraction.Remove(Handle);
}

FDelegateHandle UTicketDiscordProvider::SubscribeRawMessage(
	TFunction<void(const TSharedPtr<FJsonObject>&)> Callback)
{
	return OnRawMessage.AddLambda(MoveTemp(Callback));
}

void UTicketDiscordProvider::UnsubscribeRawMessage(FDelegateHandle Handle)
{
	OnRawMessage.Remove(Handle);
}

// ─────────────────────────────────────────────────────────────────────────────
// IDiscordBridgeProvider – REST helpers
// ─────────────────────────────────────────────────────────────────────────────

void UTicketDiscordProvider::RespondToInteraction(const FString& InteractionId,
                                                  const FString& InteractionToken,
                                                  int32 ResponseType,
                                                  const FString& Content,
                                                  bool bEphemeral)
{
	if (InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	if (ResponseType == 4 && !Content.IsEmpty())
	{
		ResponseData->SetStringField(TEXT("content"), Content);
		if (bEphemeral)
		{
			ResponseData->SetNumberField(TEXT("flags"), 64); // EPHEMERAL
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
		*TSDiscordApiBase, *InteractionId, *InteractionToken);

	DiscordHttp(TEXT("POST"), Url, BodyString);
}

void UTicketDiscordProvider::RespondWithModal(const FString& InteractionId,
                                              const FString& InteractionToken,
                                              const FString& ModalCustomId,
                                              const FString& Title,
                                              const FString& Placeholder)
{
	if (InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> TextInput = MakeShared<FJsonObject>();
	TextInput->SetNumberField(TEXT("type"),        4); // TEXT_INPUT
	TextInput->SetStringField(TEXT("custom_id"),   TEXT("ticket_reason"));
	TextInput->SetNumberField(TEXT("style"),       2); // PARAGRAPH
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
		*TSDiscordApiBase, *InteractionId, *InteractionToken);

	DiscordHttp(TEXT("POST"), Url, BodyString);
}

void UTicketDiscordProvider::SendDiscordChannelMessage(const FString& TargetChannelId,
                                                       const FString& Message)
{
	if (TargetChannelId.IsEmpty())
	{
		return;
	}

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"), Message);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s/messages"), *TSDiscordApiBase, *TargetChannelId);

	DiscordHttp(TEXT("POST"), Url, BodyString);
}

void UTicketDiscordProvider::SendMessageBodyToChannel(const FString& TargetChannelId,
                                                      const TSharedPtr<FJsonObject>& MessageBody)
{
	if (TargetChannelId.IsEmpty() || !MessageBody.IsValid())
	{
		return;
	}

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(MessageBody.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s/messages"), *TSDiscordApiBase, *TargetChannelId);

	DiscordHttp(TEXT("POST"), Url, BodyString);
}

void UTicketDiscordProvider::DeleteDiscordChannel(const FString& ChannelId)
{
	if (ChannelId.IsEmpty())
	{
		return;
	}

	const FString Url = FString::Printf(
		TEXT("%s/channels/%s"), *TSDiscordApiBase, *ChannelId);

	DiscordHttp(TEXT("DELETE"), Url);
}

void UTicketDiscordProvider::CreateDiscordGuildTextChannel(
	const FString& ChannelName,
	const FString& CategoryId,
	const TArray<TSharedPtr<FJsonValue>>& PermissionOverwrites,
	TFunction<void(const FString& NewChannelId)> OnCreated)
{
	if (GuildId.IsEmpty())
	{
		UE_LOG(LogTicketDiscordProvider, Warning,
		       TEXT("TicketDiscordProvider: CreateDiscordGuildTextChannel – "
		            "GuildId is empty. Set GuildId in DefaultTickets.ini or wait for READY."));
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
		TEXT("%s/guilds/%s/channels"), *TSDiscordApiBase, *GuildId);

	DiscordHttpWithCallback(TEXT("POST"), Url, BodyString,
		[OnCreated = MoveTemp(OnCreated), ChannelName]
		(int32 Code, const FString& ResponseBody) mutable
		{
			if (Code != 200 && Code != 201)
			{
				UE_LOG(LogTicketDiscordProvider, Warning,
				       TEXT("TicketDiscordProvider: CreateDiscordGuildTextChannel "
				            "failed for '%s'. HTTP %d."),
				       *ChannelName, Code);
				if (OnCreated)
				{
					OnCreated(TEXT(""));
				}
				return;
			}

			TSharedPtr<FJsonObject> ChannelObj;
			TSharedRef<TJsonReader<>> Reader =
				TJsonReaderFactory<>::Create(ResponseBody);
			FString NewChannelId;
			if (FJsonSerializer::Deserialize(Reader, ChannelObj) &&
			    ChannelObj.IsValid())
			{
				ChannelObj->TryGetStringField(TEXT("id"), NewChannelId);
			}

			if (OnCreated)
			{
				OnCreated(NewChannelId);
			}
		});
}
