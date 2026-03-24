// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "Containers/Ticker.h"
#include "IDiscordBridgeProvider.h"
#include "SMLWebSocketClient.h"
#include "Dom/JsonObject.h"
#include "TicketDiscordProvider.generated.h"

// Internal multicast delegates used to implement the SubscribeInteraction /
// SubscribeRawMessage contract defined by IDiscordBridgeProvider.
DECLARE_MULTICAST_DELEGATE_OneParam(FTicketProviderInteractionDelegate,
                                    const TSharedPtr<FJsonObject>& /*DataObj*/);
DECLARE_MULTICAST_DELEGATE_OneParam(FTicketProviderRawMessageDelegate,
                                    const TSharedPtr<FJsonObject>& /*MessageObj*/);

/**
 * UTicketDiscordProvider
 *
 * A self-contained Discord Gateway + REST provider that lives entirely inside
 * the TicketSystem mod.  It implements every IDiscordBridgeProvider method so
 * that UTicketSubsystem can work without the DiscordBridge mod when a
 * BotToken is supplied in DefaultTickets.ini.
 *
 * Lifecycle
 * ─────────
 *  1. UTicketSubsystem::Initialize() calls Connect() when BotToken is set.
 *  2. Connect() opens a WebSocket to the Discord Gateway, completes the
 *     Hello → Identify → Ready handshake, and starts a heartbeat ticker.
 *  3. On READY the GuildId and BotUserId are captured.
 *  4. INTERACTION_CREATE events are broadcast via OnInteraction so that
 *     UTicketSubsystem receives button clicks and modal submits.
 *  5. MESSAGE_CREATE events are broadcast via OnRawMessage so that
 *     UTicketSubsystem can detect the "!ticket-panel" command.
 *  6. When the DiscordBridge mod is also installed it calls
 *     UTicketSubsystem::SetProvider(this).  TicketSubsystem then calls
 *     Disconnect() on this provider and switches to DiscordBridge.
 *  7. Disconnect() closes the WebSocket and cancels the heartbeat ticker.
 */
UCLASS()
class TICKETSYSTEM_API UTicketDiscordProvider : public UObject, public IDiscordBridgeProvider
{
	GENERATED_BODY()

public:
	/**
	 * Connect to the Discord Gateway using the given bot token.
	 *
	 * Safe to call multiple times; a live connection is left unchanged.
	 *
	 * @param InBotToken         Discord bot token (required).
	 * @param InGuildIdOverride  Optional guild ID snowflake.  When non-empty this
	 *                           value is returned by GetGuildId() before the READY
	 *                           event confirms the guild.  Usually leave empty so
	 *                           the ID is derived automatically from READY.
	 */
	void Connect(const FString& InBotToken, const FString& InGuildIdOverride = TEXT(""));

	/** Close the WebSocket connection and cancel all timers. */
	void Disconnect();

	// ── IDiscordBridgeProvider ────────────────────────────────────────────────

	virtual const FString& GetBotToken() const override;
	virtual const FString& GetGuildId() const override;
	virtual const FString& GetGuildOwnerId() const override;

	virtual FDelegateHandle SubscribeInteraction(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) override;
	virtual void UnsubscribeInteraction(FDelegateHandle Handle) override;

	virtual FDelegateHandle SubscribeRawMessage(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) override;
	virtual void UnsubscribeRawMessage(FDelegateHandle Handle) override;

	virtual void RespondToInteraction(const FString& InteractionId,
	                                  const FString& InteractionToken,
	                                  int32 ResponseType,
	                                  const FString& Content,
	                                  bool bEphemeral) override;

	virtual void RespondWithModal(const FString& InteractionId,
	                              const FString& InteractionToken,
	                              const FString& ModalCustomId,
	                              const FString& Title,
	                              const FString& Placeholder,
	                              const FString& ComponentCustomId = TEXT("ticket_reason")) override;

	virtual void SendDiscordChannelMessage(const FString& TargetChannelId,
	                                       const FString& Message) override;

	virtual void SendMessageBodyToChannel(const FString& TargetChannelId,
	                                      const TSharedPtr<FJsonObject>& MessageBody) override;

	virtual void DeleteDiscordChannel(const FString& ChannelId) override;

	virtual void CreateDiscordGuildTextChannel(
		const FString& ChannelName,
		const FString& CategoryId,
		const TArray<TSharedPtr<FJsonValue>>& PermissionOverwrites,
		TFunction<void(const FString& NewChannelId)> OnCreated) override;

private:
	// ── WebSocket event handlers (bound via AddDynamic) ───────────────────────

	UFUNCTION()
	void OnWebSocketConnected();

	UFUNCTION()
	void OnWebSocketMessage(const FString& RawJson);

	UFUNCTION()
	void OnWebSocketClosed(int32 StatusCode, const FString& Reason);

	UFUNCTION()
	void OnWebSocketError(const FString& ErrorMessage);

	UFUNCTION()
	void OnWebSocketReconnecting(int32 AttemptNumber, float DelaySeconds);

	// ── Discord Gateway protocol ──────────────────────────────────────────────

	void HandleGatewayPayload(const FString& RawJson);
	void HandleHello(const TSharedPtr<FJsonObject>& DataObj);
	void HandleDispatch(const FString& EventType, const TSharedPtr<FJsonObject>& DataObj);
	void HandleReady(const TSharedPtr<FJsonObject>& DataObj);
	void HandleHeartbeatAck();
	void HandleReconnect();
	void HandleInvalidSession(bool bResumable);

	void SendIdentify();
	void SendHeartbeat();
	bool HeartbeatTick(float DeltaTime);
	void SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload);

	// ── HTTP helpers ──────────────────────────────────────────────────────────

	/** Fire-and-forget HTTP request (logs non-2xx responses). */
	void DiscordHttp(const FString& Verb, const FString& Url,
	                 const FString& Body = TEXT(""));

	/** HTTP request with a response callback (used by CreateDiscordGuildTextChannel). */
	void DiscordHttpWithCallback(const FString& Verb, const FString& Url,
	                             const FString& Body,
	                             TFunction<void(int32, const FString&)> OnComplete);

	// ── State ─────────────────────────────────────────────────────────────────

	UPROPERTY()
	USMLWebSocketClient* WebSocketClient = nullptr;

	FString BotToken;
	FString GuildId;         ///< Populated from READY (or GuildIdOverride if set).
	FString GuildIdOverride; ///< Optional user-supplied value from config.
	FString GuildOwnerId;    ///< Populated from GUILD_CREATE.
	FString BotUserId;       ///< Populated from READY.

	int32 LastSequenceNumber    = -1;
	bool  bGatewayReady         = false;
	bool  bPendingHeartbeatAck  = false;
	float HeartbeatIntervalSeconds = 41.25f;

	FTSTicker::FDelegateHandle HeartbeatTickerHandle;

	FTicketProviderInteractionDelegate OnInteraction;
	FTicketProviderRawMessageDelegate  OnRawMessage;
};
