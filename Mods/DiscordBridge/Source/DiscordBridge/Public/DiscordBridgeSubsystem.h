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

	/** Restrict this subsystem to dedicated servers only. */
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

private:
	// ── WebSocket event handlers (called on the game thread) ──────────────────

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

	/** Snowflake ID of the bot user; used to filter out self-sent messages. */
	FString BotUserId;

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

	// ── Whitelist enforcement ─────────────────────────────────────────────────

	/**
	 * Bound to FGameModeEvents::GameModePostLoginEvent.
	 * Kicks any player that is not on the whitelist when the whitelist is enabled.
	 */
	void OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller);

	/**
	 * Fetch all guild members who hold WhitelistRoleId via the Discord REST API
	 * and populate RoleMemberIdToNames / WhitelistRoleMemberNames.
	 * Called from HandleReady whenever WhitelistRoleId is configured.
	 */
	void FetchWhitelistRoleMembers();

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

	/** Handle a whitelist management command typed in the Satisfactory in-game chat. */
	void HandleInGameWhitelistCommand(const FString& SubCommand);

	/** Handle a ban management command typed in the Satisfactory in-game chat. */
	void HandleInGameBanCommand(const FString& SubCommand);

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
	 * Assign or revoke a Discord role from a guild member via the REST API.
	 * No-op when RoleId, GuildId, or BotToken is empty.
	 *
	 * @param UserId  Discord snowflake of the target member.
	 * @param RoleId  Discord snowflake of the role to grant or revoke.
	 * @param bGrant  true = grant the role, false = revoke it.
	 */
	void ModifyDiscordRole(const FString& UserId, const FString& RoleId, bool bGrant);

	FDelegateHandle PostLoginHandle;

	/** Snowflake ID of the guild (server) the bot is connected to.
	 *  Populated from the first entry in the READY event's guilds array.
	 *  Required for Discord REST role-management calls. */
	FString GuildId;

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
};
