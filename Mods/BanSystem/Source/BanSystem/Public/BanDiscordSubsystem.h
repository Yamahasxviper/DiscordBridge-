// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "IBanDiscordCommandProvider.h"
#include "BanDiscordConfig.h"
#include "SMLWebSocketClient.h"
#include "EOSTypes.h"
#include "BanDiscordSubsystem.generated.h"

/**
 * UBanDiscordSubsystem
 *
 * GameInstance-level subsystem that enables BanSystem's ban commands
 * (steamban, eosban, steamunban, eosunban, steambanlist, eosbanlist, banbyname,
 * playerids) to be run from Discord.
 *
 * STANDALONE MODE (recommended)
 * ──────────────────────────────
 * Set BotToken in Mods/BanSystem/Config/DefaultBanSystem.ini.
 * BanSystem connects to Discord Gateway independently via SMLWebSocket and
 * handles all bot activity itself — no DiscordBridge or any other mod required.
 *
 * SHARED CONNECTION MODE (optional)
 * ───────────────────────────────────
 * Leave BotToken empty and let an external mod (e.g. DiscordBridge) call
 * SetCommandProvider(provider) to share its existing Discord connection.
 * In this mode the subsystem starts up but waits silently until a provider
 * is injected.
 *
 * Config
 * ──────
 *   <ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
 *   Key settings: BotToken, DiscordChannelId, DiscordCommandRoleId, command prefixes.
 *   See BanDiscordConfig.h for the full list.
 */
UCLASS()
class BANSYSTEM_API UBanDiscordSubsystem : public UGameInstanceSubsystem,
                                           public IBanDiscordCommandProvider
{
	GENERATED_BODY()

public:
	// ── USubsystem ────────────────────────────────────────────────────────────

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── External provider registration ───────────────────────────────────────
	//
	// External mods (e.g. DiscordBridge) may call SetCommandProvider(provider)
	// to override or supplement the built-in standalone connection.  Passing
	// nullptr falls back to the built-in standalone connection when BotToken is
	// configured, or disables all Discord functionality when it is not.
	//
	// This method is intentionally NOT part of IBanDiscordCommandProvider —
	// it is for the receiving end, not the providing end.

	/**
	 * Inject an external Discord command provider.
	 * The external provider's connection will be used instead of the built-in
	 * standalone connection for as long as it remains registered.
	 * Pass nullptr to remove the external provider.
	 */
	void SetCommandProvider(IBanDiscordCommandProvider* InProvider);

	/** Returns the loaded configuration (read-only). */
	const FBanDiscordConfig& GetConfig() const { return Config; }

	// ── IBanDiscordCommandProvider (standalone implementation) ────────────────
	//
	// These implement the interface against BanSystem's own Gateway connection.
	// When an external provider is active, subscriptions and sends are forwarded
	// to that provider instead.

	virtual FDelegateHandle SubscribeDiscordMessages(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) override;

	virtual void UnsubscribeDiscordMessages(FDelegateHandle Handle) override;

	virtual void SendDiscordChannelMessage(const FString& ChannelId,
	                                       const FString& Message) override;

	virtual const FString& GetGuildOwnerId() const override;

private:
	// ── Discord message handling ──────────────────────────────────────────────

	/** Called on the game thread for every Discord MESSAGE_CREATE event. */
	void OnDiscordMessageReceived(const TSharedPtr<FJsonObject>& MessageObj);

	/**
	 * Returns true if the message sender is authorised to run BanSystem
	 * commands (holds DiscordCommandRoleId OR is the guild owner).
	 */
	bool HasCommandPermission(const TSharedPtr<FJsonObject>& MessageObj,
	                          const FString&                  AuthorId) const;

	// ── Per-command handlers ──────────────────────────────────────────────────

	/** !steamban <ID|Name> [minutes] [reason] */
	void HandleSteamBanCommand(const FString& Args,
	                           const FString& IssuedBy,
	                           const FString& ChannelId);

	/** !steamunban <Steam64Id> */
	void HandleSteamUnbanCommand(const FString& Args,
	                             const FString& ChannelId);

	/** !steambanlist */
	void HandleSteamBanListCommand(const FString& ChannelId);

	/** !eosban <ID|Name> [minutes] [reason] */
	void HandleEOSBanCommand(const FString& Args,
	                         const FString& IssuedBy,
	                         const FString& ChannelId);

	/** !eosunban <EOSProductUserId> */
	void HandleEOSUnbanCommand(const FString& Args,
	                           const FString& ChannelId);

	/** !eosbanlist */
	void HandleEOSBanListCommand(const FString& ChannelId);

	/** !banbyname <Name> [minutes] [reason] */
	void HandleBanByNameCommand(const FString& Args,
	                            const FString& IssuedBy,
	                            const FString& ChannelId);

	/** !playerids [Name] */
	void HandlePlayerIdsCommand(const FString& Args,
	                            const FString& ChannelId);

	/** !checkban <Steam64Id|EOSProductUserId|PlayerName> */
	void HandleCheckBanCommand(const FString& Args,
	                           const FString& ChannelId);

	// ── Helpers ───────────────────────────────────────────────────────────────

	/** Send a response message (no-op when neither standalone nor external provider). */
	void Reply(const FString& ChannelId, const FString& Message);

	/** Format ban duration for user-facing messages. */
	static FString FormatDuration(int32 DurationMinutes);

	/**
	 * Split a Discord command argument string into an array of tokens.
	 *
	 * Supports quoted strings so that multi-word player names can be passed as a
	 * single token.  Double-quoted sequences are kept together with the surrounding
	 * quotes removed, e.g.:
	 *   Input:  !banbyname "John Doe" 60 reason
	 *   Result: ["John Doe", "60", "reason"]
	 *
	 * Unquoted tokens are split on any whitespace (unchanged behaviour).
	 */
	static TArray<FString> SplitArgs(const FString& Input);

	// ── Standalone Discord Gateway ────────────────────────────────────────────
	// Used only when BotToken is configured and no external provider is active.

	/** Connect to the Discord Gateway.  No-op when already connected or no BotToken. */
	void Connect();

	/** Disconnect from the Discord Gateway and cancel heartbeat. */
	void Disconnect();

	/** Send a JSON payload as a WebSocket text frame. */
	void SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload);

	/** op=2 Identify */
	void SendIdentify();

	/**
	 * op=6 Resume — attempt to resume an interrupted Gateway session.
	 * Requires SessionId and LastSequenceNumber to be populated.
	 * Falls back to SendIdentify() when SessionId is empty.
	 */
	void SendResume();

	/** op=1 Heartbeat */
	void SendHeartbeat();

	/** Send a plain-text REST message to a Discord channel using our own BotToken. */
	void SendMessageToChannelInternal(const FString& ChannelId, const FString& Message);

	// ── Gateway event handlers ────────────────────────────────────────────────

	UFUNCTION()
	void OnWebSocketConnected();

	UFUNCTION()
	void OnWebSocketMessage(const FString& RawJson);

	UFUNCTION()
	void OnWebSocketClosed(int32 StatusCode, const FString& Reason);

	UFUNCTION()
	void OnWebSocketError(const FString& ErrorMessage);

	void HandleGatewayPayload(const FString& RawJson);
	void HandleHello(const TSharedPtr<FJsonObject>& DataObj);
	void HandleDispatch(const FString& EventType, int32 Sequence,
	                    const TSharedPtr<FJsonObject>& DataObj);
	void HandleHeartbeatAck();
	void HandleInvalidSession(bool bResumable);
	void HandleReady(const TSharedPtr<FJsonObject>& DataObj);
	void HandleResumed();
	void HandleMessageCreate(const TSharedPtr<FJsonObject>& DataObj);
	void HandleGuildCreate(const TSharedPtr<FJsonObject>& DataObj);

	/** Timer callback — fires SendHeartbeat() at the Discord-requested interval. */
	bool HeartbeatTick(float DeltaTime);

	// ── State ─────────────────────────────────────────────────────────────────

	/** Active external provider; nullptr = use built-in standalone connection. */
	IBanDiscordCommandProvider* ExternalProvider = nullptr;

	/** Handle from the external provider's SubscribeDiscordMessages() call. */
	FDelegateHandle ExternalMessageSubscriptionHandle;

	/**
	 * Internal multicast delegate — fired for each Discord MESSAGE_CREATE event
	 * when using the built-in standalone connection.
	 */
	DECLARE_MULTICAST_DELEGATE_OneParam(FOnRawDiscordMessage, const TSharedPtr<FJsonObject>&);
	FOnRawDiscordMessage OnRawDiscordMessage;

	/** Loaded configuration (bot token, channel, role, prefixes, formats). */
	FBanDiscordConfig Config;

	// ── Standalone Gateway state ──────────────────────────────────────────────

	/** WebSocket client for the standalone Discord Gateway connection. */
	UPROPERTY()
	USMLWebSocketClient* WebSocketClient{nullptr};

	/** true after the READY dispatch has been received. */
	bool bGatewayReady{false};

	/** true when we sent a heartbeat and are waiting for the op=11 ack. */
	bool bPendingHeartbeatAck{false};

	/** Last received sequence number (used in heartbeats). */
	int32 LastSequenceNumber{-1};

	/** Snowflake ID of this bot user; used to ignore self-sent messages. */
	FString BotUserId;

	/** Snowflake ID of the guild owner; populated from GUILD_CREATE. */
	FString GuildOwnerId;

	/** Session ID from the READY event; used to resume after reconnect. */
	FString SessionId;

	/** Resume gateway URL from the READY event; used as the reconnect target. */
	FString ResumeGatewayUrl;

	/** Heartbeat ticker handle. */
	FTSTicker::FDelegateHandle HeartbeatTickerHandle;
	float HeartbeatIntervalSeconds{0.0f};

	static const FString DiscordGatewayUrl;
	static const FString DiscordApiBase;
};
