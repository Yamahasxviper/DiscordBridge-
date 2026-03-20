// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SMLWebSocketClient.h"
#include "DiscordBridgeConfig.h"
#include "FGChatManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "DiscordBotFileLogger.h"
#include "GameplayTagContainer.h"
#include "GameplayEvent.h"
#include "DiscordBridgeSubsystem.generated.h"

// ── Delegate declarations ─────────────────────────────────────────────────────

/**
 * Fired on the game thread when a message is received from the bridged Discord
 * channel.  Bind this in Blueprint to forward the message to in-game chat.
 *
 * @param Username  Display name of the Discord user who sent the message.
 * @param Message   Plain text content of the Discord message.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FDiscordMessageReceivedDelegate,
                                             const FString&, Username,
                                             const FString&, Message);

/**
 * Fired on the game thread when the Discord Gateway connection is established
 * and the bot has been identified successfully (Ready event received).
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FDiscordConnectedDelegate);

/**
 * Fired on the game thread when the Discord Gateway connection is lost.
 *
 * @param Reason  Human-readable description of why the connection ended.
 */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDiscordDisconnectedDelegate,
                                            const FString&, Reason);

// ── Discord Gateway opcodes (Discord API reference §Gateway Opcodes) ──────────
namespace EDiscordGatewayOpcode
{
	static constexpr int32 Dispatch          = 0;  // Server → Client: an event was dispatched
	static constexpr int32 Heartbeat         = 1;  // Client → Server: keep-alive heartbeat
	static constexpr int32 Identify          = 2;  // Client → Server: trigger authentication
	static constexpr int32 UpdatePresence    = 3;  // Client → Server: update bot presence/status
	static constexpr int32 Reconnect         = 7;  // Server → Client: client should reconnect
	static constexpr int32 InvalidSession    = 9;  // Server → Client: session is invalid
	static constexpr int32 Hello            = 10;  // Server → Client: sent immediately after connecting
	static constexpr int32 HeartbeatAck     = 11;  // Server → Client: heartbeat was acknowledged
}

// ── Discord Gateway intent bit-flags (Discord API reference §Gateway Intents) ─
namespace EDiscordGatewayIntent
{
	// Non-privileged
	static constexpr int32 Guilds         = 1 << 0;   //    1
	static constexpr int32 GuildMessages  = 1 << 9;   //  512

	// Privileged – must be enabled in the Discord Developer Portal
	static constexpr int32 GuildMembers   = 1 << 1;   //    2  (Server Members Intent)
	static constexpr int32 MessageContent = 1 << 15;  // 32768 (Message Content Intent)

	// Combined value used when connecting to the Gateway.
	static constexpr int32 All =
		Guilds | GuildMembers | GuildMessages | MessageContent;
	// = 1 + 2 + 512 + 32768 = 33283
}

// ── ReliableMessaging channel constants ───────────────────────────────────────
/**
 * Well-known ReliableMessaging channel IDs used by DiscordBridge.
 *
 * Client-side mods can send a UTF-8 encoded message to the Discord bot by
 * obtaining their PlayerController's UReliableMessagingPlayerComponent and
 * calling:
 *
 *   UReliableMessagingPlayerComponent::GetFromPlayer(MyPC)
 *       ->SendMessage(EDiscordRelayChannel::ForwardToDiscord, Payload);
 *
 * where Payload is the message text encoded as UTF-8 bytes (no null terminator
 * needed).  DiscordBridge relays the message to the main Discord channel(s)
 * using the GameToDiscordFormat template (with the in-game player name).
 *
 * The channel number 200 was chosen to avoid conflicts with channels used by
 * the game itself (which starts from 0) while leaving room for other mods to
 * allocate channels in the 1–199 range.
 */
namespace EDiscordRelayChannel
{
	/** Send a UTF-8 text message that DiscordBridge will relay to Discord. */
	static constexpr int32 ForwardToDiscord = 200;
}

/**
 * UDiscordBridgeSubsystem
 *
 * A GameInstance-level subsystem that bridges Satisfactory in-game chat with
 * a Discord text channel.
 *
 * How it works
 * ────────────
 *  • Connects to the Discord Gateway (wss://gateway.discord.gg/?v=10&encoding=json)
 *    using USMLWebSocketClient from the SMLWebSocket plugin.
 *  • Authenticates with the configured BotToken and requests two privileged
 *    intents: Server Members Intent and Message Content Intent.
 *  • Discord → Game: MESSAGE_CREATE events on the configured channel fire
 *    OnDiscordMessageReceived so that Blueprint (or another C++ subsystem) can
 *    inject the message into the Satisfactory chat.
 *  • Game → Discord: AFGChatManager::OnChatMessageAdded is bound via a
 *    periodic ticker so that every CMT_PlayerMessage added to the server's
 *    chat history is forwarded to Discord through the REST API.  The delegate
 *    approach avoids funchook entirely, which is necessary because
 *    AddChatMessageToReceived, BroadcastChatMessage, and related functions are
 *    all too short for funchook to patch in the shipped game binary.
 *
 * Setup
 * ─────
 *  1. Create a Discord application and bot in the Discord Developer Portal.
 *  2. Enable the following Privileged Gateway Intents on the Bot page:
 *       – Server Members Intent
 *       – Message Content Intent
 *  3. Invite the bot to your server with "Send Messages" + "Read Message History".
 *  4. Fill in BotToken and ChannelId in Mods/DiscordBridge/Config/DefaultDiscordBridge.ini
 *     (auto-created on first server start) and restart the server.
 *  5. Optionally customise GameToDiscordFormat and DiscordToGameFormat.
 *  6. In Blueprint, bind to OnDiscordMessageReceived and call
 *     SendGameMessageToDiscord() from your chat hooks.
 */
UCLASS(BlueprintType)
class DISCORDBRIDGE_API UDiscordBridgeSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── USubsystem ────────────────────────────────────────────────────────────

	/** Allow the subsystem on dedicated servers and listen servers, but not in the editor. */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Delegates ─────────────────────────────────────────────────────────────

	/**
	 * Fired when a message is received from the bridged Discord channel.
	 * Bind this in Blueprint to forward the message to the Satisfactory chat.
	 */
	UPROPERTY(BlueprintAssignable, Category="Discord Bridge")
	FDiscordMessageReceivedDelegate OnDiscordMessageReceived;

	/** Fired when the Discord Gateway connection is ready. */
	UPROPERTY(BlueprintAssignable, Category="Discord Bridge")
	FDiscordConnectedDelegate OnDiscordConnected;

	/** Fired when the Discord Gateway connection is lost. */
	UPROPERTY(BlueprintAssignable, Category="Discord Bridge")
	FDiscordDisconnectedDelegate OnDiscordDisconnected;

	// ── Public API ────────────────────────────────────────────────────────────

	/**
	 * Forward a Satisfactory chat message to the bridged Discord channel via
	 * the Discord REST API.
	 *
	 * @param PlayerName  The in-game name of the player who sent the message.
	 *                    Substituted for {PlayerName} in GameToDiscordFormat.
	 * @param Message     The chat message text.
	 *                    Substituted for {Message} in GameToDiscordFormat.
	 */
	UFUNCTION(BlueprintCallable, Category="Discord Bridge")
	void SendGameMessageToDiscord(const FString& PlayerName, const FString& Message);

	/**
	 * Manually trigger a connection to the Discord Gateway.
	 * Called automatically during Initialize() when BotToken and ChannelId are set.
	 */
	UFUNCTION(BlueprintCallable, Category="Discord Bridge")
	void Connect();

	/**
	 * Disconnect from the Discord Gateway and cancel the heartbeat timer.
	 * Called automatically during Deinitialize().
	 */
	UFUNCTION(BlueprintCallable, Category="Discord Bridge")
	void Disconnect();

	/**
	 * Send a presence update to Discord to set the bot's online status.
	 *
	 * @param Status  One of "online", "idle", "dnd", or "invisible".
	 */
	UFUNCTION(BlueprintCallable, Category="Discord Bridge")
	void SendUpdatePresence(const FString& Status);

	/**
	 * Returns true when the Gateway WebSocket is open and the bot has been
	 * identified (Ready event received from Discord).
	 */
	UFUNCTION(BlueprintPure, Category="Discord Bridge")
	bool IsConnected() const { return bGatewayReady; }

	/**
	 * Called by OnGameChatMessageAdded every time a player-chat message lands
	 * in the server's history.  Forwards the message to Discord.
	 *
	 * @param PlayerName  Value of FChatMessageStruct::MessageSender (may be empty
	 *                    if the game does not populate it server-side; falls back
	 *                    to "Unknown" inside SendGameMessageToDiscord).
	 * @param MessageText Plain text of the chat message.
	 */
	void HandleIncomingChatMessage(const FString& PlayerName, const FString& MessageText);

	/**
	 * Returns the operator-configured ban kick reason string from Config.
	 * Used by the module-level Login hook (DiscordBridge.cpp) to produce the
	 * ErrorMessage when a banned player's platform ID is detected at Login time.
	 *
	 * Returns an empty string when no custom reason is configured, in which
	 * case callers fall back to "You are banned from this server."
	 */
	const FString& GetBanKickReason() const { return Config.BanKickReason; }

	/**
	 * Called by OnPostLogin when a player is kicked due to a platform-ID ban,
	 * after they have connected but before they can interact with the game world.
	 * Sends the configured BanLoginRejectDiscordMessage to the main channel(s)
	 * and ban channel(s).
	 *
	 * Rate-limited to at most one notification per platform ID per 60 seconds
	 * to prevent Discord spam if a banned player retries the connection rapidly.
	 *
	 * No-op when BanLoginRejectDiscordMessage is empty or the Discord gateway
	 * is not ready.
	 *
	 * @param PlatformId  The banned platform ID (Steam64 or EOS PUID) that was
	 *                    rejected.  Must not be empty.
	 */
	void NotifyLoginBanReject(const FString& PlatformId);

private:
	// ── WebSocket event handlers (called on the game thread) ──────────────────

	/**
	 * Performs the actual server-side initialization (config load, connect, etc.).
	 * Called immediately on dedicated servers; deferred via
	 * FWorldDelegates::OnPostWorldInitialization on listen servers so that the
	 * world's net mode is known before we start the bot.
	 */
	void InitializeServer();

	/**
	 * Bound to FCoreDelegates::OnHandleSystemError.  Fires early in the UE crash
	 * path (assertion failures, fatal errors, access-violation handlers) before
	 * the platform crash reporter terminates the process.
	 *
	 * Sets bWasCrash and attempts a best-effort HTTP notification to Discord.
	 * The HTTP request is fire-and-forget: it is dispatched immediately but
	 * there is no guarantee it completes before the process exits.
	 */
	void OnSystemError();

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

	// ── Chat capture (Game → Discord) ─────────────────────────────────────────

	/**
	 * Bound to AFGChatManager::OnChatMessageAdded.  Diffs the current message
	 * list against the previous snapshot and forwards any new CMT_PlayerMessage
	 * entries to Discord.
	 */
	UFUNCTION()
	void OnGameChatMessageAdded();

	/**
	 * Attempts to locate AFGChatManager in the current world and bind
	 * OnGameChatMessageAdded to its OnChatMessageAdded delegate.
	 * Returns true when binding succeeds; the caller's ticker stops on success.
	 */
	bool TryBindToChatManager();

	// ── Discord Gateway protocol ──────────────────────────────────────────────

	/** Dispatch the correct handler based on the Gateway opcode. */
	void HandleGatewayPayload(const FString& RawJson);

	/** op=10: Server sent Hello; start heartbeating and send Identify. */
	void HandleHello(const TSharedPtr<FJsonObject>& DataObj);

	/** op=0: Server dispatched an event. Routes to the correct event handler. */
	void HandleDispatch(const FString& EventType, int32 Sequence,
	                    const TSharedPtr<FJsonObject>& DataObj);

	/** op=11: Server acknowledged our heartbeat. */
	void HandleHeartbeatAck();

	/** op=7: Server asked us to reconnect. */
	void HandleReconnect();

	/** op=9: Session is invalid; re-identify or start fresh. */
	void HandleInvalidSession(bool bResumable);

	/** t=READY: Bot is authenticated and ready. */
	void HandleReady(const TSharedPtr<FJsonObject>& DataObj);

	/** t=MESSAGE_CREATE: A new message was posted in a channel. */
	void HandleMessageCreate(const TSharedPtr<FJsonObject>& DataObj);

	/**
	 * Automatically relays an incoming Discord message to the Satisfactory in-game
	 * chat via AFGChatManager::BroadcastChatMessage.
	 * Bound to OnDiscordMessageReceived in Initialize() so messages are bridged
	 * without requiring any Blueprint wiring.
	 */
	UFUNCTION()
	void RelayDiscordMessageToGame(const FString& Username, const FString& Message);

	/** Send the Identify payload (op=2) to authenticate the bot. */
	void SendIdentify();

	/** Send a heartbeat (op=1) to keep the Gateway connection alive. */
	void SendHeartbeat();

	/** Send a plain text message to the configured Discord channel via the REST API. */
	void SendStatusMessageToDiscord(const FString& Message);

	/** Send a plain text message to a specific Discord channel via the REST API.
	 *  When TargetChannelId is empty, the call is a no-op. */
	void SendMessageToChannel(const FString& TargetChannelId, const FString& Message);

	/** Serialise a JSON object and send it as a text WebSocket frame. */
	void SendGatewayPayload(const TSharedPtr<FJsonObject>& Payload);

	// ── Heartbeat timer ───────────────────────────────────────────────────────

	/** Timer callback – fires SendHeartbeat() at the interval Discord requested. */
	bool HeartbeatTick(float DeltaTime);

	FTSTicker::FDelegateHandle HeartbeatTickerHandle;
	float HeartbeatIntervalSeconds{0.0f};

	// ── Player count presence ─────────────────────────────────────────────────

	/**
	 * Queries the current connected player count, formats it using
	 * PlayerCountPresenceFormat, and sends a Discord presence update
	 * (op=3) containing a "Watching" activity with the formatted text.
	 * No-ops when the Gateway is not ready or the feature is disabled.
	 */
	void UpdatePlayerCountPresence();

	/** Timer callback – fires UpdatePlayerCountPresence() at the configured interval. */
	bool PlayerCountTick(float DeltaTime);

	FTSTicker::FDelegateHandle PlayerCountTickerHandle;

	// ── Internal state ────────────────────────────────────────────────────────

	/** The WebSocket client connected to the Discord Gateway. */
	UPROPERTY()
	USMLWebSocketClient* WebSocketClient{nullptr};

	/** Loaded configuration (populated in Initialize()). */
	FDiscordBridgeConfig Config;

	/** Last sequence number received from Discord (used in heartbeats). */
	int32 LastSequenceNumber{-1};

	/** true after the READY dispatch has been received from Discord. */
	bool bGatewayReady{false};

	/**
	 * true when a heartbeat has been sent and we are waiting for the server's
	 * op=11 HeartbeatAck.  If this flag is still true when the next heartbeat
	 * is due, the connection is a zombie (Discord's term) and must be closed so
	 * that USMLWebSocketClient's auto-reconnect can re-establish it.
	 */
	bool bPendingHeartbeatAck{false};

	/**
	 * true after the ServerOnlineMessage has been sent for the current server
	 * session.  Prevents the message from being re-sent on Discord Gateway
	 * reconnects (which happen periodically and trigger a fresh READY event
	 * even when the game server has not restarted).
	 * Reset to false in Disconnect() so that a true server restart sends the
	 * message again on the next connection.
	 */
	bool bServerOnlineMessageSent{false};

	/**
	 * Set to true when FCoreDelegates::OnHandleSystemError fires, indicating
	 * the server is crashing rather than shutting down gracefully.
	 * Used in Disconnect() to choose ServerCrashMessage over ServerOfflineMessage.
	 */
	bool bWasCrash{false};

	/**
	 * Set to true once the crash notification has been sent to Discord to prevent
	 * duplicate messages when both OnSystemError and Disconnect run (e.g. on
	 * platforms that perform UE subsystem cleanup after a crash).
	 */
	bool bCrashNotificationSent{false};

	/**
	 * Set to true when the EOS platform probe in InitializeServer() determines
	 * that UEOSIntegrationSubsystem::IsPlatformOperational() returns false
	 * (i.e. the CSS OnlineIntegrationSubsystem or its session manager is absent).
	 * When true the "status" command appends an EOS warning and the "eos" command
	 * reports the failure details.
	 *
	 * Note: EOS detection uses GConfig->GetString() (always available via
	 * CoreMinimal.h) and UOnlineIntegrationSubsystem (CSS-native plugin).
	 * The v1 IOnlineSubsystem::Get() API is NOT used here; it is commented out
	 * in the Alpakit template and must not be referenced in CSS UE mods.
	 *
	 * On a properly configured CSS UnrealEngine-CSS dedicated server
	 * (DefaultEngine.ini: DefaultPlatformService=EOS) this flag should remain
	 * false.  A true value indicates a missing or misconfigured EOS platform –
	 * ban-by-platform-ID will not function until the platform is fixed and the
	 * server is restarted.
	 *
	 * EOS state is owned by DiscordBridgeSubsystem directly using OnlineIntegration.
	 * All writes go through the IsEOSPlatformOperational() / IsEOSPlatformConfigured()
	 * helpers declared below.
	 */
	bool bEosPlatformConfirmedUnavailable{false};

	/**
	 * Set to true when EOS PUID resolution is definitively known to be
	 * unavailable for this server session.  Once set, subsequent player-join
	 * events will NOT schedule the 120-retry deferred EOS PUID timer,
	 * preventing repetitive Discord "PUID unresolved" spam on custom
	 * UnrealEngine-CSS builds where the EOS platform components are absent.
	 *
	 * This flag is ONLY about EOS Product User ID (PUID) resolution.
	 * Steam ID ban enforcement is NOT affected: Steam64 IDs are read directly
	 * from PS->GetUniqueId() in GetPlayerPlatformId() without going through
	 * the EOS path, so Steam bans work regardless of this flag's value.
	 *
	 * Set to true in InitializeServer() when:
	 *   – No recognised online platform is configured (neither EOS nor Steam), or
	 *   – The detected platform type is None (neither EOS nor Steam configured).
	 *
	 * Also set to true the first time the deferred timer times out while
	 * IsPlatformOperational()==false (the server ran 60 s of retries and the
	 * EOS platform never came up; further retries for subsequent players
	 * are pointless and would only flood Discord).
	 *
	 * NOT set when the platform is Steam or when EOS is operational: in those
	 * cases the timer is expected to succeed and should not be suppressed.
	 */
	bool bPlatformIdResolutionConfirmedUnavailable{false};

	/**
	 * Periodic ticker that polls IsEOSPlatformOperational() every 5 seconds after
	 * bPlatformIdResolutionConfirmedUnavailable has been set due to an EOS
	 * deferred-check timeout (IsEOSPlatformOperational()==false at timeout).
	 *
	 * When EOS becomes operational (session manager appears) the ticker clears
	 * bPlatformIdResolutionConfirmedUnavailable so subsequent players can have
	 * their EOS PUIDs resolved and ban-checked.  It also retroactively schedules
	 * EOS PUID followup checks (ScheduleEosPuidFollowupCheck) for all currently-
	 * connected players who joined while EOS was unavailable: Steam64-ID players
	 * are queued for EOS PUID upgrade detection, and any player whose EOS PUID is
	 * already resolvable at recovery time receives an immediate ban check.  The
	 * ticker stops itself once EOS is confirmed operational or after the maximum
	 * poll duration.
	 *
	 * Not started on servers where IsNoPlatformConfigured() returns true, because
	 * a server with no platform configured can never become EOS-operational.
	 */
	FTSTicker::FDelegateHandle EOSPlatformRecoveryPollHandle;

	/**
	 * Periodic ticker that scans all connected players against the ban list and
	 * kicks any who are found to be banned.  This is a safety-net for bans added
	 * directly to ServerBanlist.json without going through the bot commands (bot
	 * commands already kick the player immediately at the time the ban is issued).
	 *
	 * Started in InitializeServer() when BanScanIntervalSeconds > 0.
	 * Stopped in Deinitialize().
	 */
	FTSTicker::FDelegateHandle PeriodicBanScanHandle;

	/** Handle for the FCoreDelegates::OnHandleSystemError binding. */
	FDelegateHandle SystemErrorHandle;

	// ── GameplayEvents integration ────────────────────────────────────────────

	/**
	 * Dispatch a DiscordBridge GameplayEvent with no payload via
	 * UGameplayEventsSubsystem.  No-op when the subsystem is unavailable.
	 */
	void DispatchDiscordGameplayEvent(const FGameplayTag& Tag) const;

	/**
	 * Dispatch a DiscordBridge GameplayEvent with a string payload via
	 * UGameplayEventsSubsystem.  No-op when the subsystem is unavailable.
	 */
	void DispatchDiscordGameplayEvent(const FGameplayTag& Tag, const FString& Payload) const;

	/**
	 * Bound to UGameplayEventsSubsystem::OnGameplayEventTriggeredNative.
	 * Handles incoming DiscordBridge.Message.ToDiscord events dispatched by
	 * other mods and forwards the string payload to the main Discord channel(s).
	 */
	void OnToDiscordGameplayEvent(const FGameplayEvent& Event);

	/** Handle for the GameplayEvents ToDiscord subscription (removed in Deinitialize). */
	FDelegateHandle ToDiscordGameplayEventHandle;

	/** Discord user ID of the bot; used to filter out self-sent messages. */
	FString BotUserId;

	// ── Config hot-reload polling ─────────────────────────────────────────────

	/**
	 * Periodic ticker that polls the primary config file every 30 seconds when
	 * BotToken or ChannelId were absent at startup.  Stops automatically once
	 * credentials are detected and Connect() is called, allowing operators to
	 * configure the bridge without restarting the server.
	 * Never started when credentials are present at startup.
	 */
	FTSTicker::FDelegateHandle ConfigPollingHandle;

	/**
	 * Timer callback – checks the primary config file for BotToken/ChannelId.
	 * Performs a full config reload and calls Connect() when both are present.
	 * Returns true to keep polling, false when connected (stops the ticker).
	 */
	bool TickConfigPolling(float DeltaTime);

	// ── Chat manager binding ──────────────────────────────────────────────────

	/**
	 * Periodic ticker that attempts to bind to AFGChatManager::OnChatMessageAdded
	 * once per second until the chat manager is available.  Cleared on success.
	 */
	FTSTicker::FDelegateHandle ChatManagerBindTickHandle;

	/** Weak reference to the chat manager we bound to; used for cleanup. */
	TWeakObjectPtr<AFGChatManager> BoundChatManager;

	/**
	 * Snapshot of mReceivedMessages taken when OnGameChatMessageAdded last ran.
	 * Used to diff against the current message list and detect newly added entries.
	 */
	TArray<FChatMessageStruct> LastKnownMessages;

	// ── EOS / platform-ID helpers ─────────────────────────────────────────────

	/**
	 * Returns the platform ID string for the given player state, or an empty
	 * string when the ID cannot be safely retrieved.
	 *
	 * Resolution is split by ID type:
	 *
	 *   Steam players (UniqueId type != "EOS"):
	 *     Steam64 IDs are read directly from PS->GetUniqueId() without going
	 *     through the EOS path.  Steam IDs do not use EOS SDK handles
	 *     so the mIsOnline and GetNumLocalPlayers() guards do not apply.  This
	 *     means Steam ban enforcement works even when EOS is absent.
	 *
	 *   EOS players (UniqueId type == "EOS"):
	 *     Delegates to SafeGetEOSPlayerPlatformId(), which applies the mIsOnline
	 *     guard (via UE reflection) and the GetNumLocalPlayers() guard before
	 *     touching any EOS SDK function, preventing the SIMD/SIGSEGV crash
	 *     confirmed in CSS UE5.3 production logs.
	 *     Returns empty string when EOS is not yet ready.
	 */
	FString GetPlayerPlatformId(const APlayerState* PS) const;

	/**
	 * Attempts to resolve the player's platform account ID (Steam64 or EOS PUID)
	 * from the UOnlineIntegrationControllerComponent attached to the given
	 * PlayerController.
	 *
	 * The component receives the client's platform account ID via a reliable Server
	 * RPC (Server_RegisterControllerComponent) during the client's BeginPlay().
	 * This data typically arrives 1–2 network round-trips after PostLogin (<<1 s),
	 * which is MUCH sooner than the EOS SDK path when GetNumLocalPlayers()==0
	 * (the server EOS service-account auth delay, up to 60 s in production).
	 *
	 * The FAccountId is serialized to the X-FactoryGame-PlayerId hex format
	 * (BytesToHex([EOnlineServices_type_byte, ...account_bytes...])) and then
	 * passed through NormalizePlatformId() to produce a canonical ban-system ID:
	 *   "06" + 16 hex chars → decimal Steam64 ID (EOnlineServices::Steam = 6)
	 *   "01" + 32 hex chars → 32-char EOS PUID hex (EOnlineServices::Epic  = 1)
	 *
	 * This is a supplementary fallback.  The primary resolution path remains
	 * GetPlayerPlatformId() (EOS SDK via OnlineIntegration).  The component path
	 * enables Steam64-based bans to be enforced when EOS is still initialising or
	 * permanently unavailable, with no EOS SDK involvement.
	 *
	 * Returns empty when the component is absent or the Server RPC has not yet
	 * arrived.
	 *
	 * @param PC  The server-side PlayerController.  May be null.
	 * @return    Canonical ban-system ID string, or empty on failure.
	 */
	FString GetPlayerPlatformIdFromComponent(const APlayerController* PC) const;

	// ── Platform helpers (replaces UEOSIntegrationSubsystem) ─────────────────────
	/**
	 * Returns true when the EOS session manager is present at runtime (live check).
	 * Equivalent to UEOSIntegrationSubsystem::IsPlatformOperational() for EOS.
	 * For Steam servers always returns false (Steam has no EOS session manager).
	 */
	bool IsEOSPlatformOperational() const;

	/**
	 * Returns true when DefaultPlatformService=EOS (or EOS session manager is
	 * present at runtime via auto-detection).
	 */
	bool IsEOSPlatformConfigured() const;

	/**
	 * Returns true when DefaultPlatformService=Steam.
	 * NOTE: On CSS dedicated servers this is a misconfiguration; use EOS+NativeSteam.
	 */
	bool IsSteamPlatformConfigured() const;

	/**
	 * Returns true when no recognised online platform is configured (neither EOS
	 * nor Steam), OR when the platform service name is absent/NULL/None.
	 * When this returns true, EOS PUID resolution is unavailable.
	 */
	bool IsNoPlatformConfigured() const;

	/**
	 * Reads DefaultPlatformService from DefaultEngine.ini via GConfig and returns
	 * the raw string, e.g. "EOS", "Steam", "None", or empty.
	 */
	FString GetConfiguredPlatformServiceName() const;

	/**
	 * Returns the number of locally-authenticated online users on this server.
	 * Equivalent to UEOSIntegrationSubsystem::GetNumLocalEOSUsers().
	 * Uses UCommonUserSubsystem::GetNumLocalPlayers() from the OnlineIntegration plugin.
	 */
	int32 GetNumLocalOnlineUsers() const;

	/**
	 * Safely resolves an EOS Product User ID for the given player state.
	 * Applies the mIsOnline reflection guard and the GetNumLocalPlayers() guard
	 * required in CSS UE5.3 to prevent SIGSEGV when touching EOS SDK handles.
	 * Non-EOS IDs (Steam64 etc.) are returned directly without any guard.
	 * Returns empty string on any failure.
	 *
	 * This is the inline equivalent of UEOSIntegrationSubsystem::GetPlayerPlatformId().
	 */
	FString SafeGetEOSPlayerPlatformId(const APlayerState* PS) const;

	/**
	 * Builds a rich platform diagnostics string for the `!server eos` and
	 * `!serverinfo eos` admin commands.
	 * Equivalent to UEOSIntegrationSubsystem::GetPlatformDiagnostics().
	 *
	 * @param bPlainText  When true, strips Discord markdown (for in-game output).
	 */
	FString BuildPlatformDiagnostics(bool bPlainText = false) const;

	/**
	 * Bound to FGameModeEvents::GameModePostLoginEvent.
	 * Kicks any player that is not on the whitelist when the whitelist is enabled.
	 * Sends a player join notification to Discord for players that pass all checks.
	 */
	void OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller);

	/**
	 * Bound to FGameModeEvents::GameModeLogoutEvent.
	 * Sends a player leave notification to Discord whenever a tracked player
	 * disconnects, whether by a clean logout or a connection timeout.
	 */
	void OnLogout(AGameModeBase* GameMode, AController* Controller);

	/**
	 * Records the player in TrackedPlayerNames and posts a join notification to
	 * the bridged Discord channel(s).  Called from OnPostLogin once a player has
	 * passed all ban and whitelist checks (i.e. they are actually joining).
	 */
	void NotifyPlayerJoined(APlayerController* Controller, const FString& PlayerName);

	/**
	 * Fetch all guild members who hold WhitelistRoleId via the Discord REST API
	 * and populate RoleMemberIdToNames / WhitelistRoleMemberNames.
	 * Called from HandleReady whenever WhitelistRoleId is configured.
	 *
	 * @param RetryCount  Number of previous failed attempts.  When the HTTP
	 *                    request fails (e.g. due to a transient SSL/network
	 *                    error at server startup) the callback reschedules this
	 *                    function with an incremented count up to a maximum of
	 *                    3 retries, using an exponential back-off delay.
	 */
	void FetchWhitelistRoleMembers(int32 RetryCount = 0);

	/**
	 * Add or remove a single member's display names in the Discord role member
	 * cache based on whether they currently hold WhitelistRoleId.
	 * Called from GUILD_MEMBER_ADD, GUILD_MEMBER_UPDATE and GUILD_MEMBER_REMOVE
	 * Gateway events.
	 *
	 * @param MemberObj  The member object from the Gateway event payload.
	 * @param bRemoved   When true the member left the guild; purge them unconditionally.
	 */
	void UpdateWhitelistRoleMemberEntry(const TSharedPtr<FJsonObject>& MemberObj,
	                                    bool bRemoved = false);

	/**
	 * Rebuild WhitelistRoleMemberNames (the flat name-lookup set) from the
	 * authoritative RoleMemberIdToNames map.  Must be called after any mutation
	 * of RoleMemberIdToNames.
	 */
	void RebuildWhitelistRoleNameSet();

	/** Handle a server-information command received from Discord (e.g. !server players).
	 *  These commands are open to all channel members (no role gate).
	 *  @param SubCommand        Everything after the command prefix (trimmed).
	 *  @param ResponseChannelId The channel to send the response to. */
	void HandleServerInfoCommand(const FString& SubCommand,
	                             const FString& ResponseChannelId);

	/** Handle an admin server control command received from Discord (e.g. !admin stop).
	 *  Requires the sender to hold ServerControlCommandRoleId.
	 *  @param SubCommand        Everything after the command prefix (trimmed).
	 *  @param DiscordUsername   Display name of the user who issued the command.
	 *  @param ResponseChannelId The channel to send the response to. */
	void HandleServerControlCommand(const FString& SubCommand,
	                                const FString& DiscordUsername,
	                                const FString& ResponseChannelId);

	/** Handle a whitelist management command received from Discord.
	 *  @param ResponseChannelId  The channel to send the response to.
	 *                            When empty, falls back to the main ChannelId. */
	void HandleWhitelistCommand(const FString& SubCommand, const FString& DiscordUsername,
	                            const FString& ResponseChannelId);

	/** Handle a ban management command received from Discord.
	 *  @param ResponseChannelId  The channel to send the response to.
	 *                            When empty, falls back to the main ChannelId. */
	void HandleBanCommand(const FString& SubCommand, const FString& DiscordUsername,
	                      const FString& ResponseChannelId);

	/** Handle a kick command received from Discord (kicks without banning).
	 *  Requires the sender to hold KickCommandRoleId.
	 *  @param SubCommand        Everything after the command prefix (trimmed).
	 *  @param DiscordUsername   Display name of the user who issued the command.
	 *  @param ResponseChannelId The channel to send the response to. */
	void HandleKickCommand(const FString& SubCommand, const FString& DiscordUsername,
	                       const FString& ResponseChannelId);

	/**
	 * Handle an INTERACTION_CREATE gateway event (button clicks, select menus, etc.).
	 * Called from HandleDispatch when the event type is "INTERACTION_CREATE".
	 */
	void HandleInteractionCreate(const TSharedPtr<FJsonObject>& DataObj);

	/**
	 * Handle a MESSAGE_COMPONENT interaction (button click) for the ticket panel.
	 * Dispatches to the appropriate ticket-open or ticket-close logic.
	 *
	 * @param InteractionId    Discord interaction ID (used when responding).
	 * @param InteractionToken Discord interaction token (used when responding).
	 * @param CustomId         The button's custom_id value.
	 * @param DiscordUserId    Discord user ID of the member who clicked.
	 * @param DiscordUsername  Display name of the member who clicked.
	 * @param MemberRoles      List of Discord role IDs the member holds.
	 * @param SourceChannelId  The channel the interaction originated from.
	 */
	void HandleTicketButtonInteraction(const FString& InteractionId,
	                                   const FString& InteractionToken,
	                                   const FString& CustomId,
	                                   const FString& DiscordUserId,
	                                   const FString& DiscordUsername,
	                                   const TArray<FString>& MemberRoles,
	                                   const FString& SourceChannelId);

	/**
	 * Respond to a Discord interaction (button click) via the REST API.
	 * Must be called within 3 seconds of receiving the INTERACTION_CREATE event.
	 *
	 * @param InteractionId     The interaction's id field.
	 * @param InteractionToken  The interaction's token field.
	 * @param ResponseType      Discord interaction callback type:
	 *                            4 = CHANNEL_MESSAGE_WITH_SOURCE (send a message)
	 *                            6 = DEFERRED_UPDATE_MESSAGE    (ack silently)
	 *                            9 = MODAL                      (show a modal form)
	 * @param Content           Message text (used when ResponseType is 4).
	 * @param bEphemeral        When true the response is only visible to the clicker.
	 */
	void RespondToInteraction(const FString& InteractionId,
	                          const FString& InteractionToken,
	                          int32 ResponseType,
	                          const FString& Content,
	                          bool bEphemeral = true);

	/**
	 * Respond to a ticket button interaction by showing a Discord modal (popup
	 * form) that lets the user type a reason before the ticket channel is created.
	 * Must be called within 3 seconds of receiving the INTERACTION_CREATE event.
	 *
	 * @param InteractionId    Discord interaction ID.
	 * @param InteractionToken Discord interaction token.
	 * @param ModalCustomId    custom_id encoded in the modal (e.g. "ticket_modal:wl").
	 *                         This value is echoed back in the MODAL_SUBMIT interaction.
	 * @param ModalTitle       Title shown at the top of the modal (max 45 chars).
	 * @param PlaceholderText  Placeholder text for the reason text area (max 100 chars).
	 */
	void ShowTicketReasonModal(const FString& InteractionId,
	                           const FString& InteractionToken,
	                           const FString& ModalCustomId,
	                           const FString& ModalTitle,
	                           const FString& PlaceholderText);

	/**
	 * Handle a MODAL_SUBMIT interaction (type 5) submitted from a ticket reason
	 * modal shown by ShowTicketReasonModal.  Extracts the user-supplied reason,
	 * runs duplicate-ticket and enabled-check guards, then calls CreateTicketChannel.
	 *
	 * @param InteractionId    Discord interaction ID (used when responding).
	 * @param InteractionToken Discord interaction token (used when responding).
	 * @param ModalCustomId    The modal's custom_id (e.g. "ticket_modal:wl").
	 * @param ModalData        The full "data" JSON object from the MODAL_SUBMIT event.
	 * @param DiscordUserId    Discord user ID of the member who submitted the modal.
	 * @param DiscordUsername  Display name of the member who submitted the modal.
	 */
	void HandleTicketModalSubmit(const FString& InteractionId,
	                             const FString& InteractionToken,
	                             const FString& ModalCustomId,
	                             const TSharedPtr<FJsonObject>& ModalData,
	                             const FString& DiscordUserId,
	                             const FString& DiscordUsername);

	/**
	 * Create a private Discord text channel for a support ticket.
	 * The channel is visible only to the ticket opener and members holding
	 * TicketNotifyRoleId (the admin/support role).
	 * After the channel is created, a welcome message and a Close Ticket
	 * button are posted automatically.
	 *
	 * @param OpenerUserId    Discord user ID of the member who opened the ticket.
	 * @param OpenerUsername  Display name used in the channel name and messages.
	 * @param TicketType      One of "whitelist", "help", "report", or a custom slug.
	 * @param ExtraInfo       Optional extra text (reason / message body) provided
	 *                        by the opener.
	 * @param DisplayLabel    Human-readable label for custom ticket types (empty for built-ins).
	 * @param DisplayDesc     Description for custom ticket types shown in the welcome message.
	 */
	void CreateTicketChannel(const FString& OpenerUserId,
	                         const FString& OpenerUsername,
	                         const FString& TicketType,
	                         const FString& ExtraInfo,
	                         const FString& DisplayLabel = TEXT(""),
	                         const FString& DisplayDesc  = TEXT(""));

	/**
	 * Delete a Discord channel via the REST API (used to close ticket channels).
	 * Removes the entry from the active-ticket maps after a successful deletion.
	 *
	 * @param ChannelId  The Discord channel ID to delete.
	 */
	void DeleteDiscordChannel(const FString& ChannelId);

	/**
	 * Post the ticket panel message (a message with clickable buttons for each
	 * ticket type) to the configured TicketPanelChannelId (or ChannelId when
	 * TicketPanelChannelId is empty).
	 * Called by the `!admin ticket-panel` command.
	 *
	 * @param PanelChannelId   Channel where the panel is posted.
	 * @param ResponseChannelId Channel where the confirmation reply is sent.
	 */
	void PostTicketPanel(const FString& PanelChannelId, const FString& ResponseChannelId);

	/** Handle a whitelist management command typed in the Satisfactory in-game chat. */
	void HandleInGameWhitelistCommand(const FString& SubCommand);

	/** Handle a ban management command typed in the Satisfactory in-game chat. */
	void HandleInGameBanCommand(const FString& SubCommand);

	/** Handle a kick command typed in the Satisfactory in-game chat (kicks without banning). */
	void HandleInGameKickCommand(const FString& SubCommand);

	/** Handle a server-info command typed in the Satisfactory in-game chat. */
	void HandleInGameServerInfoCommand(const FString& SubCommand);

	/** Broadcast a status/feedback message to all connected players via the game chat. */
	void SendGameChatStatusMessage(const FString& Message);

	/**
	 * Scans connected players and kicks any whose name matches PlayerName
	 * (case-insensitive) using the configured BanKickReason.
	 * When PlayerName is empty all connected players that are on the ban list
	 * are kicked, which is useful when the ban system is toggled on at runtime.
	 * Returns the number of players kicked.
	 */
	int32 KickConnectedBannedPlayers(const FString& PlayerName = TEXT(""));

	/**
	 * Schedule an EOS PUID followup check for a player initially identified by a
	 * Steam64 ID on an EOS-configured server.
	 *
	 * On EOS servers (DefaultPlatformService=EOS) the UE FUniqueNetId handle may
	 * briefly present as a Steam-type ID before EOS processes the login and upgrades
	 * it to an EOS Product User ID (PUID).  This method polls GetPlayerPlatformId()
	 * every 0.5 s (up to 120 retries = 60 s) and, as soon as the handle changes from
	 * the captured Steam64 ID to a different value (the EOS PUID), runs a ban check
	 * against both identifiers and performs cross-platform ID linking.
	 *
	 * Called from three places:
	 *   1. OnPostLogin() immediately when PlatformId is already a Steam64 ID and
	 *      IsEOSPlatformConfigured() is true.
	 *   2. The deferred platform-ID check timer, when a Steam64 ID is resolved via
	 *      UOnlineIntegrationControllerComponent on an EOS server (the common case
	 *      where the component RPC arrives before the EOS SDK path is ready).
	 *   3. The EOS platform recovery poll, when EOS becomes operational after an
	 *      initial failure: all already-connected players with Steam64 IDs are
	 *      retroactively scheduled for EOS PUID followup checks.
	 *
	 * @param Controller       The joining player's controller. Validated inside the
	 *                         timer lambda; the call is a no-op if null/invalid.
	 * @param CapturedSteamId  The Steam64 ID that was resolved at scheduling time.
	 *                         Used to detect when EOS replaces it with an EOS PUID.
	 * @param PlayerName       Player display name at scheduling time (logging only).
	 */
	void ScheduleEosPuidFollowupCheck(APlayerController* Controller,
	                                  const FString&     CapturedSteamId,
	                                  const FString&     PlayerName);

	/**
	 * Schedule a cross-check to verify Steam64-based bans when an EOS PUID was
	 * resolved immediately at login on an EOS server.
	 *
	 * On EOS servers Steam players have both an EOS PUID (returned by
	 * GetPlayerPlatformId() when EOS is operational) and a Steam64 ID (returned
	 * by GetPlayerPlatformIdFromComponent() once the client Server RPC arrives,
	 * typically < 1 s after PostLogin).  When an admin bans a player by their
	 * Steam64 ID the ban would otherwise be silently skipped because only the
	 * EOS PUID is checked at PostLogin time (the component is not yet consulted).
	 *
	 * This method polls GetPlayerPlatformIdFromComponent() every 0.5 s (up to
	 * 20 retries = 10 s) and, as soon as the native Steam64 ID is available,
	 * checks it against the ban list and performs cross-platform ID linking so
	 * both identifiers (Steam64 and EOS PUID) are consistently banned.
	 *
	 * Called from OnPostLogin() when:
	 *   – The EOS PUID was resolved immediately (PlatformId is Epic-type).
	 *   – Steam64-based bans exist in the ban list.
	 *   – EOS is configured (DefaultPlatformService=EOS).
	 *
	 * @param Controller    The joining player's controller.
	 * @param CapturedPuid  The EOS PUID resolved at PostLogin time.
	 * @param PlayerName    Player display name at scheduling time (logging only).
	 */
	void ScheduleNativePlatformIdCrossCheck(APlayerController* Controller,
	                                        const FString&     CapturedPuid,
	                                        const FString&     PlayerName);

	/**
	 * Assign or revoke a Discord role from a guild member via the REST API.
	 * No-op when RoleId, GuildId, or BotToken is empty.
	 *
	 * @param UserId  Discord user ID of the target member.
	 * @param RoleId  Discord role ID to grant or revoke.
	 * @param bGrant  true = grant the role, false = revoke it.
	 */
	void ModifyDiscordRole(const FString& UserId, const FString& RoleId, bool bGrant);

	FDelegateHandle PostLoginHandle;

	/** Handle for FGameModeEvents::GameModeLogoutEvent. */
	FDelegateHandle LogoutHandle;

	/**
	 * Periodic ticker that polls the game net driver's client connections once
	 * per second to detect new incoming connections before login completes and
	 * connections that drop without completing login.  Used to post
	 * PlayerConnectingMessage and PlayerConnectionDroppedMessage to Discord.
	 * Started in InitializeServer() and stopped in Deinitialize().
	 */
	FTSTicker::FDelegateHandle NetConnectionMonitorHandle;

	/**
	 * Set of remote addresses (IP:port strings) for connections that have been
	 * accepted by the net driver but have not yet completed login (i.e. their
	 * UNetConnection::PlayerController is still null).
	 *
	 * Each entry is added the first time the monitor tick observes a connection
	 * without a PlayerController and removed either when the monitor no longer
	 * sees it (drop / failed login) or when OnPostLogin fires for that
	 * connection (successful login).  This prevents false "dropped" notifications
	 * for players who successfully logged in.
	 */
	TSet<FString> PendingConnectionAddresses;

	/** Timer callback – polls the net driver and logs connection events. */
	bool NetConnectionMonitorTick(float DeltaTime);

	/**
	 * Maps each player controller to the in-game name of the player who joined.
	 * Populated in NotifyPlayerJoined (only for players who pass all ban/whitelist
	 * checks) and consumed in OnLogout to send the leave notification.
	 * Using the raw pointer as a key is safe because OnLogout is called before
	 * the controller is destroyed.
	 */
	TMap<const APlayerController*, FString> TrackedPlayerNames;

	/**
	 * Maps each player controller to the platform display name (e.g. "Steam",
	 * "Epic Games Store") of the player.  Populated alongside TrackedPlayerNames
	 * in NotifyPlayerJoined() and consumed in OnLogout() to support the
	 * %Platform% placeholder in PlayerLeaveMessage.
	 */
	TMap<const APlayerController*, FString> TrackedPlayerPlatforms;

	/**
	 * Rate-limit map for Login-hook ban-reject Discord notifications.
	 * Key:   Lowercase platform ID (Steam64 or EOS PUID).
	 * Value: FPlatformTime::Seconds() at the time the last notification was sent.
	 * Entries are never pruned (the number of distinct banned IDs is small) so
	 * the map stays small for the life of the server session.
	 */
	TMap<FString, double> LastLoginRejectNotifyTimeByPlatformId;

	/** Deferred-init handle used on listen servers to wait for world net-mode. */
	FDelegateHandle PostWorldInitHandle;

	/** Discord ID of the guild (server) the bot is connected to.
	 *  Populated from the first entry in the READY event's guilds array.
	 *  Required for Discord REST role-management calls. */
	FString GuildId;

	/** Discord user ID of the guild owner.
	 *  Populated from the GUILD_CREATE event received after the READY handshake.
	 *  Guild owners bypass all role-based permission checks so they can always
	 *  use server control, whitelist, and ban commands even when no role ID is
	 *  configured. */
	FString GuildOwnerId;

	// ── Active ticket channel tracking ────────────────────────────────────────

	/**
	 * Maps each active ticket channel ID to the Discord user ID of the member
	 * who opened it.  Used to authorise close requests and to prevent
	 * duplicate tickets (one active ticket per user at a time).
	 */
	TMap<FString, FString> TicketChannelToOpener;

	/**
	 * Reverse of TicketChannelToOpener: maps each opener's Discord user ID to
	 * the active ticket channel they currently have open.
	 * Allows quick duplicate-ticket detection when a user clicks a panel button.
	 */
	TMap<FString, FString> OpenerToTicketChannel;

	// ── Discord role member cache (WhitelistRoleId) ───────────────────────────

	/**
	 * Authoritative store: maps each Discord user ID to the set of lowercased
	 * display names associated with that member (nick, global_name, username).
	 * Only members who currently hold WhitelistRoleId are present.
	 * Written from the game thread only (HTTP callbacks + Gateway event handlers).
	 */
	TMap<FString, TArray<FString>> RoleMemberIdToNames;

	/**
	 * Flat lookup set derived from RoleMemberIdToNames.
	 * Contains every lowercased display name of every member who holds
	 * WhitelistRoleId.  Rebuilt via RebuildWhitelistRoleNameSet() after
	 * any change to RoleMemberIdToNames.
	 * Checked in OnPostLogin as a secondary whitelist pass-through so that
	 * players whose in-game name matches a Discord role-member name are not
	 * kicked even if they are not listed in ServerWhitelist.json.
	 */
	TSet<FString> WhitelistRoleMemberNames;

	/**
	 * true while the FetchWhitelistRoleMembers HTTP request is in-flight.
	 * Set to true at the start of each fetch and cleared when the response
	 * arrives (success or failure).
	 *
	 * Used in OnPostLogin to defer whitelist kicks during the initial boot
	 * window: a player who holds the WhitelistRoleId would otherwise be
	 * kicked immediately after server start because the role-member cache
	 * (WhitelistRoleMemberNames) has not yet been populated.
	 */
	bool bWhitelistRoleFetchPending{false};

	/**
	 * File output device that captures LogDiscordBridge and LogSMLWebSocket
	 * messages and appends them to FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log.
	 * Registered with GLog in InitializeServer() and removed in Deinitialize().
	 */
	TUniquePtr<FDiscordBotFileLogger> FileLogger;
};
