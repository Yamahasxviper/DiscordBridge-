// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridgeSubsystem.h"
#include "DiscordBridge.h"
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
#include "FGPlayerState.h"
#include "Engine/World.h"
#include "Engine/NetDriver.h"
#include "Engine/NetConnection.h"
#include "Misc/OutputDeviceRedirector.h"
#include "UObject/UnrealType.h"
// CSS OnlineIntegration plugin – provides UOnlineIntegrationSubsystem (session/user
// manager), UCommonUserSubsystem (GetNumLocalPlayers for EOS guard), and
// UOnlineIntegrationControllerComponent (client platform account ID via Server RPC).
// All three are already reachable via the "OnlineIntegration" Build.cs dependency.
#include "OnlineIntegrationSubsystem.h"
#include "CommonUserSubsystem.h"
// GConfig->GetString() requires the full FConfigCacheIni definition.
#include "Misc/ConfigCacheIni.h"
// GameplayEvents plugin (Plugins/GameplayEvents) – dispatches loose-coupled events
// (player joined/left, Discord connected/disconnected, message from/to Discord) so
// that other mods can react without depending on UDiscordBridgeSubsystem directly.
#include "GameplayEventsSubsystem.h"
#include "GameplayEvent.h"
#include "GameplayTagContainer.h"
// ReliableMessaging plugin (Plugins/ReliableMessaging) – per-player message handler
// registered on login so client mods can forward UTF-8 text to Discord by sending on
// channel EDiscordRelayChannel::ForwardToDiscord via UReliableMessagingPlayerComponent.
#include "ReliableMessagingPlayerComponent.h"
// OnlineIntegration plugin (Plugins/Online/OnlineIntegration) – provides
// UOnlineIntegrationControllerComponent which receives the client's platform account ID
// (Steam64 or EOS PUID) via a reliable Server RPC during BeginPlay().  Used as a fast
// fallback for platform-ID resolution when the EOS SDK path is unavailable
// (e.g. mIsOnline=false or GetNumLocalPlayers()==0, which can delay the SDK path by
// up to 60 s).  The component path has no EOS SDK dependency and typically resolves
// within 1–2 network round-trips after PostLogin.
#include "User/OnlineIntegrationControllerComponent.h"
// FLocalUserNetIdBundle (OnlineIntegration/Public/LocalUserInfo.h) – provides the
// CSS-sanctioned SetAssociatedAccountId() helper that serialises UE::Online::FAccountId
// to the BytesToHex([type_byte, ...account_bytes]) format expected by
// NormalizePlatformId().  Using this API is preferred over calling
// FOnlineIdRegistryRegistry::Get().ToReplicationData() directly because:
//   • SetAssociatedAccountId() is the officially supported serialisation path in the
//     CSS OnlineIntegration plugin and matches how FGPlayerState stores the ID.
//   • The ONLINESERVICESINTERFACE_API symbols (Get/ToReplicationData) are satisfied by
//     the "OnlineServicesInterface" Build.cs dependency; this include makes the
//     dependency explicit and keeps the call site aligned with the CSS public API.
#include "LocalUserInfo.h"

// Discord Gateway endpoint (v10, JSON encoding)
static const FString DiscordGatewayUrl = TEXT("wss://gateway.discord.gg/?v=10&encoding=json");
// Discord REST API base URL
static const FString DiscordApiBase    = TEXT("https://discord.com/api/v10");

// ── Gameplay tags ─────────────────────────────────────────────────────────────
// All tags are declared in Mods/DiscordBridge/Config/Tags/DiscordBridgeGameplayTags.ini.
// Resolved at first use via FGameplayTag::RequestGameplayTag (returns a valid tag
// or an empty tag when the tag manager has not loaded the INI yet).
namespace FDiscordGameplayTags
{
	const FGameplayTag Connected()    { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Connected"),    false); }
	const FGameplayTag Disconnected() { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Disconnected"), false); }
	const FGameplayTag PlayerJoined() { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Player.Joined"), false); }
	const FGameplayTag PlayerLeft()   { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Player.Left"),   false); }
	const FGameplayTag FromDiscord()  { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Message.FromDiscord"), false); }
	const FGameplayTag ToDiscord()    { return FGameplayTag::RequestGameplayTag(TEXT("DiscordBridge.Message.ToDiscord"),   false); }
}

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	// This mod is server-only: RequiredOnRemote=false in DiscordBridge.uplugin
	// means SMM/SML will never require connecting players to download it.
	//
	// The subsystem is created on all non-editor builds (dedicated servers AND
	// potential listen-server hosts) because we cannot distinguish a listen
	// server from a pure client at UGameInstanceSubsystem creation time — the
	// world does not exist yet.  InitializeServer() is only ever called once
	// the world's net mode is confirmed to be NM_DedicatedServer or
	// NM_ListenServer (see Initialize() below), so pure clients that happen to
	// have this mod installed will have an idle, no-op subsystem instance.
	return !GIsEditor;
}

void UDiscordBridgeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	if (IsRunningDedicatedServer())
	{
		// Net mode is known immediately on a dedicated server.
		InitializeServer();
	}
	else
	{
		// On a listen server the net mode is only resolved once the world has
		// been created.  Register a one-shot delegate that fires after world
		// initialization so we can check the actual net mode before starting.
		//
		// Defensive: also handle NM_DedicatedServer here in case
		// IsRunningDedicatedServer() returned false unexpectedly (e.g. a CSS
		// server started without the -server command-line flag, or a hosting
		// wrapper that omits it).  Without this guard, InitializeServer() would
		// never be called on such a server and bPlatformIdResolutionConfirmedUnavailable
		// would be set to true immediately in InitializeServer() (because
		// IsNoPlatformConfigured() returns true when no platform probe has run yet),
		// permanently suppressing per-join ID retry timers and preventing all
		// platform-ID ban enforcement.
		PostWorldInitHandle = FWorldDelegates::OnPostWorldInitialization.AddWeakLambda(
			this, [this](UWorld* World, const UWorld::InitializationValues)
			{
				if (!World) { return; }
				const ENetMode NetMode = World->GetNetMode();
				if (NetMode == NM_ListenServer || NetMode == NM_DedicatedServer)
				{
					FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
					PostWorldInitHandle.Reset();
					InitializeServer();
				}
			});
	}
}

void UDiscordBridgeSubsystem::InitializeServer()
{
	// Create the dedicated log file at FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log
	// and register it with the engine's log system so all LogDiscordBridge and
	// LogSMLWebSocket messages are captured there automatically.
	FileLogger = MakeUnique<FDiscordBotFileLogger>();
	GLog->AddOutputDevice(FileLogger.Get());
	// Log the path so operators who have any form of server log access (console,
	// hosting-panel stdout, etc.) can quickly locate the file.  The message is
	// also captured by the file logger itself so the path is self-documenting.
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Mod log file location: %s"), *FileLogger->GetLogFilePath());

	// ── Platform state (using OnlineIntegration directly) ──────────────────────
	// Read the initial platform operational state using the helpers defined in
	// this class.  These use UOnlineIntegrationSubsystem and GConfig directly,
	// removing the dependency on the separate EOSIntegration mod.
	bEosPlatformConfirmedUnavailable = !IsEOSPlatformOperational();
	const FString PlatformServiceName = GetConfiguredPlatformServiceName();
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: platform service='%s', EOS operational=%s.  "
	            "Use '!server eos' for full diagnostics."),
	       *PlatformServiceName,
	       IsEOSPlatformOperational() ? TEXT("true") : TEXT("false"));

	if (IsNoPlatformConfigured())
	{
		bPlatformIdResolutionConfirmedUnavailable = true;
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: No online platform configured (DefaultPlatformService='%s'). "
		            "EOS PUID ban enforcement is not available; "
		            "Steam64 bans are still enforced via direct ID resolution. "
		            "EOS PUID deferred-retry timer suppressed. "
		            "Set [OnlineSubsystem] DefaultPlatformService=EOS in DefaultEngine.ini "
		            "to enable EOS PUID bans."),
		       *PlatformServiceName);
	}

	// Subscribe to PostLogin to enforce the whitelist and ban list on each player join.
	PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddUObject(
		this, &UDiscordBridgeSubsystem::OnPostLogin);

	// Subscribe to Logout to send Discord leave notifications for tracked players.
	LogoutHandle = FGameModeEvents::GameModeLogoutEvent.AddUObject(
		this, &UDiscordBridgeSubsystem::OnLogout);

	// Subscribe to the engine's system-error delegate so we can send a crash
	// notification to Discord before the crash reporter terminates the process.
	SystemErrorHandle = FCoreDelegates::OnHandleSystemError.AddUObject(
		this, &UDiscordBridgeSubsystem::OnSystemError);

	// Subscribe to DiscordBridge.Message.ToDiscord GameplayEvents so that other
	// mods and Blueprint code can post messages to Discord without depending on
	// UDiscordBridgeSubsystem directly.  Dispatch the event from any system with:
	//   UGameplayEventsSubsystem::DispatchGameplayEvent(
	//       FGameplayEvent(FDiscordGameplayTags::ToDiscord(), {}, TEXT("your message")));
	if (UGameplayEventsSubsystem* GES = GetGameInstance()
	        ? GetGameInstance()->GetSubsystem<UGameplayEventsSubsystem>()
	        : nullptr)
	{
		ToDiscordGameplayEventHandle = GES->AddOnGameplayEventDelegate(
			FOnGameplayEventTriggered::FDelegate::CreateUObject(
				this, &UDiscordBridgeSubsystem::OnToDiscordGameplayEvent));
	}

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

	// Start a 2-second periodic ticker that monitors the game net driver's
	// client connections.  It detects new connections before login completes
	// (and posts PlayerConnectingMessage) and connections that drop without
	// completing login (and posts PlayerConnectionDroppedMessage).
	NetConnectionMonitorHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) -> bool
		{
			return NetConnectionMonitorTick(DeltaTime);
		}),
		2.0f);

	// Load (or auto-create) the JSON config file from Configs/DiscordBridge.cfg.
	Config = FDiscordBridgeConfig::LoadOrCreate();

	// Load (or create) the whitelist JSON from disk, using the config value as
	// the default only on the very first server start (when no JSON file exists).
	// After the first start the enabled/disabled state is saved in the JSON and
	// survives restarts, so runtime !whitelist on / !whitelist off changes persist.
	// To force-reset to this config value: delete ServerWhitelist.json and restart.
	FWhitelistManager::Load(Config.bWhitelistEnabled);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Whitelist active = %s (WhitelistEnabled config = %s)"),
	       FWhitelistManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));

	// Load the ban list AFTER the config so we can pass BanSystemEnabled as the
	// authoritative enabled state.  The ban list (players) is read from
	// ServerBanlist.json, but the enabled/disabled state always comes from the
	// INI config so operators can toggle it by editing the file and restarting.
	FBanManager::Load(Config.bBanSystemEnabled);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: BanSystem active = %s (BanSystemEnabled config = %s)"),
	       FBanManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bBanSystemEnabled ? TEXT("True") : TEXT("False"));

	// ── Periodic ban scan ─────────────────────────────────────────────────────
	// Scans all connected players against the ban list at a configurable interval
	// and kicks anyone who is found to be banned.  This acts as a safety net when
	// bans are added directly to ServerBanlist.json (outside the bot commands).
	// Bot commands (e.g. !ban add / !ban id add) already kick the matching player
	// immediately, so the periodic scan only catches out-of-band edits.
	// Interval must be at least 30 s; 0 disables the scan.
	if (Config.BanScanIntervalSeconds > 0.0f)
	{
		const float ScanInterval = FMath::Max(Config.BanScanIntervalSeconds, 30.0f);
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Periodic ban scan enabled (interval=%.0f s)."),
		       ScanInterval);
		PeriodicBanScanHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				if (!IsValid(this))
				{
					return false; // Subsystem destroyed; stop ticking.
				}
				if (FBanManager::IsEnabled())
				{
					const int32 Kicked = KickConnectedBannedPlayers();
					if (Kicked > 0)
					{
						UE_LOG(LogDiscordBridge, Warning,
						       TEXT("DiscordBridge BanSystem: periodic scan kicked %d "
						            "connected banned player(s)."),
						       Kicked);
					}
				}
				return true; // Keep ticking.
			}),
			ScanInterval);
	}
	else
	{
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Periodic ban scan disabled (BanScanIntervalSeconds=0)."));
	}

	if (Config.BotToken.IsEmpty() || Config.ChannelId.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: BotToken or ChannelId is not configured. "
		            "Edit Mods/DiscordBridge/Config/DefaultDiscordBridge.ini to enable the bridge. "
		            "The bridge will connect automatically once credentials are saved (no restart required)."));

		// Poll the config file every 30 seconds so the bridge starts automatically
		// when BotToken and ChannelId are added, without requiring a server restart.
		ConfigPollingHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float DeltaTime) -> bool
			{
				return TickConfigPolling(DeltaTime);
			}),
			30.0f);
		return;
	}

	// Log active format strings so operators can verify they were loaded correctly.
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: ServerName           = \"%s\""), *Config.ServerName);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: GameToDiscordFormat  = \"%s\""), *Config.GameToDiscordFormat);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: DiscordToGameFormat  = \"%s\""), *Config.DiscordToGameFormat);

	Connect();
}

void UDiscordBridgeSubsystem::Deinitialize()
{
	// Cancel deferred listen-server init if it hasn't fired yet.
	if (PostWorldInitHandle.IsValid())
	{
		FWorldDelegates::OnPostWorldInitialization.Remove(PostWorldInitHandle);
		PostWorldInitHandle.Reset();
	}

	// Unbind the system-error crash-detection delegate.
	FCoreDelegates::OnHandleSystemError.Remove(SystemErrorHandle);
	SystemErrorHandle.Reset();

	// Stop the chat-manager bind ticker if it is still running.
	FTSTicker::GetCoreTicker().RemoveTicker(ChatManagerBindTickHandle);
	ChatManagerBindTickHandle.Reset();

	// Stop the config polling ticker if it is still running.
	FTSTicker::GetCoreTicker().RemoveTicker(ConfigPollingHandle);
	ConfigPollingHandle.Reset();

	// Stop the net connection monitor ticker and clear pending connection tracking.
	FTSTicker::GetCoreTicker().RemoveTicker(NetConnectionMonitorHandle);
	NetConnectionMonitorHandle.Reset();
	PendingConnectionAddresses.Empty();

	// Stop the EOS platform recovery polling ticker if it is running.
	FTSTicker::GetCoreTicker().RemoveTicker(EOSPlatformRecoveryPollHandle);
	EOSPlatformRecoveryPollHandle.Reset();

	// Stop the periodic ban scan ticker if it is running.
	FTSTicker::GetCoreTicker().RemoveTicker(PeriodicBanScanHandle);
	PeriodicBanScanHandle.Reset();

	// Unsubscribe from the GameplayEvents ToDiscord subscription.
	if (ToDiscordGameplayEventHandle.IsValid())
	{
		if (UGameplayEventsSubsystem* GES =
		        GetGameInstance()
		            ? GetGameInstance()->GetSubsystem<UGameplayEventsSubsystem>()
		            : nullptr)
		{
			GES->RemoveOnGameplayEventDelegate(ToDiscordGameplayEventHandle);
		}
		ToDiscordGameplayEventHandle.Reset();
	}

	// Remove the whitelist PostLogin listener.
	FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
	PostLoginHandle.Reset();

	// Remove the logout listener and clear the player tracking maps.
	FGameModeEvents::GameModeLogoutEvent.Remove(LogoutHandle);
	LogoutHandle.Reset();
	TrackedPlayerNames.Empty();
	TrackedPlayerPlatforms.Empty();

	// Unbind from the chat manager's delegate so no stale callbacks fire
	// after this subsystem is destroyed.
	if (BoundChatManager.IsValid())
	{
		BoundChatManager->OnChatMessageAdded.RemoveDynamic(
			this, &UDiscordBridgeSubsystem::OnGameChatMessageAdded);
		BoundChatManager.Reset();
	}

	Disconnect();

	// Unregister and shut down the file logger last so that the Disconnect()
	// call above (which may log the server-offline message) is still captured.
	if (FileLogger)
	{
		GLog->RemoveOutputDevice(FileLogger.Get());
		FileLogger.Reset();
	}

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Crash detection
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::OnSystemError()
{
	// Mark the shutdown as a crash so Disconnect() sends the crash message
	// instead of the normal offline message (handles platforms that call
	// Deinitialize during crash cleanup).
	bWasCrash = true;

	// Best-effort: attempt to dispatch the crash notification immediately.
	// The HTTP request fires on a background thread and may complete before
	// the crash reporter terminates the process, but delivery is not guaranteed.
	if (Config.bServerStatusMessagesEnabled && !Config.ServerCrashMessage.IsEmpty()
	    && !Config.BotToken.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Error,
		       TEXT("DiscordBridge: Server crash detected – attempting to send crash "
		            "notification to Discord (best-effort)."));
		SendStatusMessageToDiscord(Config.ServerCrashMessage);
		bCrashNotificationSent = true;
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Net connection monitor
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::NetConnectionMonitorTick(float /*DeltaTime*/)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return true; // Keep ticking; world may not exist yet.
	}

	UNetDriver* NetDriver = World->GetNetDriver();
	if (!NetDriver)
	{
		// No net driver yet (early in startup or listen server before a client
		// connects for the first time); clear stale tracking and keep ticking.
		PendingConnectionAddresses.Empty();
		return true;
	}

	// Build the set of remote addresses currently seen in a pending (pre-login)
	// state, i.e. connections whose PlayerController has not yet been assigned.
	TSet<FString> CurrentPending;
	for (UNetConnection* Conn : NetDriver->ClientConnections)
	{
		if (!Conn)
		{
			continue;
		}

		// PlayerController is set once the player has completed login.
		// While it is null the connection is still in the handshake / login phase.
		if (Conn->PlayerController)
		{
			continue; // Login already completed for this connection.
		}

		// Skip connections that are no longer open (closed or invalid state).
		// Closed connections may still linger in ClientConnections for a brief
		// window before the net driver removes them; calling LowLevelGetRemoteAddress
		// on such a connection can crash on certain platform implementations.
		// NOTE: UNetConnection::State is private in UE 5.3.2 (including the CSS
		// fork, 5.3.2-CSS). Use GetConnectionState() – the public accessor – instead
		// of accessing the State member directly (which produces C2248).
		if (Conn->GetConnectionState() == USOCK_Closed || Conn->GetConnectionState() == USOCK_Invalid)
		{
			continue;
		}

		// LowLevelGetRemoteAddress(true) returns "IP:Port" which is unique per
		// connection even when multiple clients share the same IP.
		const FString RemoteAddr = Conn->LowLevelGetRemoteAddress(true);
		if (RemoteAddr.IsEmpty())
		{
			continue;
		}

		CurrentPending.Add(RemoteAddr);

		if (!PendingConnectionAddresses.Contains(RemoteAddr))
		{
			// First time this connection has been seen – log a connecting event.
			PendingConnectionAddresses.Add(RemoteAddr);
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: New pre-login connection from %s."),
			       *RemoteAddr);

			if (!Config.PlayerConnectingMessage.IsEmpty())
			{
				FString Notice = Config.PlayerConnectingMessage;
				Notice = Notice.Replace(TEXT("%RemoteAddr%"), *RemoteAddr);
				for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
				{
					SendMessageToChannel(ChanId, Notice);
				}
			}
		}
	}

	// Any address previously in PendingConnectionAddresses that is no longer in
	// the current pending set has either completed login (its PlayerController
	// was set so we skipped it above) or was dropped before login finished.
	// We remove completed logins from PendingConnectionAddresses inside
	// OnPostLogin so they are not mistakenly reported as dropped here.
	TArray<FString> Dropped;
	for (const FString& Addr : PendingConnectionAddresses)
	{
		if (!CurrentPending.Contains(Addr))
		{
			Dropped.Add(Addr);
		}
	}

	for (const FString& Addr : Dropped)
	{
		PendingConnectionAddresses.Remove(Addr);
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Pre-login connection from %s dropped before completing login."),
		       *Addr);

		if (!Config.PlayerConnectionDroppedMessage.IsEmpty())
		{
			FString Notice = Config.PlayerConnectionDroppedMessage;
			Notice = Notice.Replace(TEXT("%RemoteAddr%"), *Addr);
			for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
			{
				SendMessageToChannel(ChanId, Notice);
			}
		}
	}

	return true; // Keep ticking.
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

	UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Connecting to Discord Gateway…"));
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

	// Signal offline status and post the server-offline (or crash) notification
	// while the WebSocket is still open so Discord receives it before we close.
	if (bGatewayReady)
	{
		// Setting presence to "invisible" makes the bot appear offline to
		// users immediately, without waiting for Discord to detect the
		// WebSocket disconnection.
		SendUpdatePresence(TEXT("invisible"));

		if (bWasCrash)
		{
			// Send crash notification if the OnSystemError handler hasn't
			// already done so (the handler fires first on platforms that call
			// Deinitialize during crash cleanup).
			if (!bCrashNotificationSent && !Config.ServerCrashMessage.IsEmpty())
			{
				SendStatusMessageToDiscord(Config.ServerCrashMessage);
				bCrashNotificationSent = true;
			}
		}
		else if (!Config.ServerOfflineMessage.IsEmpty())
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
	UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: WebSocket connection established. Awaiting Hello…"));
	// Discord will send op=10 (Hello) next; we send Identify in response.
}

void UDiscordBridgeSubsystem::OnWebSocketMessage(const FString& RawJson)
{
	HandleGatewayPayload(RawJson);
}

void UDiscordBridgeSubsystem::OnWebSocketClosed(int32 StatusCode, const FString& Reason)
{
	UE_LOG(LogDiscordBridge, Warning,
	       TEXT("DiscordBridge: Gateway connection closed (code=%d, reason='%s')."),
	       StatusCode, *Reason);

	// Detect Discord-specific close codes that indicate a permanent error.
	// For these codes reconnecting with the same credentials will never succeed,
	// so we signal the WebSocket client to stop auto-reconnecting.
	bool bTerminal = false;
	switch (StatusCode)
	{
	case 4004:
		UE_LOG(LogDiscordBridge, Error,
		       TEXT("DiscordBridge: Authentication failed (4004). "
		            "Verify BotToken in Mods/DiscordBridge/Config/DefaultDiscordBridge.ini. "
		            "Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4010:
		UE_LOG(LogDiscordBridge, Error, TEXT("DiscordBridge: Invalid shard sent (4010). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4011:
		UE_LOG(LogDiscordBridge, Error, TEXT("DiscordBridge: Sharding required (4011). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4012:
		UE_LOG(LogDiscordBridge, Error, TEXT("DiscordBridge: Invalid Gateway API version (4012). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4013:
		UE_LOG(LogDiscordBridge, Error, TEXT("DiscordBridge: Invalid intent(s) (4013). Auto-reconnect disabled."));
		bTerminal = true;
		break;
	case 4014:
		UE_LOG(LogDiscordBridge, Error,
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
		const FString DisconnectReason = FString::Printf(TEXT("Connection closed (code %d): %s"), StatusCode, *Reason);
		OnDiscordDisconnected.Broadcast(DisconnectReason);
		DispatchDiscordGameplayEvent(FDiscordGameplayTags::Disconnected(), DisconnectReason);
	}
}

void UDiscordBridgeSubsystem::OnWebSocketError(const FString& ErrorMessage)
{
	UE_LOG(LogDiscordBridge, Error, TEXT("DiscordBridge: WebSocket error: %s"), *ErrorMessage);

	FTSTicker::GetCoreTicker().RemoveTicker(HeartbeatTickerHandle);
	HeartbeatTickerHandle.Reset();
	bPendingHeartbeatAck = false;

	if (bGatewayReady)
	{
		bGatewayReady = false;
		const FString ErrorReason = FString::Printf(TEXT("WebSocket error: %s"), *ErrorMessage);
		OnDiscordDisconnected.Broadcast(ErrorReason);
		DispatchDiscordGameplayEvent(FDiscordGameplayTags::Disconnected(), ErrorReason);
	}
}

void UDiscordBridgeSubsystem::OnWebSocketReconnecting(int32 AttemptNumber, float DelaySeconds)
{
	UE_LOG(LogDiscordBridge, Log,
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
		UE_LOG(LogDiscordBridge, Warning, TEXT("DiscordBridge: Failed to parse Gateway JSON: %s"), *RawJson);
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
		UE_LOG(LogDiscordBridge, VeryVerbose,
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

	UE_LOG(LogDiscordBridge, Log,
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
	else if (EventType == TEXT("INTERACTION_CREATE"))
	{
		HandleInteractionCreate(DataObj);
	}
	else if (EventType == TEXT("GUILD_CREATE"))
	{
		// Cache the guild owner ID so the owner can bypass role checks.
		// Discord sends GUILD_CREATE for each guild the bot belongs to
		// shortly after the READY handshake.
		FString OwnerId;
		if (DataObj->TryGetStringField(TEXT("owner_id"), OwnerId) && !OwnerId.IsEmpty())
		{
			GuildOwnerId = OwnerId;
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Guild owner ID cached: %s"), *GuildOwnerId);
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
	UE_LOG(LogDiscordBridge, VeryVerbose, TEXT("DiscordBridge: Heartbeat acknowledged."));
	bPendingHeartbeatAck = false;
}

void UDiscordBridgeSubsystem::HandleReconnect()
{
	UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Server requested reconnect."));

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
	UE_LOG(LogDiscordBridge, Warning,
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

	UE_LOG(LogDiscordBridge, Log,
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
	DispatchDiscordGameplayEvent(FDiscordGameplayTags::Connected());
}

void UDiscordBridgeSubsystem::HandleMessageCreate(const TSharedPtr<FJsonObject>& DataObj)
{
	// Accept messages from the main channel(s) OR the dedicated whitelist/ban channel(s).
	FString MsgChannelId;
	if (!DataObj->TryGetStringField(TEXT("channel_id"), MsgChannelId))
	{
		return;
	}

	const TArray<FString> MainChannelIds      = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
	const TArray<FString> WhitelistChannelIds = FDiscordBridgeConfig::ParseChannelIds(Config.WhitelistChannelId);
	const TArray<FString> BanChannelIds       = FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

	const bool bIsMainChannel      = MainChannelIds.Contains(MsgChannelId);
	const bool bIsWhitelistChannel = WhitelistChannelIds.Contains(MsgChannelId);
	const bool bIsBanChannel       = BanChannelIds.Contains(MsgChannelId);

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
		UE_LOG(LogDiscordBridge, Warning,
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
			UE_LOG(LogDiscordBridge, Log,
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

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Discord message received from '%s' (channel %s): %s"),
	       *Username, *MsgChannelId, *Content);

	// Helper: returns true if the Discord member holds the given required role.
	// When RequiredRoleId is empty the command is disabled entirely – nobody
	// can run it until a role ID is configured.  Note: WhitelistCommandRoleId
	// does NOT grant game-join bypass; only WhitelistRoleId holders receive that
	// via the role-member cache checked in OnPostLogin.
	// Exception: the guild owner always passes this check regardless of role
	// configuration, so they can always administrate their own server.
	auto HasRequiredRole = [&](const FString& RequiredRoleId) -> bool
	{
		// Guild owner bypasses every role restriction unconditionally.
		if (!GuildOwnerId.IsEmpty() && AuthorId == GuildOwnerId)
		{
			return true;
		}
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
			UE_LOG(LogDiscordBridge, Log,
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
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Ignoring ban command from '%s' – BanCommandsEnabled=False."),
			       *Username);
			return;
		}
		if (!HasRequiredRole(Config.BanCommandRoleId))
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Ignoring ban command from '%s' – sender lacks BanCommandRoleId."),
			       *Username);
			// Reply in the channel where the command was typed.
			SendMessageToChannel(MsgChannelId,
			                     TEXT(":no_entry: You do not have permission to use ban commands."));
			return;
		}
		// Extract everything after the prefix as the sub-command (trimmed).
		const FString SubCommand = Content.Mid(Config.BanCommandPrefix.Len()).TrimStartAndEnd();
		// Route the response back to the channel where the command was issued.
		HandleBanCommand(SubCommand, Username, MsgChannelId);
		return; // Do not relay ban commands to in-game chat.
	}

	// Check whether this message is a kick command (kicks without banning).
	if (!Config.KickCommandPrefix.IsEmpty() &&
	    Content.StartsWith(Config.KickCommandPrefix, ESearchCase::IgnoreCase))
	{
		if (!Config.bKickCommandsEnabled)
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Ignoring kick command from '%s' – KickCommandsEnabled=False."),
			       *Username);
			return;
		}
		if (!HasRequiredRole(Config.KickCommandRoleId))
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Ignoring kick command from '%s' – sender lacks KickCommandRoleId."),
			       *Username);
			SendMessageToChannel(MsgChannelId,
			                     TEXT(":no_entry: You do not have permission to use kick commands."));
			return;
		}
		const FString SubCommand = Content.Mid(Config.KickCommandPrefix.Len()).TrimStartAndEnd();
		HandleKickCommand(SubCommand, Username, MsgChannelId);
		return; // Do not relay kick commands to in-game chat.
	}

	// Check whether this message is a server-info command (open to all channel members).
	if (!Config.ServerInfoCommandPrefix.IsEmpty() &&
	    Content.StartsWith(Config.ServerInfoCommandPrefix, ESearchCase::IgnoreCase))
	{
		// Only respond to server-info commands from the main channel (not the
		// admin-only ban channel or the whitelist-only channel).
		if (bIsMainChannel)
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Server-info command from '%s': '%s'"),
			       *Username, *Content);
			const FString SubCommand = Content.Mid(Config.ServerInfoCommandPrefix.Len()).TrimStartAndEnd();
			HandleServerInfoCommand(SubCommand, MsgChannelId);
		}
		return; // Do not relay server-info commands to in-game chat.
	}

	// Check whether this message is an admin server control command.
	if (!Config.ServerControlCommandPrefix.IsEmpty() &&
	    Content.StartsWith(Config.ServerControlCommandPrefix, ESearchCase::IgnoreCase))
	{
		const FString SubCommand = Content.Mid(Config.ServerControlCommandPrefix.Len()).TrimStartAndEnd();

		// The "ticket-panel" sub-command is a ticket management action and is
		// therefore authorised by the ticket notify role (TicketNotifyRoleId,
		// configured in DefaultTickets.ini) in addition to the server control
		// role.  This lets ticket admins post the panel without also needing
		// server stop/restart access.  All other sub-commands (start, stop,
		// restart) still require ServerControlCommandRoleId.
		const bool bIsTicketPanel = SubCommand.Equals(TEXT("ticket-panel"), ESearchCase::IgnoreCase);
		const bool bAuthorised = bIsTicketPanel
			? (HasRequiredRole(Config.TicketNotifyRoleId) || HasRequiredRole(Config.ServerControlCommandRoleId))
			: HasRequiredRole(Config.ServerControlCommandRoleId);

		if (!bAuthorised)
		{
			if (bIsTicketPanel)
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Ignoring ticket-panel command from '%s' – sender lacks TicketNotifyRoleId and ServerControlCommandRoleId."),
				       *Username);
				SendMessageToChannel(MsgChannelId,
				                     TEXT(":no_entry: You do not have permission to post the ticket panel. "
				                          "Set **TicketNotifyRoleId** in DefaultTickets.ini to your admin/support role."));
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: Ignoring server control command from '%s' – sender lacks ServerControlCommandRoleId."),
				       *Username);
				SendMessageToChannel(MsgChannelId,
				                     TEXT(":no_entry: You do not have permission to use server control commands. "
				                          "Set **ServerControlCommandRoleId** in DefaultDiscordBridge.ini to your admin role."));
			}
			return;
		}
		HandleServerControlCommand(SubCommand, Username, MsgChannelId);
		return; // Do not relay server control commands to in-game chat.
	}

	// Messages from the dedicated ban channel that are not ban commands are
	// silently ignored – the ban channel is admin-only and not bridged to game chat.
	if (bIsBanChannel)
	{
		return;
	}

	// SML chat commands are prefaced with '/' and must be typed directly in the
	// Satisfactory game-chat window.  They cannot be executed by relaying a
	// Discord message because SML only processes commands typed by a real player
	// controller in the game client.  Inform the sender rather than silently
	// dropping the message or injecting it into the game chat as plain text.
	//
	// NOTE: this check runs AFTER all bot-command prefix checks above, so it
	// never intercepts !whitelist, !ban, !server, !admin, or any other
	// configured bot-command prefix — including prefixes that themselves start
	// with '/'.  Only plain Discord chat messages that reach this point and
	// happen to start with '/' are caught here.
	if (Content.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Ignoring '/' message from '%s' – SML chat commands cannot be sent from Discord."),
		       *Username);
		SendMessageToChannel(MsgChannelId,
		    TEXT(":information_source: Chat commands starting with `/` are **SML chat commands**. "
		         "They must be typed directly in the Satisfactory in-game chat window and cannot be sent from Discord.\n"
		         "See: <https://docs.ficsit.app/satisfactory-modding/latest/SMLChatCommands.html>"));
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
	// platform.  Use the actual compile-time platform so it is accurate for all
	// three Alpakit targets: "Windows" and "WindowsServer" (both Win64 = "windows"),
	// and "LinuxServer" (Linux = "linux").
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

	UE_LOG(LogDiscordBridge, Log,
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
		UE_LOG(LogDiscordBridge, Warning,
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

	UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Presence updated to '%s'."), *Status);
}

void UDiscordBridgeSubsystem::SendStatusMessageToDiscord(const FString& Message)
{
	if (!Config.bServerStatusMessagesEnabled)
	{
		return;
	}

	// Use the dedicated status channel(s) when configured; fall back to the main channel(s).
	const TArray<FString> TargetChannelIds = Config.StatusChannelId.IsEmpty()
		? FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId)
		: FDiscordBridgeConfig::ParseChannelIds(Config.StatusChannelId);

	// Prepend the @mention prefix when a notify role/user is configured.
	// Discord role mention syntax:  <@&ROLE_ID>
	// Discord user mention syntax:  <@USER_ID>
	// Special keywords @everyone and @here are passed through as-is.
	FString MentionPrefix;
	if (!Config.ServerStatusNotifyRoleId.IsEmpty())
	{
		const FString& Id = Config.ServerStatusNotifyRoleId;
		if (Id == TEXT("everyone") || Id == TEXT("@everyone"))
		{
			MentionPrefix = TEXT("@everyone ");
		}
		else if (Id == TEXT("here") || Id == TEXT("@here"))
		{
			MentionPrefix = TEXT("@here ");
		}
		else
		{
			MentionPrefix = TEXT("<@&") + Id + TEXT("> ");
		}
	}

	const FString FullMessage = MentionPrefix + Message;
	for (const FString& ChanId : TargetChannelIds)
	{
		SendMessageToChannel(ChanId, FullMessage);
	}
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
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: HTTP request failed for status message '%s'."),
				       *Message);
				return;
			}
			if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
			{
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: Discord REST API returned %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
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

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Player count presence updated (%d players) – activity: \"%s\""),
	       PlayerCount, *ActivityText);
}

// ─────────────────────────────────────────────────────────────────────────────
// Config hot-reload polling
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::TickConfigPolling(float)
{
	if (!FDiscordBridgeConfig::HasCredentials())
	{
		return true; // Still not configured – keep polling.
	}

	// Credentials now present in the config file.  Perform a full reload so
	// all settings (format strings, status messages, backups, etc.) are applied.
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: BotToken and ChannelId detected in config – "
	            "performing full config reload and starting Discord connection."));
	Config = FDiscordBridgeConfig::LoadOrCreate();

	// Guard against the narrow TOCTOU window where credentials were present when
	// HasCredentials() ran but LoadOrCreate() ended up with empty credentials
	// (e.g. primary wiped and backup missing).  Without this check, Connect()
	// would be called with an empty BotToken and enter an infinite reconnect loop.
	if (Config.BotToken.IsEmpty() || Config.ChannelId.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Credentials were detected but disappeared after full reload. "
		            "Continuing to poll every 30 seconds."));
		return true; // Keep polling.
	}

	// Reload the whitelist and ban managers now that the config has been refreshed.
	// This mirrors what InitializeServer() does and ensures the managers reflect any
	// setting changes the operator made at the same time as adding credentials.
	// BanSystemEnabled always comes from the INI (never from the JSON state file),
	// so re-loading is the only way to pick up changes made since startup.
	FWhitelistManager::Load(Config.bWhitelistEnabled);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Whitelist active = %s (WhitelistEnabled config = %s)"),
	       FWhitelistManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bWhitelistEnabled ? TEXT("True") : TEXT("False"));

	FBanManager::Load(Config.bBanSystemEnabled);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: BanSystem active = %s (BanSystemEnabled config = %s)"),
	       FBanManager::IsEnabled() ? TEXT("True") : TEXT("False"),
	       Config.bBanSystemEnabled ? TEXT("True") : TEXT("False"));

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: ServerName           = \"%s\""), *Config.ServerName);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: GameToDiscordFormat  = \"%s\""), *Config.GameToDiscordFormat);
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: DiscordToGameFormat  = \"%s\""), *Config.DiscordToGameFormat);

	Connect();
	return false; // Connected – stop the polling ticker.
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

	UE_LOG(LogDiscordBridge, Log, TEXT("DiscordBridge: Bound to AFGChatManager::OnChatMessageAdded."));
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
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Skipping player message with empty text from '%s'."),
		       *PlayerName);
		return;
	}

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Player message detected. Sender: '%s', Text: '%s'"),
	       *PlayerName, *MessageText);

	// SML chat commands start with '/' and are processed entirely in-game.
	// They should not be forwarded to Discord as ordinary chat messages.
	if (MessageText.StartsWith(TEXT("/"), ESearchCase::CaseSensitive))
	{
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Skipping in-game SML command from '%s' – not relaying to Discord."),
		       *PlayerName);
		return;
	}

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

	// Check whether this message is an in-game kick command.
	if (!Config.InGameKickCommandPrefix.IsEmpty() &&
	    MessageText.StartsWith(Config.InGameKickCommandPrefix, ESearchCase::IgnoreCase))
	{
		if (!Config.bKickCommandsEnabled)
		{
			return;
		}
		const FString SubCommand = MessageText.Mid(Config.InGameKickCommandPrefix.Len()).TrimStartAndEnd();
		HandleInGameKickCommand(SubCommand);
		return; // Do not forward commands to Discord.
	}

	// Check whether this message is an in-game server-info command.
	if (!Config.InGameServerInfoCommandPrefix.IsEmpty() &&
	    MessageText.StartsWith(Config.InGameServerInfoCommandPrefix, ESearchCase::IgnoreCase))
	{
		const FString SubCommand = MessageText.Mid(Config.InGameServerInfoCommandPrefix.Len()).TrimStartAndEnd();
		HandleInGameServerInfoCommand(SubCommand);
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
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Cannot send message - BotToken or ChannelId not configured."));
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
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: GameToDiscordFormat produced an empty string for player '%s'. "
		            "Check the GameToDiscordFormat setting in DefaultDiscordBridge.ini."),
		       *EffectivePlayerName);
		return;
	}

	UE_LOG(LogDiscordBridge, Log,
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
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge: HTTP request failed for player '%s'."),
					       *EffectivePlayerName);
					return;
				}
				if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
				{
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge: Discord REST API returned %d: %s"),
					       Resp->GetResponseCode(), *Resp->GetContentAsString());
				}
			});

		Request->ProcessRequest();
	};

	// POST to all main chat channels.
	const TArray<FString> MainChannelIds = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
	for (const FString& ChanId : MainChannelIds)
	{
		PostToChannel(ChanId);
	}

	// When dedicated whitelist channel(s) are configured, also post there for
	// players who are on the whitelist (so whitelisted members have their own
	// channel view of whitelisted player activity).
	if (FWhitelistManager::IsWhitelisted(EffectivePlayerName))
	{
		const TArray<FString> WhitelistChannelIds = FDiscordBridgeConfig::ParseChannelIds(Config.WhitelistChannelId);
		for (const FString& ChanId : WhitelistChannelIds)
		{
			if (!MainChannelIds.Contains(ChanId))
			{
				PostToChannel(ChanId);
			}
		}
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
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: No world available – cannot relay Discord message to game chat."));
		return;
	}

	AFGChatManager* ChatManager = AFGChatManager::Get(World);
	if (!ChatManager)
	{
		UE_LOG(LogDiscordBridge, Warning,
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

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Relayed to game chat – text: '%s'"),
	       *FormattedMessage);

	// Notify other mods via GameplayEvents that a Discord message arrived.
	// Payload is "Username: Message" so subscribers have the full context.
	DispatchDiscordGameplayEvent(FDiscordGameplayTags::FromDiscord(),
	                             FString::Printf(TEXT("%s: %s"), *Username, *Message));
}

// ─────────────────────────────────────────────────────────────────────────────
// Whitelist and ban enforcement
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Returns the platform ID string for the given player state, or an empty
 * string when the ID cannot be safely retrieved.
 *
 * The resolution strategy is split by ID type:
 *
 *   Steam players (FUniqueNetId::GetType() != "EOS"):
 *     Steam64 IDs are read directly from PS->GetUniqueId() without going
 *     through any EOS SDK.  Steam IDs are backed by the Steam SDK, not the EOS
 *     SDK, so the mIsOnline and GetNumLocalPlayers() guards do NOT apply.
 *     Steam ban enforcement therefore works even when EOS is unavailable.
 *
 *   EOS players (FUniqueNetId::GetType() == "EOS"):
 *     Delegated to SafeGetEOSPlayerPlatformId(), which applies the mIsOnline
 *     guard (via UE reflection) and the GetNumLocalPlayers() guard before
 *     touching any EOS SDK function, preventing the SIMD/SIGSEGV crash
 *     confirmed in CSS UE5.3 production logs.  Returns empty string when EOS
 *     is not yet ready; the deferred retry timer in OnPostLogin will retry.
 */
FString UDiscordBridgeSubsystem::GetPlayerPlatformId(const APlayerState* PS) const
{
	if (!PS || !::IsValid(PS))
	{
		return FString();
	}

	// ── Direct non-EOS path (Steam and any other non-EOS platform) ────────────
	// Steam players carry Steam64 IDs that are backed entirely by the Steam SDK,
	// not the EOS SDK.  FUniqueNetIdSteam::ToString() is always safe to call
	// regardless of mIsOnline or EOS state; there is no EOS handle involved and
	// therefore no SIGSEGV risk.  Steam ban enforcement works even when EOS is
	// unavailable.
	const FUniqueNetIdRepl& IdRepl = PS->GetUniqueId();
	const FUniqueNetId* const RawPtr = IdRepl.operator->();
	if (RawPtr && RawPtr->GetType() != FName(TEXT("EOS")))
	{
		// Non-EOS ID (Steam64 etc.) – safe to call ToString() directly.
		return RawPtr->ToString();
	}

	// ── EOS PUID path – inline safe resolution using OnlineIntegration ────────
	// Applies the mIsOnline and GetNumLocalPlayers() guards that prevent SIGSEGV
	// when touching EOS SDK handles in CSS UE5.3.  These guards were previously
	// centralised in UEOSIntegrationSubsystem; they are now inlined here so that
	// DiscordBridge does not need a separate EOSIntegration mod.
	const FString EosId = SafeGetEOSPlayerPlatformId(PS);
	if (!EosId.IsEmpty())
	{
		return EosId;
	}

	// ── Component fallback ────────────────────────────────────────────────────
	// When the EOS SDK path returns empty (mIsOnline=false or
	// GetNumLocalPlayers()==0), try UOnlineIntegrationControllerComponent.
	// The component receives the client's native platform account ID
	// (Steam64 or EOS PUID) via a reliable Server RPC typically arriving
	// <<1 s after PostLogin – much sooner than the EOS SDK path on a fresh
	// server (which can take up to 60 s to authenticate).
	// This makes Steam64-based bans enforceable without depending on EOS,
	// and also enables admin lookup commands to show a platform ID even
	// when EOS is still initialising.
	const APlayerController* CompPC = Cast<APlayerController>(PS->GetOwner());
	if (CompPC && !CompPC->IsLocalController())
	{
		return GetPlayerPlatformIdFromComponent(CompPC);
	}
	return FString();
}

FString UDiscordBridgeSubsystem::GetPlayerPlatformIdFromComponent(
	const APlayerController* PC) const
{
	if (!PC || !::IsValid(PC))
	{
		return FString();
	}

	// UOnlineIntegrationControllerComponent is a PlayerController component
	// that receives the client's platform account ID (Steam64 or EOS PUID) via
	// a reliable Server RPC (Server_RegisterControllerComponent) during the
	// client's BeginPlay().  The data typically arrives 1–2 network round-trips
	// after PostLogin (<<1 s), making this a fast fallback when the EOS SDK path
	// in GetPlayerPlatformId() returns empty because:
	//   • mIsOnline=false – EOS session not yet registered, OR
	//   • GetNumLocalPlayers()==0 – server EOS service account not yet authenticated
	//     (can take up to 60 s; the ProductUserId handle is at an invalid address
	//     in this state – attempting EOS_ProductUserId_ToString() crashes the server).
	// This path makes NO EOS SDK calls and is safe regardless of mIsOnline state.
	const UOnlineIntegrationControllerComponent* OICComp =
	    PC->FindComponentByClass<UOnlineIntegrationControllerComponent>();
	if (!OICComp)
	{
		return FString();
	}

	const UE::Online::FAccountId AccId = OICComp->GetPlatformAccountId();
	if (!AccId.IsValid())
	{
		// Server RPC has not arrived yet – component data is still pending.
		return FString();
	}

	// Serialize the FAccountId to the X-FactoryGame-PlayerId hex format used by
	// NormalizePlatformId() and the ban system.  Use FLocalUserNetIdBundle from
	// LocalUserInfo.h (OnlineIntegration public API) – this is the CSS-sanctioned
	// serialisation path that mirrors how FGPlayerState stores the platform ID.
	// SetAssociatedAccountId() produces:
	//   BytesToHex([EOnlineServices_type_byte, ...raw_account_id_bytes...])
	// NormalizePlatformId() then converts:
	//   "06" + 16 hex chars  →  decimal Steam64 ID   (EOnlineServices::Steam = 6)
	//   "01" + 32 hex chars  →  32-char EOS PUID hex  (EOnlineServices::Epic  = 1)
	FLocalUserNetIdBundle Bundle;
	Bundle.SetAssociatedAccountId(AccId);
	const FString& HexStr = Bundle.AssociatedAccountIdString;

	// NormalizePlatformId() is available in FBanManager as a static C++ helper.
	// This avoids any dependency on UEOSIntegrationSubsystem.
	const FString Canonical = FBanManager::NormalizePlatformId(HexStr);

	if (!Canonical.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Verbose,
		       TEXT("DiscordBridge: GetPlayerPlatformIdFromComponent: resolved '%s' "
		            "(backend='%s') via UOnlineIntegrationControllerComponent"),
		       *Canonical, *OICComp->GetPlayerPlatformBackend().ToString());
	}
	return Canonical;
}

// ─────────────────────────────────────────────────────────────────────────────
// Platform helpers (replaces UEOSIntegrationSubsystem)
// ─────────────────────────────────────────────────────────────────────────────

bool UDiscordBridgeSubsystem::IsEOSPlatformOperational() const
{
	// The EOS session manager is the authoritative signal that EOS is
	// fully initialised.  UOnlineIntegrationSubsystem::GetSessionManager()
	// returns non-null when the CSS engine's EOS stack is ready.
	// This mirrors UEOSIntegrationSubsystem::IsPlatformOperational() exactly.
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UOnlineIntegrationSubsystem* OIS =
				GI->GetSubsystem<UOnlineIntegrationSubsystem>())
		{
			return OIS->GetSessionManager() != nullptr;
		}
	}
	return false;
}

FString UDiscordBridgeSubsystem::GetConfiguredPlatformServiceName() const
{
	// Read DefaultPlatformService from [OnlineSubsystem] in DefaultEngine.ini.
	// This is the same GConfig read that UEOSIntegrationSubsystem::ProbeOnlinePlatform()
	// performs; using it directly eliminates the EOSIntegration dependency.
	FString ServiceName;
	if (GConfig)
	{
		GConfig->GetString(TEXT("OnlineSubsystem"),
						   TEXT("DefaultPlatformService"),
						   ServiceName, GEngineIni);
	}
	return ServiceName;
}

bool UDiscordBridgeSubsystem::IsEOSPlatformConfigured() const
{
	// "EOS" is explicitly configured, OR the EOS session manager is present
	// at runtime even when the INI says something else (auto-detection, same
	// logic as UEOSIntegrationSubsystem::ProbeOnlinePlatform() bRuntimeEosAutoDetected).
	const FString Name = GetConfiguredPlatformServiceName();
	if (Name.Equals(TEXT("EOS"), ESearchCase::IgnoreCase))
	{
		return true;
	}
	// Runtime auto-detection: session manager present even if INI disagrees.
	return IsEOSPlatformOperational();
}

bool UDiscordBridgeSubsystem::IsSteamPlatformConfigured() const
{
	return GetConfiguredPlatformServiceName()
			   .Equals(TEXT("Steam"), ESearchCase::IgnoreCase);
}

bool UDiscordBridgeSubsystem::IsNoPlatformConfigured() const
{
	// No recognised platform means neither EOS (including auto-detected) nor Steam.
	return !IsEOSPlatformConfigured() && !IsSteamPlatformConfigured();
}

int32 UDiscordBridgeSubsystem::GetNumLocalOnlineUsers() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		if (const UOnlineIntegrationSubsystem* OIS =
				GI->GetSubsystem<UOnlineIntegrationSubsystem>())
		{
			if (const UCommonUserSubsystem* UserMgr = OIS->GetUserManager())
			{
				return UserMgr->GetNumLocalPlayers();
			}
		}
	}
	return 0;
}

FString UDiscordBridgeSubsystem::SafeGetEOSPlayerPlatformId(const APlayerState* PS) const
{
	if (!PS || !::IsValid(PS))
	{
		return FString();
	}

	// Cache the mIsOnline property descriptor (permanent for engine session).
	static const FBoolProperty* IsOnlineProp = CastField<FBoolProperty>(
		AFGPlayerState::StaticClass()->FindPropertyByName(TEXT("mIsOnline")));

	const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(PS);
	if (!FGPS)
	{
		// Non-AFGPlayerState: cannot safely access EOS PUID (no mIsOnline guard).
		UE_LOG(LogDiscordBridge, Warning,
			   TEXT("SafeGetEOSPlayerPlatformId: PS '%s' is not AFGPlayerState – returning empty."),
			   *PS->GetPlayerName());
		return FString();
	}

	// ── Non-EOS fast path ────────────────────────────────────────────────────
	// Steam64 and other non-EOS IDs are safe to read regardless of mIsOnline.
	// There is no EOS SDK handle involved; ToString() never crashes for these types.
	const FUniqueNetIdRepl& IdRepl  = PS->GetUniqueId();
	const FUniqueNetId* const RawPtr = IdRepl.operator->();
	if (!RawPtr || RawPtr->GetType() != FName(TEXT("EOS")))
	{
		UE_LOG(LogDiscordBridge, Verbose,
			   TEXT("SafeGetEOSPlayerPlatformId: non-EOS fast-path for '%s' (type='%s')."),
			   *PS->GetPlayerName(),
			   RawPtr ? *RawPtr->GetType().ToString() : TEXT("null"));
		return RawPtr ? RawPtr->ToString() : FString();
	}

	// ── EOS PUID path – apply crash guards ───────────────────────────────────
	// Guard 1: mIsOnline=false means the EOS ProductUserId handle is at an
	// invalid address.  Calling ToString() or IsValid() on it causes SIGSEGV.
	if (!IsOnlineProp || !IsOnlineProp->GetPropertyValue_InContainer(FGPS))
	{
		UE_LOG(LogDiscordBridge, Verbose,
			   TEXT("SafeGetEOSPlayerPlatformId: mIsOnline=false for '%s' – returning empty."),
			   *PS->GetPlayerName());
		return FString();
	}

	// Guard 2: GetNumLocalPlayers()==0 means the server's EOS service account
	// is not yet authenticated.  The EOS ProductUserId handle is at an invalid
	// address even when mIsOnline=true.  This prevents the confirmed crash at
	// address 0x0000000006000001 seen in CSS UE5.3 production logs.
	// *** DO NOT REMOVE THIS GUARD ***
	if (GetNumLocalOnlineUsers() == 0)
	{
		UE_LOG(LogDiscordBridge, Verbose,
			   TEXT("SafeGetEOSPlayerPlatformId: GetNumLocalOnlineUsers()==0 for '%s' – returning empty."),
			   *PS->GetPlayerName());
		return FString();
	}

	// Both guards passed: the EOS ProductUserId handle is fully initialised.
	const FUniqueNetId* const RawId = PS->GetUniqueId().operator->();
	return RawId ? RawId->ToString() : FString();
}

FString UDiscordBridgeSubsystem::BuildPlatformDiagnostics(bool bPlainText) const
{
	// Read EOS credentials from config for display purposes.
	auto ReadCfg = [](const TCHAR* Key) -> FString
	{
		FString Val;
		if (GConfig)
		{
			GConfig->GetString(TEXT("OnlineSubsystemEOS"), Key, Val, GEngineIni);
		}
		return Val.IsEmpty() ? TEXT("<not set>") : Val;
	};

	// Re-check runtime state live.
	const bool bLiveSessionMgr = IsEOSPlatformOperational();
	const int32 LiveLocalUsers = GetNumLocalOnlineUsers();
	const bool bLiveOIS = [this]() -> bool
	{
		if (const UGameInstance* GI = GetGameInstance())
		{
			return GI->GetSubsystem<UOnlineIntegrationSubsystem>() != nullptr;
		}
		return false;
	}();

	const FString ConfigName = GetConfiguredPlatformServiceName();
	const bool bNullSubsystem =
		!ConfigName.IsEmpty() &&
		ConfigName.Equals(TEXT("NULL"), ESearchCase::IgnoreCase);
	const bool bNoneSubsystem =
		!ConfigName.IsEmpty() &&
		ConfigName.Equals(TEXT("None"), ESearchCase::IgnoreCase);

	FString ConfigDisplay;
	if (ConfigName.IsEmpty())         { ConfigDisplay = TEXT("(not set)"); }
	else if (bNullSubsystem)          { ConfigDisplay = TEXT("NULL (UE no-op – not a real platform service)"); }
	else if (bNoneSubsystem)          { ConfigDisplay = TEXT("None (explicit no-platform configuration)"); }
	else                              { ConfigDisplay = ConfigName; }

	FString Report;
	if (!bPlainText)
	{
		Report += TEXT("**EOS / Platform Diagnostics**\n");
		Report += TEXT("──────────────────────────────\n");
	}
	Report += FString::Printf(TEXT("Platform service (config):  %s\n"), *ConfigDisplay);

	const FString DetectedStr = IsEOSPlatformConfigured()  ? TEXT("EOS")
							  : IsSteamPlatformConfigured() ? TEXT("Steam")
							  :                               TEXT("None");
	Report += FString::Printf(TEXT("Detected type:              %s\n"), *DetectedStr);

	// Note when EOS is auto-detected at runtime despite a non-EOS INI value.
	const bool bAutoDetected = IsEOSPlatformConfigured()
		&& !ConfigName.Equals(TEXT("EOS"), ESearchCase::IgnoreCase);
	if (bAutoDetected)
	{
		if (!bPlainText)
		{
			Report += FString::Printf(
				TEXT(":information_source: **EOS auto-detected** – `DefaultPlatformService=%s` in "
					 "`DefaultEngine.ini` but the EOS session manager is present at runtime. "
					 "Set `DefaultPlatformService=EOS` (and `NativePlatformService=Steam` for "
					 "Steam player bridging) to suppress this note.\n"),
				*ConfigName);
		}
		else
		{
			Report += FString::Printf(
				TEXT("Note (EOS auto-detected): DefaultPlatformService=%s but EOS session manager "
					 "is present – EOS is being used automatically.\n"),
				*ConfigName);
		}
	}

	Report += FString::Printf(TEXT("Platform operational (live): %s\n"),
							   bLiveSessionMgr ? TEXT("YES") : TEXT("NO"));
	Report += TEXT("\n");

	if (!bPlainText) { Report += TEXT("**Runtime (live)**\n"); }
	else             { Report += TEXT("Runtime (live)\n"); }
	Report += FString::Printf(TEXT("OnlineIntegrationSubsystem: %s\n"),
							   bLiveOIS ? TEXT("present") : TEXT("NULL"));
	Report += FString::Printf(TEXT("Session manager:            %s\n"),
							   bLiveSessionMgr ? TEXT("present") : TEXT("NULL"));
	Report += FString::Printf(TEXT("Local online users:         %d\n"), LiveLocalUsers);

	if (IsEOSPlatformConfigured())
	{
		Report += TEXT("\n");
		if (!bPlainText) { Report += TEXT("**EOS Credentials (DefaultEngine.ini [OnlineSubsystemEOS])**\n"); }
		else             { Report += TEXT("EOS Credentials (DefaultEngine.ini [OnlineSubsystemEOS])\n"); }
		Report += FString::Printf(TEXT("ProductId:     %s\n"), *ReadCfg(TEXT("ProductId")));
		Report += FString::Printf(TEXT("SandboxId:     %s\n"), *ReadCfg(TEXT("SandboxId")));
		Report += FString::Printf(TEXT("DeploymentId:  %s\n"), *ReadCfg(TEXT("DeploymentId")));
		Report += FString::Printf(TEXT("ClientId:      %s\n"), *ReadCfg(TEXT("ClientId")));

		if (!bLiveOIS || !bLiveSessionMgr)
		{
			Report += TEXT("\n");
			if (!bPlainText) { Report += TEXT(":warning: **Fix instructions**\n"); }
			else             { Report += TEXT("Fix instructions\n"); }
			if (!bLiveOIS)
			{
				Report += TEXT("• OnlineIntegrationSubsystem is NULL.  Ensure the CSS engine's "
							   "OnlineIntegration plugin is enabled.\n");
			}
			if (!bLiveSessionMgr)
			{
				Report += TEXT("• Session manager is NULL.  Check [OnlineSubsystem] "
							   "DefaultPlatformService=EOS and EOS credentials in DefaultEngine.ini.\n");
			}
		}
		if (LiveLocalUsers == 0)
		{
			Report += TEXT("\n");
			if (!bPlainText)
			{
				Report += TEXT(":warning: **Server EOS account not yet authenticated (GetNumLocalOnlineUsers()==0)**\n");
			}
			else
			{
				Report += TEXT("Server EOS account not yet authenticated (GetNumLocalOnlineUsers()==0)\n");
			}
			Report += TEXT("• Player platform IDs are NOT available yet.  Deferred-retry timer is active.\n"
						   "• Check EOS credentials in DefaultEngine.ini if this persists.\n");
		}
	}
	else if (IsSteamPlatformConfigured())
	{
		Report += TEXT("\nSteam platform detected.\n");
		if (IsRunningDedicatedServer())
		{
			if (!bPlainText) { Report += TEXT(":warning: **CSS server misconfiguration warning**\n"); }
			else             { Report += TEXT("CSS server misconfiguration warning\n"); }
			Report += TEXT("DefaultPlatformService=Steam is set, but CSS dedicated servers do NOT\n"
						   "load OnlineSubsystemSteam.  Use DefaultPlatformService=EOS + NativePlatformService=Steam.\n");
		}
		else
		{
			Report += TEXT("No EOS credentials needed – player IDs are Steam64 IDs.\n");
		}
	}
	else
	{
		if (bNoneSubsystem)
		{
			if (!bPlainText) { Report += TEXT("\n:information_source: No online platform configured (DefaultPlatformService=None).\n"); }
			else             { Report += TEXT("\n No online platform configured (DefaultPlatformService=None).\n"); }
			Report += TEXT("EOS PUID ban enforcement is not available.  Steam64 bans are still enforced.\n");
			Report += TEXT("Set [OnlineSubsystem] DefaultPlatformService=EOS + NativePlatformService=Steam to enable EOS PUID bans.\n");
		}
		else
		{
			if (!bPlainText) { Report += TEXT("\n:warning: No recognised platform service is configured.\n"); }
			else             { Report += TEXT("\n No recognised platform service is configured.\n"); }
			if (bNullSubsystem)
			{
				Report += TEXT("DefaultPlatformService is set to NULL (UE no-op).  Check for a duplicate "
							   "[OnlineSubsystem] section in DefaultEngine.ini.\n");
			}
			else
			{
				Report += TEXT("Set [OnlineSubsystem] DefaultPlatformService=EOS or Steam in DefaultEngine.ini.\n");
			}
		}
	}

	return Report;
}

void UDiscordBridgeSubsystem::OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller)
{
	if (!Controller || Controller->IsLocalController())
	{
		return;
	}

	// Log entry details so crashes during the join flow can be correlated to a
	// specific player/connection without needing a debugger.
	const FString EntryRemoteAddr = Controller->NetConnection
		? Controller->NetConnection->LowLevelGetRemoteAddress(true)
		: TEXT("(no connection)");
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: OnPostLogin – controller='%s', remoteAddr=%s, "
	            "hasNetConn=%s, connState=%d"),
	       *Controller->GetName(),
	       *EntryRemoteAddr,
	       Controller->NetConnection ? TEXT("yes") : TEXT("no"),
	       Controller->NetConnection ? static_cast<int32>(Controller->NetConnection->GetConnectionState()) : -1);

	// Guard against players whose session failed to register
	// (e.g. RegisterPlayerWithSession returned false due to IsOnline=false).
	// When registration fails the engine may close or invalidate the net
	// connection during the same game frame, before PostLogin completes.
	// Accessing player-state objects through a closed connection is unsafe
	// and can produce a SIGSEGV.
	if (Controller->NetConnection &&
	    (Controller->NetConnection->GetConnectionState() == USOCK_Closed ||
	     Controller->NetConnection->GetConnectionState() == USOCK_Invalid))
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: OnPostLogin – net connection already closed or invalid "
		            "(state=%d, addr=%s). RegisterPlayerWithSession likely failed. "
		            "Skipping player tracking."),
		       static_cast<int32>(Controller->NetConnection->GetConnectionState()),
		       *EntryRemoteAddr);
		return;
	}

	// The connection has progressed past the pre-login phase – remove it from
	// the pending-connection tracking set so the net monitor does not emit a
	// spurious "dropped before login" notification for this player.
	if (Controller->NetConnection)
	{
		PendingConnectionAddresses.Remove(EntryRemoteAddr);
	}

	const APlayerState* PS = Controller->GetPlayerState<APlayerState>();
	const FString PlayerName = PS ? PS->GetPlayerName() : FString();
	const FString PlatformIdEntry = GetPlayerPlatformId(PS);

	// Gather the raw UniqueNetId type (subsystem name) for the join log.
	// GetType() is always safe to call; ToString() is handled safely above by
	// GetPlayerPlatformId() which guards against the EOS SIGSEGV.
	const FUniqueNetId* const RawId = PS ? PS->GetUniqueId().operator->() : nullptr;
	const FString UniqueIdType = RawId ? RawId->GetType().ToString() : TEXT("(none)");

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: OnPostLogin – playerName='%s', uniqueIdType='%s', uniqueId='%s', addr=%s"),
	       *PlayerName, *UniqueIdType, *PlatformIdEntry, *EntryRemoteAddr);

	// Detect mIsOnline=false at join time.  This occurs when a player disconnects
	// and quickly rejoins before the previous EOS session is fully cleaned up, or
	// when the server runs without a valid EOS platform (engine emits "Telemetry,
	// no local player found" at startup in that case).  GetPlayerPlatformId already
	// returns '' to prevent a SIGSEGV; record the condition here so the deferred
	// ban-check block below can schedule a retry without repeating the cast.
	static const FBoolProperty* SIsOnlineProp = CastField<FBoolProperty>(
		AFGPlayerState::StaticClass()->FindPropertyByName(TEXT("mIsOnline")));
	const AFGPlayerState* EntryFGPS    = Cast<const AFGPlayerState>(PS);
	const bool            bIsOnlineFalse =
		PlatformIdEntry.IsEmpty() && EntryFGPS && SIsOnlineProp
		&& !SIsOnlineProp->GetPropertyValue_InContainer(EntryFGPS);

	// Broader deferred-retry flag: schedule a platform-ID ban re-check whenever
	// the ID is empty for an AFGPlayerState player and the ban system is active.
	//
	// GetPlayerPlatformId() returns empty in three known scenarios:
	//   1. mIsOnline=false – EOS session not registered yet (most common cause:
	//      player disconnected and rejoined before the old session cleaned up).
	//   2. GetNumLocalPlayers()==0 – mIsOnline=true but the server's EOS service
	//      account is not yet authenticated.  Production logs confirm the EOS
	//      ProductUserId handle is at an invalid address in this state, so
	//      GetPlayerPlatformId() returns empty to avoid SIGSEGV.
	//      This is also the root cause of the engine's LogGame Error:
	//        "RegisterPlayerWithSession: Failed –
	//         UniqueId.IsValid(): true, IsV2(): true, IsOnline: false"
	//      Both errors share the same root cause: the server's EOS local user
	//      is not yet authenticated (GetNumLocalPlayers()==0).  Once the EOS
	//      service account authenticates, both the session registration and the
	//      platform-ID resolution succeed.  The engine also logs:
	//        "Set Online State For Player : false/true"
	//        "Could not process player whose OnlineState changed: local user was invalid"
	//      for the same reason.
	//   3. BaseId TSharedPtr null – mIsOnline=true but the EOS handle has not
	//      yet been populated (rare edge case).
	//
	// DO NOT remove the GetNumLocalPlayers()==0 guard from GetPlayerPlatformId()
	// in EOSIntegrationSubsystem.cpp.  It prevents a confirmed crash at address
	// 0x0000000006000001.  The retry here handles all three scenarios.
	// It stops as soon as a non-empty ID is returned.
	//
	// bPlatformIdResolutionConfirmedUnavailable is set when the EOS platform is
	// definitively known to be unable to supply EOS PUIDs (no platform configured,
	// or the EOS platform timed out on a previous player join).  In that case
	// the 120-retry EOS PUID timer is pointless and would only generate Discord
	// spam on every subsequent player join; skip it entirely.
	//
	// NOTE: Steam players are NOT affected by this flag — their Steam64 IDs are
	// resolved directly in GetPlayerPlatformId() without EOSIntegration, so
	// PlatformIdEntry is always non-empty for Steam players and this condition
	// evaluates to false regardless of bPlatformIdResolutionConfirmedUnavailable.
	const bool bNeedsDeferredPlatformIdCheck =
		FBanManager::IsEnabled() && PlatformIdEntry.IsEmpty() && EntryFGPS
		&& !bPlatformIdResolutionConfirmedUnavailable;

	if (bIsOnlineFalse)
	{
		// Emit a Warning so the empty platformId is explained in server logs.
		// mIsOnline=false at PostLogin occurs in several distinct scenarios:
		//
		//  0. No online platform configured (DefaultPlatformService is not EOS
		//     or Steam).  mIsOnline stays false because there is no session
		//     manager to register the player.  A startup warning was already
		//     emitted; suppress per-join noise to avoid flooding the log.
		//
		//  1. Server EOS service account not yet authenticated
		//     (GetNumLocalPlayers()==0).  This is the common "first player
		//     joins while server EOS is still initialising" case.  The engine
		//     logs correlate:
		//       • LogGame Error: "RegisterPlayerWithSession: Failed –
		//         UniqueId.IsValid(): true, IsV2(): true, IsOnline: false"
		//       • LogGame Warning: "Set Online State For Player : true"
		//         followed immediately by "Could not process player whose
		//         OnlineState changed: local user was invalid"
		//     The deferred retry (scheduled below) will keep polling until
		//     GetNumLocalPlayers()>0, at which point the EOS PUID resolves
		//     and ban enforcement runs.
		//
		//  2. Player rejoined before previous EOS session was cleaned up
		//     (GetNumLocalPlayers()>0 but player's own session is in a
		//     transitional state).  EOS fails to register the new session
		//     because the old one is still being torn down.  mIsOnline is
		//     briefly false at PostLogin; the deferred retry below catches
		//     it once the session state settles.
		//
		// NOTE (Steam servers): mIsOnline=false can occur on Steam servers too
		// (Steam session registration failure).  However, for Steam players the
		// Steam64 ID is resolved DIRECTLY from PS->GetUniqueId() – it does NOT
		// depend on mIsOnline and is already in PlatformIdEntry.  No EOS-related
		// action is needed and the EOS warnings below do not apply.
		//
		// Use GetNumLocalEOSUsers() to distinguish scenarios 1 and 2 and emit
		// the most accurate warning for the server admin.
		if (!PlatformIdEntry.IsEmpty())
		{
			// Platform ID was resolved immediately (e.g. Steam64 ID via the
			// direct non-EOS path).  mIsOnline=false is irrelevant for ban
			// enforcement – the ID is already available and was checked above.
			// Log at Verbose only to avoid noise; the join log already has the ID.
			UE_LOG(LogDiscordBridge, Verbose,
			       TEXT("DiscordBridge: mIsOnline=false for '%s' (addr=%s) but "
			            "platform ID '%s' was resolved via direct path – "
			            "no deferred EOS check needed."),
			       *PlayerName, *EntryRemoteAddr, *PlatformIdEntry);
		}
		else if (bPlatformIdResolutionConfirmedUnavailable)
		{
			// Scenario 0: no platform configured.  The startup warning already
			// told the admin; no per-join repetition is needed.  Deferred
			// EOS PUID checks are suppressed for this server session.
		}
		else
		{
			// WarnEOSSub is no longer needed; use platform helpers directly.
			const int32 LiveEOSUsers = GetNumLocalOnlineUsers();
			const bool bEOSScenario =
			    IsEOSPlatformConfigured()
			    && (LiveEOSUsers == 0 || !IsEOSPlatformOperational());

			if (bEOSScenario)
			{
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: mIsOnline=false for '%s' (addr=%s) – "
				            "server EOS platform not ready (GetNumLocalOnlineUsers=%d, "
				            "IsPlatformOperational=%s). Correlates with LogGame "
				            "'RegisterPlayerWithSession: Failed' and "
				            "'local user was invalid'. EOS PUID ban checks deferred "
				            "until the server EOS account authenticates."),
				       *PlayerName, *EntryRemoteAddr, LiveEOSUsers,
				       IsEOSPlatformOperational() ? TEXT("true") : TEXT("false"));
			}
			else
			{
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: mIsOnline=false for '%s' (addr=%s) – EOS session "
				            "registration failed (GetNumLocalOnlineUsers=%d). Most likely the player "
				            "rejoined before the previous EOS session was cleaned up; the server "
				            "EOS platform is operational. EOS PUID ban checks deferred."),
				       *PlayerName, *EntryRemoteAddr, LiveEOSUsers);
			}
		}
	}
	else if (bNeedsDeferredPlatformIdCheck)
	{
		// mIsOnline=true but platform ID is still empty.  Most likely cause:
		//   GetNumLocalPlayers()==0 – the server's EOS service account is not
		//   yet authenticated.  This is also why the engine logs
		//   "RegisterPlayerWithSession: Failed – UniqueId.IsOnline: false"
		//   and "Could not process player whose OnlineState changed: local user
		//   was invalid" in the same frame: all three errors share the same root
		//   cause.  The deferred retry below will keep polling until the server's
		//   EOS local user authenticates (GetNumLocalPlayers()>0), at which point
		//   the platform ID will resolve and the ban check will be enforced.
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: platform ID empty for '%s' (addr=%s) with mIsOnline=true – "
		            "server EOS service account not yet authenticated (GetNumLocalPlayers()==0) "
		            "or BaseId null. Correlates with LogGame 'RegisterPlayerWithSession: Failed' "
		            "and 'local user was invalid' errors. Platform-ID ban checks deferred."),
		       *PlayerName, *EntryRemoteAddr);
	}

	// In Satisfactory's CSS UE build, the player name is populated asynchronously
	// from Epic Online Services (via Server_SetPlayerNames RPC) after PostLogin fires.

	// When enforcement is active and the name is not yet available, schedule a
	// one-shot deferred retry rather than silently allowing the player through.
	if (PlayerName.IsEmpty())
	{
		const bool bEnforcementActive = FWhitelistManager::IsEnabled() || FBanManager::IsEnabled();
		if (bEnforcementActive)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: player joined with an empty name (addr=%s) – scheduling deferred whitelist/ban check."),
			       *EntryRemoteAddr);

			if (UWorld* World = GetWorld())
			{
				TWeakObjectPtr<AGameModeBase>     WeakGM(GameMode);
				TWeakObjectPtr<APlayerController> WeakPC(Controller);
				TWeakObjectPtr<UWorld>            WeakWorld(World);

				// Retry every 0.5 s, up to MaxRetries times.  A shared handle
				// lets the lambda cancel the repeating timer once done.
				// 40 retries x 0.5 s = 20 s maximum, giving enough headroom for
				// slow EOS name-resolution on first server start or busy networks.
				TSharedRef<FTimerHandle> SharedHandle = MakeShared<FTimerHandle>();
				TSharedRef<int32>        RetriesLeft  = MakeShared<int32>(40);

				World->GetTimerManager().SetTimer(*SharedHandle,
					FTimerDelegate::CreateWeakLambda(this,
						[this, WeakGM, WeakPC, WeakWorld, SharedHandle, RetriesLeft, EntryRemoteAddr]()
					{
						UWorld* W = WeakWorld.Get();

						if (!WeakPC.IsValid())
						{
							// Player already disconnected – stop the timer.
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge: deferred whitelist/ban check – "
							            "player controller gone (addr=%s), aborting."),
							       *EntryRemoteAddr);
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						// Guard against a net connection closed/invalidated while waiting.
						APlayerController* PC = WeakPC.Get();
						if (PC->NetConnection &&
						    (PC->NetConnection->GetConnectionState() == USOCK_Closed ||
						     PC->NetConnection->GetConnectionState() == USOCK_Invalid))
						{
							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge: deferred whitelist/ban check – "
							            "net connection closed/invalid (state=%d, addr=%s), aborting."),
							       static_cast<int32>(PC->NetConnection->GetConnectionState()),
							       *EntryRemoteAddr);
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						// Guard against the player disconnecting between WeakPC.IsValid()
						// and the actual use of the controller.  A null or closed connection
						// means the player has left; there is nothing left to enforce.
						if (!WeakPC->NetConnection ||
						    WeakPC->NetConnection->GetConnectionState() == USOCK_Closed ||
						    WeakPC->NetConnection->GetConnectionState() == USOCK_Invalid)
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						const APlayerState* RetryPS = WeakPC->GetPlayerState<APlayerState>();
						// The raw PlayerState pointer may be non-null but the UObject
						// GC'd between timer schedule and fire.  ::IsValid() checks the
						// GC flags without dereferencing object data and is safe to call
						// on a potentially-freed UObject.
						if (!::IsValid(RetryPS))
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}
						const FString RetryName = RetryPS->GetPlayerName();

						if (!RetryName.IsEmpty())
						{
							// Name is now available – run the whitelist/ban check and stop the timer.
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge: deferred whitelist/ban check – "
							            "name resolved to '%s' (addr=%s), running check."),
							       *RetryName, *EntryRemoteAddr);
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							OnPostLogin(WeakGM.Get(), WeakPC.Get());
							return;
						}

						--(*RetriesLeft);
						if (*RetriesLeft > 0)
						{
							return; // Name not yet populated – try again next tick.
						}

						// All retries exhausted.
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }

						if (FWhitelistManager::IsEnabled())
						{
							// Whitelist is active – we must verify the player’s identity to
							// allow entry.  Kick fail-closed: an unidentified player cannot
							// be confirmed as whitelisted and must reconnect once EOS
							// populates their name.
							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge: player name still empty after deferred check (addr=%s) – kicking to enforce whitelist (fail-closed)."),
							       *EntryRemoteAddr);

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
						}
						else
						{
							// Only the ban system is active (no whitelist).  Ban checks
							// require a known display name; since we cannot identify this
							// player we choose fail-open: let them through rather than kick
							// a legitimate player who has a slow EOS name-resolution.
							// Platform-ID bans are still enforced once the name resolves.
							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge: player name still empty after deferred check (addr=%s) – "
							            "allowing through (ban-only mode, fail-open)."),
							       *EntryRemoteAddr);
						}
					}),
					0.5f, true);
			}
		}
		else
		{
			// Even with enforcement disabled the player name in Satisfactory's
			// EOS build is populated asynchronously after PostLogin fires.
			// Schedule a short retry so the join notification is sent once the
			// name is available, instead of silently dropping it.
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: player joined with an empty name (enforcement disabled) – scheduling deferred join notification."));

			if (UWorld* World = GetWorld())
			{
				TWeakObjectPtr<APlayerController> WeakPC(Controller);
				TWeakObjectPtr<UWorld>            WeakWorld(World);

				// 40 retries x 0.5 s = 20 s maximum.  No kick on timeout – just
				// skip the notification silently; the player is already connected.
				TSharedRef<FTimerHandle> SharedHandle = MakeShared<FTimerHandle>();
				TSharedRef<int32>        RetriesLeft  = MakeShared<int32>(40);

				World->GetTimerManager().SetTimer(*SharedHandle,
					FTimerDelegate::CreateWeakLambda(this,
						[this, WeakPC, WeakWorld, SharedHandle, RetriesLeft]()
					{
						UWorld* W = WeakWorld.Get();

						if (!WeakPC.IsValid())
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						// Abort if the player's connection has closed since the timer
						// was scheduled.  NotifyPlayerJoined should only be called for
						// players who are actively connected.
						if (!WeakPC->NetConnection ||
						    WeakPC->NetConnection->GetConnectionState() == USOCK_Closed ||
						    WeakPC->NetConnection->GetConnectionState() == USOCK_Invalid)
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}

						const APlayerState* RetryPS = WeakPC->GetPlayerState<APlayerState>();
						// Guard: PlayerState may be GC'd by the time the timer fires.
						if (!::IsValid(RetryPS))
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							return;
						}
						const FString RetryName = RetryPS->GetPlayerName();

						if (!RetryName.IsEmpty())
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							NotifyPlayerJoined(WeakPC.Get(), RetryName);
							return;
						}

						--(*RetriesLeft);
						if (*RetriesLeft <= 0)
						{
							if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge: player name still empty after wait (enforcement disabled) – skipping join notification."));
						}
					}),
					0.5f, true);
			}
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
	// Primary platform-ID resolution via EOS SDK (guarded by mIsOnline and
	// GetNumLocalPlayers() guards to prevent SIGSEGV).  This may return empty
	// at PostLogin time when EOS is still initialising; the deferred retry loop
	// below handles that case.
	FString PlatformId = GetPlayerPlatformId(PS);

	// Supplementary fallback: when the EOS SDK path returns empty, try
	// UOnlineIntegrationControllerComponent.  The component receives the
	// client's platform account ID via a reliable Server RPC during BeginPlay().
	// At PostLogin time the RPC has almost certainly NOT arrived yet (it needs a
	// network round-trip), so this will typically still return empty here.
	// However, in edge cases where the Server RPC processes extremely quickly
	// (loopback, local listen-server) the component may already be populated,
	// allowing Steam64 ban enforcement at PostLogin without waiting for the
	// deferred retry timer.
	if (PlatformId.IsEmpty() && FBanManager::IsEnabled())
	{
		PlatformId = GetPlayerPlatformIdFromComponent(Controller);
	}

	const bool bNameBanned     = FBanManager::IsEnabled() && FBanManager::IsBanned(PlayerName);
	const bool bPlatformBanned = FBanManager::IsEnabled() && !PlatformId.IsEmpty()
	                             && FBanManager::IsPlatformIdBanned(PlatformId);

	if (bNameBanned || bPlatformBanned)
	{
		// Include the CSS UE platform name (Steam / Epic / None) in the log so
		// admins can see which online platform the banned player is using.
		const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(PS);
		const FString PlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");

		const FString BanReason = bPlatformBanned
			? FString::Printf(TEXT("platform ID '%s' (platform: %s)"), *PlatformId, *PlatformName)
			: FString::Printf(TEXT("name '%s' (platform: %s)"), *PlayerName, *PlatformName);
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge BanSystem: kicking player '%s' (banned by %s)"),
		       *PlayerName, *BanReason);

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

			const TArray<FString> MainChannelIds = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
			const TArray<FString> BanChannelIds  = FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

			for (const FString& ChanId : MainChannelIds)
			{
				SendMessageToChannel(ChanId, Notice);
			}
			// Also notify the dedicated ban channel(s) (if configured) so admins
			// have a focused audit log of all ban-related events.
			for (const FString& ChanId : BanChannelIds)
			{
				if (!MainChannelIds.Contains(ChanId))
				{
					SendMessageToChannel(ChanId, Notice);
				}
			}
		}

		// For platform-ID bans also send the BanLoginRejectDiscordMessage
		// notification (rate-limited per platform ID, 60 s).  This was
		// previously sent from the AFGGameMode::Login hook; since that hook
		// caused a Fatal crash when the CSS binary's Login implementation was
		// too short for funchook, ban enforcement was moved entirely to
		// PostLogin.  Calling NotifyLoginBanReject here preserves the
		// BanLoginRejectDiscordMessage channel for operators who have it
		// configured, while still sending BanKickDiscordMessage above.
		if (bPlatformBanned)
		{
			NotifyLoginBanReject(PlatformId);
		}
		return;
	}

	// ── Deferred platform-ID ban check ───────────────────────────────────────
	// GetPlayerPlatformId() returns '' in three known crash-safe scenarios:
	//   1. mIsOnline=false – EOS session not registered yet.
	//   2. GetNumLocalPlayers()==0 – mIsOnline=true but the server's EOS service
	//      account is not yet authenticated; the EOS ProductUserId handle is at an
	//      invalid address at this point (confirmed crash at 0x0000000006000001).
	//      This is the root cause of the LogGame Error:
	//        "RegisterPlayerWithSession: Failed – UniqueId.IsOnline: false"
	//      and the LogGame Warnings:
	//        "Set Online State For Player: false/true"
	//        "Could not process player whose OnlineState changed: local user was invalid"
	//      All three errors fire when GetNumLocalPlayers()==0; they resolve once
	//      the server's EOS service account authenticates.
	//   3. BaseId null – the EOS handle TSharedPtr is not yet populated.
	// All three are handled by the guard in EOSIntegrationSubsystem::GetPlayerPlatformId.
	// Retry at 0.5 s intervals (120 retries = 60 s total) so platform-ID bans are
	// enforced once EOS state settles.  60 s gives ample time for the server EOS
	// service account to authenticate, even on a fresh start where the EOS auth
	// handshake can take significantly longer than 15 s (confirmed in production
	// logs: GetNumLocalPlayers()==0 persisted beyond the original 15 s window).
	// DO NOT remove the GetNumLocalPlayers()==0 guard from GetPlayerPlatformId() —
	// without it the EOS SDK dereferences an invalid handle and crashes the server.
	//
	// IMPORTANT – join-notification gating:
	//   When this deferred check is scheduled, OnPostLogin returns IMMEDIATELY
	//   (via the `return` added inside the if-DeferWorld block below) WITHOUT
	//   calling NotifyPlayerJoined() or running the whitelist check.  Both are
	//   deferred to the timer callback, which calls them once the platform-ID ban
	//   result is known.  This prevents a platform-ID-banned player from:
	//     a) receiving the Discord "joined" announcement before their ban is
	//        confirmed, and
	//     b) interacting with the game world during the EOS PUID resolution window.
	//   The whitelist check is also deferred (run inline inside the timer) so that
	//   it still fires exactly once, with the correct ban result already in hand.
	if (bNeedsDeferredPlatformIdCheck)
	{
		if (UWorld* DeferWorld = GetWorld())
		{
			TWeakObjectPtr<APlayerController> DeferWeakPC(Controller);
			TWeakObjectPtr<UWorld>            DeferWeakWorld(DeferWorld);
			TSharedRef<FTimerHandle>          DeferHandle = MakeShared<FTimerHandle>();
			TSharedRef<int32>                 DeferLeft   = MakeShared<int32>(120);

			DeferWorld->GetTimerManager().SetTimer(*DeferHandle,
				FTimerDelegate::CreateWeakLambda(this,
					[this, DeferWeakPC, DeferWeakWorld, DeferHandle, DeferLeft]()
				{
					UWorld* W = DeferWeakWorld.Get();

					if (!DeferWeakPC.IsValid())
					{
						// Player already disconnected – stop the timer.
						if (W) { W->GetTimerManager().ClearTimer(*DeferHandle); }
						return;
					}

					APlayerController* DeferPC = DeferWeakPC.Get();

					// Guard: abort if the connection closed while waiting
					// (covers the leave-then-rejoin scenario where the old
					// controller is still alive but already disconnected).
					if (!DeferPC->NetConnection ||
					    DeferPC->NetConnection->GetConnectionState() == USOCK_Closed ||
					    DeferPC->NetConnection->GetConnectionState() == USOCK_Invalid)
					{
						if (W) { W->GetTimerManager().ClearTimer(*DeferHandle); }
						return;
					}

					const APlayerState* DeferPS = DeferPC->GetPlayerState<APlayerState>();
					// Guard: PlayerState raw pointer may be non-null but GC'd
					// between timer schedule and fire.  ::IsValid() checks GC flags
					// safely without dereferencing object data.
					if (!::IsValid(DeferPS))
					{
						if (W) { W->GetTimerManager().ClearTimer(*DeferHandle); }
						return;
					}
					// Primary resolution: EOS SDK via EOSIntegration (requires
					// mIsOnline=true && GetNumLocalPlayers()>0; may take up to 60 s).
					FString DeferPId = GetPlayerPlatformId(DeferPS);

					// ── Component fallback ────────────────────────────────────────
					// When the EOS SDK path returns empty, try the
					// UOnlineIntegrationControllerComponent.  The component receives
					// the client's platform account ID (Steam64 or EOS PUID) via a
					// reliable Server RPC during BeginPlay(), typically within 1–2
					// network round-trips (<< 1 s after PostLogin).  This makes
					// Steam64-based bans enforceable much sooner and even when EOS is
					// permanently unavailable (no EOS SDK call is involved).
					if (DeferPId.IsEmpty())
					{
						DeferPId = GetPlayerPlatformIdFromComponent(DeferPC);
						if (!DeferPId.IsEmpty())
						{
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge: deferred check: resolved platform "
							            "ID '%s' via UOnlineIntegrationControllerComponent "
							            "for '%s' (addr=%s)."),
							       *DeferPId,
							       *DeferPS->GetPlayerName(),
							       DeferPC->NetConnection
							           ? *DeferPC->NetConnection->LowLevelGetRemoteAddress(true)
							           : TEXT("(no connection)"));
						}
					}

					// Derive player name and address from the live player state /
					// connection at the time the timer fires.  Captured FStrings can
					// become corrupted over multiple timer callbacks on CSS UE 5.3
					// (observed as garbage Unicode in production logs); reading from
					// the live player-state object avoids that entirely.
					const FString LiveName = DeferPS->GetPlayerName();
					const FString LiveAddr = DeferPC->NetConnection
						? DeferPC->NetConnection->LowLevelGetRemoteAddress(true)
						: TEXT("(disconnected)");

					if (!DeferPId.IsEmpty())
					{
						// Platform ID is now available – run the deferred ban check.
						if (W) { W->GetTimerManager().ClearTimer(*DeferHandle); }

						if (FBanManager::IsEnabled() && FBanManager::IsPlatformIdBanned(DeferPId))
						{
							const AFGPlayerState* DeferFGPS =
								Cast<const AFGPlayerState>(DeferPS);
							const FString DeferPlatformName =
								DeferFGPS ? DeferFGPS->GetPlayingPlatformName()
								          : TEXT("Unknown");

							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge BanSystem: kicking '%s' – deferred "
							            "platform-ID ban (id='%s', platform=%s, addr=%s)."),
							       *LiveName, *DeferPId, *DeferPlatformName, *LiveAddr);

							AGameModeBase* DeferGM = nullptr;
							if (W) { DeferGM = W->GetAuthGameMode<AGameModeBase>(); }

							if (DeferGM && DeferGM->GameSession)
							{
								const FString KickReason = Config.BanKickReason.IsEmpty()
									? TEXT("You are banned from this server.")
									: Config.BanKickReason;
								DeferGM->GameSession->KickPlayer(
									DeferPC, FText::FromString(KickReason));
							}

							// Remove from tracking to suppress the normal leave
							// notification for a kicked player.
							TrackedPlayerNames.Remove(DeferPC);

							// Mirror the ban-kick Discord message if configured.
							if (!Config.BanKickDiscordMessage.IsEmpty())
							{
								FString Notice = Config.BanKickDiscordMessage;
								Notice = Notice.Replace(TEXT("%PlayerName%"), *LiveName);

								const TArray<FString> MainChanIds =
									FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
								const TArray<FString> BanChanIds =
									FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

								for (const FString& ChanId : MainChanIds)
								{
									SendMessageToChannel(ChanId, Notice);
								}
								for (const FString& ChanId : BanChanIds)
								{
									if (!MainChanIds.Contains(ChanId))
									{
										SendMessageToChannel(ChanId, Notice);
									}
								}
							}
						}
						else
						{
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge: deferred platform-ID ban check: "
							            "'%s' not banned (id='%s', addr=%s)."),
							       *LiveName, *DeferPId, *LiveAddr);

							// Platform ID confirmed not banned.  Now run the whitelist
							// check that was deferred when OnPostLogin returned early to
							// wait for this result, then finalize the player join.
							// Note: bWhitelistRoleFetchPending is intentionally NOT
							// re-checked here — the Discord role cache has had up to
							// 60 s to populate; failing open is safer than indefinitely
							// holding a player who has already passed the ban check.
							if (!FWhitelistManager::IsEnabled()
							    || FWhitelistManager::IsWhitelisted(LiveName)
							    || (!Config.WhitelistRoleId.IsEmpty()
							        && WhitelistRoleMemberNames.Contains(LiveName.ToLower())))
							{
								NotifyPlayerJoined(DeferPC, LiveName);
							}
							else
							{
								// Player is not on the whitelist – kick fail-closed.
								UE_LOG(LogDiscordBridge, Warning,
								       TEXT("DiscordBridge Whitelist: kicking non-whitelisted "
								            "player '%s' (platform ID: '%s') – deferred "
								            "whitelist check after platform-ID resolution."),
								       *LiveName, *DeferPId);
								AGameModeBase* DeferJoinGM =
									W ? W->GetAuthGameMode<AGameModeBase>() : nullptr;
								if (DeferJoinGM && DeferJoinGM->GameSession)
								{
									const FString WLKickReason = Config.WhitelistKickReason.IsEmpty()
										? TEXT("You are not on this server's whitelist. "
										       "Contact the server admin to be added.")
										: Config.WhitelistKickReason;
									DeferJoinGM->GameSession->KickPlayer(
										DeferPC, FText::FromString(WLKickReason));
								}
								if (!Config.WhitelistKickDiscordMessage.IsEmpty())
								{
									FString WLNotice = Config.WhitelistKickDiscordMessage;
									WLNotice = WLNotice.Replace(TEXT("%PlayerName%"), *LiveName);
									const TArray<FString> WLMainChanIds =
										FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
									for (const FString& WLChanId : WLMainChanIds)
									{
										SendMessageToChannel(WLChanId, WLNotice);
									}
								}
							}
						}

						// On EOS-configured servers, a Steam64 ID resolved via the
						// UOnlineIntegrationControllerComponent is a fast-path result
						// that may later be upgraded to an EOS PUID by the EOS SDK.
						// Schedule the EOS PUID followup check so that platform-ID bans
						// stored as EOS PUIDs (e.g. added via `!ban epic add`) are also
						// enforced for this player.  Without this, a player who slipped
						// the PostLogin primary check (PlatformIdEntry was empty) and
						// was resolved here via the component as a Steam64 ID would
						// never receive the EOS PUID followup, leaving a gap if they are
						// only banned by EOS PUID.
						if (FBanManager::IsEnabled()
						    && FBanManager::GetPlatformIdType(DeferPId) == FBanManager::EPlatformIdType::Steam
						    && IsEOSPlatformConfigured())
						{
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge: deferred check resolved Steam64 ID '%s' "
							            "for '%s' on EOS server – scheduling EOS PUID followup check."),
							       *DeferPId, *LiveName);
							ScheduleEosPuidFollowupCheck(DeferPC, DeferPId, LiveName);
						}
						return;
					}

					// Platform ID still not available – retry.
					--(*DeferLeft);
					if (*DeferLeft <= 0)
					{
						if (W) { W->GetTimerManager().ClearTimer(*DeferHandle); }

						// Use live data from the still-valid player state for the log
						// and Discord messages (captured FStrings observed as garbage
						// Unicode in production logs on CSS UE 5.3 at timer-fire time).
						UE_LOG(LogDiscordBridge, Warning,
						       TEXT("DiscordBridge: deferred platform-ID check timed out "
						            "for '%s' (addr=%s) – EOS platform cannot resolve this "
						            "player's PUID after 60 s; platform-ID bans cannot be enforced. "
						            "Check for 'RegisterPlayerWithSession: Failed' and "
						            "'local user was invalid' LogGame errors at join time, which "
						            "indicate the server EOS service account did not authenticate."),
						       *LiveName, *LiveAddr);

						// Notify admins in Discord: the PUID was never resolved so
						// platform-ID bans cannot be enforced for this player.
						// Distinguish between two root causes:
						//   A. EOS platform is operational (IsPlatformOperational==true)
						//      but the server's local EOS user never authenticated within
						//      60 s (GetNumLocalPlayers()==0).  Saying "EOS not operational"
						//      here would be misleading because the platform IS configured
						//      and the session manager IS present.
						//   B. EOS platform is genuinely not operational
						//      (IsPlatformOperational==false): correct to say "unavailable".
						const AFGPlayerState* TimeoutFGPS = Cast<const AFGPlayerState>(DeferPS);
						const FString LivePlatformName = TimeoutFGPS
							? TimeoutFGPS->GetPlayingPlatformName()
							: TEXT("Unknown");

						const bool bPlatformOperationalAtTimeout = IsEOSPlatformOperational();

						// If the EOS platform is NOT operational at timeout, suppress all
						// future deferred retry timers for this server session.  Further
						// retries are pointless: the platform came up non-operational and
						// stayed that way for 60 s, meaning this custom UE build does not
						// have the EOS components.  Setting the flag prevents Discord spam
						// on every subsequent player join.
						// (The flag is NOT set when the platform is operational at timeout,
						// because that case — GetNumLocalPlayers()==0 auth delay — is a
						// transient condition that can clear for later players.)
						if (!bPlatformOperationalAtTimeout)
						{
							bPlatformIdResolutionConfirmedUnavailable = true;

							// Start a recovery polling ticker (5 s interval) so that if
							// the EOS session manager appears later (e.g. EOS auth takes
							// longer than the 60 s deferred-check window but less than the
							// EOSIntegration 120 s poll timeout), the flag is cleared and
							// new players can have their EOS PUIDs resolved.
							// The ticker does not re-run ban checks for players already
							// connected; it only enables the per-join deferred check for
							// future joins.  It stops itself once EOS becomes operational.
							// Not started if the recovery poll is already running.
							if (!EOSPlatformRecoveryPollHandle.IsValid())
							{
								UE_LOG(LogDiscordBridge, Log,
								       TEXT("DiscordBridge: EOS unavailable at deferred-check timeout – "
								            "starting EOS recovery poll (5 s interval) to detect if EOS "
								            "becomes operational and re-enable PUID lookups."));

								EOSPlatformRecoveryPollHandle = FTSTicker::GetCoreTicker().AddTicker(
									FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
									{
										if (!IsValid(this))
										{
											return false; // Subsystem destroyed; stop ticking.
										}

										if (IsEOSPlatformOperational())
										{
											// EOS has become operational.  Clear the suppression
											// flag so the next player who joins gets the deferred
											// EOS PUID check instead of being silently skipped.
											bPlatformIdResolutionConfirmedUnavailable = false;
											EOSPlatformRecoveryPollHandle.Reset();

											UE_LOG(LogDiscordBridge, Log,
											       TEXT("DiscordBridge: EOS platform became operational – "
											            "PUID deferred-check re-enabled for future player joins. "
											            "Use '%s eos' for full diagnostics."),
											       *Config.ServerInfoCommandPrefix);

											// Retroactively schedule EOS PUID ban checks for all
											// currently-connected players who joined while EOS was
											// unavailable and therefore did not receive a platform-ID
											// ban check.  For Steam64-ID players on an EOS server the
											// EOS PUID followup timer will enforce EOS-PUID-based bans
											// once EOS upgrades their handle.  For players whose EOS
											// PUID is already resolvable now, perform an immediate
											// platform-ID ban check and kick if necessary.
											int32 RecovChecksScheduled = 0;
											if (FBanManager::IsEnabled())
											{
												if (UWorld* RecovWorld = GetWorld())
												{
													AGameStateBase* RecovGS = RecovWorld->GetGameState<AGameStateBase>();
													AGameModeBase*  RecovGM = RecovWorld->GetAuthGameMode<AGameModeBase>();
													if (RecovGS)
													{
														const TArray<APlayerState*> RecovSnapshot = RecovGS->PlayerArray;
														for (APlayerState* RecovPS : RecovSnapshot)
														{
															if (!::IsValid(RecovPS)) { continue; }

															APlayerController* RecovPC =
																Cast<APlayerController>(RecovPS->GetOwner());
															if (!RecovPC || RecovPC->IsLocalController()) { continue; }

															if (!RecovPC->NetConnection ||
															    RecovPC->NetConnection->GetConnectionState() == USOCK_Closed ||
															    RecovPC->NetConnection->GetConnectionState() == USOCK_Invalid)
															{
																continue;
															}

															const FString RecovName = RecovPS->GetPlayerName();

															// Try to resolve the platform ID now that EOS is available.
															FString RecovPId = GetPlayerPlatformId(RecovPS);
															if (RecovPId.IsEmpty())
															{
																RecovPId = GetPlayerPlatformIdFromComponent(RecovPC);
															}

															if (RecovPId.IsEmpty())
															{
																// Platform ID still not available (EOS may need a
																// moment to authenticate this player's session).
																// The admin can use `!ban players` to review.
																UE_LOG(LogDiscordBridge, Log,
																       TEXT("DiscordBridge: EOS recovery – "
																            "platform ID for '%s' still empty; "
																            "retroactive check skipped."),
																       *RecovName);
																continue;
															}

															// Immediate platform-ID ban check with the
															// resolved ID.
															if (FBanManager::IsPlatformIdBanned(RecovPId))
															{
																const AFGPlayerState* RecovFGPS =
																	Cast<const AFGPlayerState>(RecovPS);
																const FString RecovPlatformName =
																	RecovFGPS ? RecovFGPS->GetPlayingPlatformName()
																	          : TEXT("Unknown");
																UE_LOG(LogDiscordBridge, Warning,
																       TEXT("DiscordBridge BanSystem: kicking '%s' – "
																            "platform-ID ban detected on EOS recovery "
																            "(id='%s', platform=%s)."),
																       *RecovName, *RecovPId, *RecovPlatformName);
																if (RecovGM && RecovGM->GameSession)
																{
																	const FString KickReason =
																		Config.BanKickReason.IsEmpty()
																		? TEXT("You are banned from this server.")
																		: Config.BanKickReason;
																	RecovGM->GameSession->KickPlayer(
																		RecovPC, FText::FromString(KickReason));
																}
																TrackedPlayerNames.Remove(RecovPC);
																if (!Config.BanKickDiscordMessage.IsEmpty())
																{
																	FString RecovNotice = Config.BanKickDiscordMessage;
																	RecovNotice = RecovNotice.Replace(
																		TEXT("%PlayerName%"), *RecovName);
																	const TArray<FString> RMain =
																		FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
																	const TArray<FString> RBan =
																		FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);
																	for (const FString& CId : RMain)
																	{
																		SendMessageToChannel(CId, RecovNotice);
																	}
																	for (const FString& CId : RBan)
																	{
																		if (!RMain.Contains(CId))
																		{
																			SendMessageToChannel(CId, RecovNotice);
																		}
																	}
																}
																++RecovChecksScheduled;
															}
															else
															{
																// Not currently banned.  If this player has a
																// Steam64 ID on an EOS-configured server, schedule
																// the EOS PUID followup so that any EOS-PUID-based
																// ban is enforced once EOS upgrades their handle.
																if (FBanManager::GetPlatformIdType(RecovPId) ==
																	    FBanManager::EPlatformIdType::Steam
																	&& IsEOSPlatformConfigured())
																{
																	UE_LOG(LogDiscordBridge, Log,
																	       TEXT("DiscordBridge: EOS recovery – "
																	            "scheduling EOS PUID followup for "
																	            "'%s' (Steam64='%s')."),
																	       *RecovName, *RecovPId);
																	ScheduleEosPuidFollowupCheck(
																		RecovPC, RecovPId, RecovName);
																	++RecovChecksScheduled;
																}
																else
																{
																	// EOS PUID already resolved or non-EOS server:
																	// the immediate ban check above is sufficient.
																	++RecovChecksScheduled;
																}
															}
														}
													}
												}
											}

											// Notify Discord so admins know the EOS platform
											// recovered and ban enforcement is active again.
											const TArray<FString> RecovMainIds =
												FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
											const TArray<FString> RecovBanIds  =
												FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);
											const FString RecovMsg = FBanManager::IsEnabled()
												? FString::Printf(
													TEXT(":white_check_mark: EOS platform is now **operational** – "
													     "EOS PUID ban enforcement re-enabled for future player joins. "
													     "Retroactive EOS PUID checks scheduled for %d connected "
													     "player(s) who joined while EOS was unavailable."),
													RecovChecksScheduled)
												: FString(
													TEXT(":white_check_mark: EOS platform is now **operational** – "
													     "EOS PUID enforcement re-enabled for future player joins."));
											for (const FString& ChanId : RecovMainIds)
											{
												SendMessageToChannel(ChanId, RecovMsg);
											}
											for (const FString& ChanId : RecovBanIds)
											{
												if (!RecovMainIds.Contains(ChanId))
												{
													SendMessageToChannel(ChanId, RecovMsg);
												}
											}

											return false; // Stop the recovery ticker.
										}

										return true; // Keep polling.
									}),
									5.0f);
							}
						}

						FString EosPuidWarn;
						FString EosBanWarn;
						if (bPlatformOperationalAtTimeout)
						{
							// EOS IS configured and operational; the local EOS service
							// account simply did not authenticate in time.  This is the
							// GetNumLocalPlayers()==0 scenario confirmed in production logs.
							EosPuidWarn = FString::Printf(
								TEXT(":warning: Player **%s** joined but their EOS Product User ID "
								     "could not be resolved after 60 s (platform: %s). "
								     "The server EOS service account may not have authenticated. "
								     "Platform-ID bans cannot be enforced for this player."),
								*LiveName, *LivePlatformName);
							EosBanWarn = FString::Printf(
								TEXT(":warning: Platform-ID bans could not be enforced for **%s** – "
								     "EOS PUID unavailable after 60 s (server EOS account not yet "
								     "authenticated). Run `%s eos` for diagnostics. "
								     "If this persists, check EOS credentials and server EOS auth."),
								*LiveName, *Config.ServerInfoCommandPrefix);
						}
						else
						{
							// EOS platform is genuinely not operational.
							EosPuidWarn = FString::Printf(
								TEXT(":warning: Player **%s** is connected (platform: %s) but their "
								     "platform ban ID cannot be retrieved – EOS platform is not operational."),
								*LiveName, *LivePlatformName);
							EosBanWarn = FString::Printf(
								TEXT(":warning: **EOS platform is unavailable** – platform-ID bans are "
								     "stored but will NOT be enforced at join time. "
								     "Run `%s eos` for diagnostics."),
								*Config.ServerInfoCommandPrefix);
						}

						const TArray<FString> MainChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
						const TArray<FString> BanChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

						for (const FString& ChanId : MainChanIds)
						{
							SendMessageToChannel(ChanId, EosPuidWarn);
							SendMessageToChannel(ChanId, EosBanWarn);
						}
						for (const FString& ChanId : BanChanIds)
						{
							if (!MainChanIds.Contains(ChanId))
							{
								SendMessageToChannel(ChanId, EosPuidWarn);
								SendMessageToChannel(ChanId, EosBanWarn);
							}
						}

						// EOS PUID could not be resolved after 60 s, so
						// platform-ID ban enforcement has already been skipped
						// (admin warned above).  Name-based bans were checked at
						// PostLogin time.  Now run the whitelist check (which was
						// deferred when OnPostLogin returned early) and finalize
						// the join so the player is announced to Discord.
						if (!FWhitelistManager::IsEnabled()
						    || FWhitelistManager::IsWhitelisted(LiveName)
						    || (!Config.WhitelistRoleId.IsEmpty()
						        && WhitelistRoleMemberNames.Contains(LiveName.ToLower())))
						{
							NotifyPlayerJoined(DeferPC, LiveName);
						}
						else
						{
							// Not whitelisted – kick fail-closed.
							UE_LOG(LogDiscordBridge, Warning,
							       TEXT("DiscordBridge Whitelist: kicking non-whitelisted "
							            "player '%s' – deferred whitelist check (platform-ID "
							            "resolution timed out)."),
							       *LiveName);
							AGameModeBase* TimeoutGM =
								W ? W->GetAuthGameMode<AGameModeBase>() : nullptr;
							if (TimeoutGM && TimeoutGM->GameSession)
							{
								const FString WLKickReason2 = Config.WhitelistKickReason.IsEmpty()
									? TEXT("You are not on this server's whitelist. "
									       "Contact the server admin to be added.")
									: Config.WhitelistKickReason;
								TimeoutGM->GameSession->KickPlayer(
									DeferPC, FText::FromString(WLKickReason2));
							}
							if (!Config.WhitelistKickDiscordMessage.IsEmpty())
							{
								FString WLNotice2 = Config.WhitelistKickDiscordMessage;
								WLNotice2 = WLNotice2.Replace(TEXT("%PlayerName%"), *LiveName);
								const TArray<FString> WLChanIds2 =
									FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
								for (const FString& WLChanId2 : WLChanIds2)
								{
									SendMessageToChannel(WLChanId2, WLNotice2);
								}
							}
						}
					}
				}),
				0.5f, true);
			// The timer is now running.  Return without calling
			// NotifyPlayerJoined() or running the whitelist check here.
			// Both are deferred until the timer callback confirms the
			// player is not platform-ID banned (or the 60 s window expires).
			// This prevents a banned player from receiving the Discord
			// "joined" message or interacting with the server while their
			// EOS PUID is still being resolved by the EOS SDK.
			return;
		}
	}

	// ── EOS PUID followup check (EOS servers only) ───────────────────────────
	// On an EOS-configured server (DefaultPlatformService=EOS), a Steam player
	// may initially present a Steam-type unique ID because the direct non-EOS
	// fast-path in GetPlayerPlatformId() fires BEFORE EOS processes the login
	// and upgrades the handle to an EOS Product User ID (PUID).
	//
	// The main ban check above already ran against that Steam64 ID.  However,
	// an admin may have banned the player by their EOS PUID (via !ban epic add
	// or !ban id add), which would not have been caught by the Steam64 check.
	// This is a real-world gap on custom UnrealEngine-CSS builds where EOS
	// components are partially loaded: the Steam direct path fires immediately,
	// but the EOS PUID takes tens of seconds to resolve.
	//
	// Conditions that trigger the followup:
	//   – Ban system is enabled.
	//   – The platform ID resolved at join time is a Steam64 format.
	//   – EOS is configured (the server runs DefaultPlatformService=EOS).
	// NOT triggered on Steam-only servers (DefaultPlatformService=Steam):
	//   on a pure Steam server the ID format never changes from Steam64, so
	//   a followup is pointless.  IsEOSConfigured()==false for Steam-only
	//   servers, which ensures this block is skipped entirely.
	// NOT triggered when EOS is unavailable: no EOS PUID will ever resolve.
	const bool bNeedsEosPuidFollowupCheck =
		FBanManager::IsEnabled()
		&& FBanManager::GetPlatformIdType(PlatformId) == FBanManager::EPlatformIdType::Steam
		&& IsEOSPlatformConfigured();

	if (bNeedsEosPuidFollowupCheck)
	{
		ScheduleEosPuidFollowupCheck(Controller, PlatformId, PlayerName);
	}

	// ── Steam64 cross-check (reverse direction) ───────────────────────────────
	// When the EOS SDK returned an EOS PUID immediately AND there are Steam64-
	// based bans, the component (UOnlineIntegrationControllerComponent) is the
	// only way to retrieve the player's native Steam64 ID for comparison.  The
	// primary ban check above only tested the EOS PUID; a player banned by their
	// Steam64 ID (e.g. via `!ban steam add`) would otherwise slip through
	// undetected on any join where EOS is fully operational at login time.
	//
	// Schedule a short-lived followup (20 retries × 0.5 s = 10 s max) that
	// waits for the component Server RPC to arrive and then cross-checks the
	// Steam64 against the ban list.  Cross-platform ID linking is also performed
	// so that both the Steam64 and the EOS PUID end up in the ban list for all
	// future joins.
	//
	// NOT scheduled when PlatformId is already a Steam64 (bNeedsEosPuidFollowupCheck
	// handles that direction), when there are no Steam64 bans, or when EOS is not
	// configured (non-EOS servers identify Steam players via the direct path anyway).
	const bool bNeedsSteam64CrossCheck =
		FBanManager::IsEnabled()
		&& FBanManager::GetPlatformIdType(PlatformId) == FBanManager::EPlatformIdType::Epic
		&& IsEOSPlatformConfigured()
		&& FBanManager::GetPlatformIdsByType(FBanManager::EPlatformIdType::Steam).Num() > 0;

	if (bNeedsSteam64CrossCheck)
	{
		ScheduleNativePlatformIdCrossCheck(Controller, PlatformId, PlayerName);
	}

	// ── Whitelist check ───────────────────────────────────────────────────────
	if (!FWhitelistManager::IsEnabled())
	{
		NotifyPlayerJoined(Controller, PlayerName);
		return;
	}

	if (FWhitelistManager::IsWhitelisted(PlayerName))
	{
		NotifyPlayerJoined(Controller, PlayerName);
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
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge Whitelist: allowing '%s' – matches a Discord member with WhitelistRoleId."),
		       *PlayerName);
		NotifyPlayerJoined(Controller, PlayerName);
		return;
	}

	// If WhitelistRoleId is configured and the role-member cache has not yet
	// been fully populated (the HTTP fetch from Discord is still in-flight),
	// defer the kick decision.  Without this guard a player who holds the
	// WhitelistRoleId would be kicked immediately after server start because
	// WhitelistRoleMemberNames is empty until the fetch response arrives.
	if (!Config.WhitelistRoleId.IsEmpty() && bWhitelistRoleFetchPending)
	{
		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge Whitelist: Discord role cache still loading – "
		            "deferring whitelist check for '%s'."), *PlayerName);

		if (UWorld* World = GetWorld())
		{
			TWeakObjectPtr<AGameModeBase>     WeakGM(GameMode);
			TWeakObjectPtr<APlayerController> WeakPC(Controller);
			TWeakObjectPtr<UWorld>            WeakWorld(World);

			// Re-check every 0.5 s, up to 20 times (10 s total).  As soon as
			// bWhitelistRoleFetchPending clears, re-run the full OnPostLogin so
			// the populated cache is used for the decision.
			TSharedRef<FTimerHandle> SharedHandle = MakeShared<FTimerHandle>();
			TSharedRef<int32>        RetriesLeft  = MakeShared<int32>(20);

			World->GetTimerManager().SetTimer(*SharedHandle,
				FTimerDelegate::CreateWeakLambda(this,
					[this, WeakGM, WeakPC, WeakWorld, SharedHandle, RetriesLeft, EntryRemoteAddr]()
				{
					UWorld* W = WeakWorld.Get();

					if (!WeakPC.IsValid())
					{
						// Player already disconnected – cancel the timer.
						UE_LOG(LogDiscordBridge, Log,
						       TEXT("DiscordBridge: deferred whitelist-role check – "
						            "player controller gone (addr=%s), aborting."),
						       *EntryRemoteAddr);
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						return;
					}

					// Guard against a net connection closed/invalidated while waiting.
					APlayerController* PC3 = WeakPC.Get();
					if (PC3->NetConnection &&
					    (PC3->NetConnection->GetConnectionState() == USOCK_Closed ||
					     PC3->NetConnection->GetConnectionState() == USOCK_Invalid))
					{
						UE_LOG(LogDiscordBridge, Warning,
						       TEXT("DiscordBridge: deferred whitelist-role check – "
						            "net connection closed/invalid (state=%d, addr=%s), aborting."),
						       static_cast<int32>(PC3->NetConnection->GetConnectionState()),
						       *EntryRemoteAddr);
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						return;
					}

					// Guard against disconnect between the IsValid() check and actual
					// use of the controller.
					if (!WeakPC->NetConnection ||
					    WeakPC->NetConnection->GetConnectionState() == USOCK_Closed ||
					    WeakPC->NetConnection->GetConnectionState() == USOCK_Invalid)
					{
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						return;
					}

					if (!bWhitelistRoleFetchPending)
					{
						// Fetch complete (success or failure) – re-run the full check.
						UE_LOG(LogDiscordBridge, Log,
						       TEXT("DiscordBridge: deferred whitelist-role check – "
						            "role fetch complete (addr=%s), re-running check."),
						       *EntryRemoteAddr);
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						OnPostLogin(WeakGM.Get(), WeakPC.Get());
						return;
					}

					--(*RetriesLeft);
					if (*RetriesLeft <= 0)
					{
						// Timeout: proceed with whatever is in the cache now.
						if (W) { W->GetTimerManager().ClearTimer(*SharedHandle); }
						UE_LOG(LogDiscordBridge, Warning,
						       TEXT("DiscordBridge Whitelist: timed out waiting for Discord role cache (addr=%s) "
						            "– proceeding with current cache state."),
						       *EntryRemoteAddr);
						OnPostLogin(WeakGM.Get(), WeakPC.Get());
					}
				}),
				0.5f, true);
		}
		return;
	}

	UE_LOG(LogDiscordBridge, Warning,
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

	// Notify Discord so admins can see the kick in the bridge channel(s).
	if (!Config.WhitelistKickDiscordMessage.IsEmpty())
	{
		FString Notice = Config.WhitelistKickDiscordMessage;
		Notice = Notice.Replace(TEXT("%PlayerName%"), *PlayerName);
		for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
		{
			SendMessageToChannel(ChanId, Notice);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Player join / leave notifications
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::NotifyPlayerJoined(APlayerController* Controller,
                                                  const FString& PlayerName)
{
	// Track this player so OnLogout can send a matching leave notification.
	TrackedPlayerNames.Add(Controller, PlayerName);

	const APlayerState*    JoinPS     = Controller ? Controller->GetPlayerState<APlayerState>() : nullptr;
	const AFGPlayerState*  JoinFGPS   = Cast<const AFGPlayerState>(JoinPS);
	// Platform display name (e.g. "Steam", "Epic Games Store") – used both in
	// the PlayerJoinMessage %Platform% placeholder and in the EOS warning below.
	const FString JoinPlatformName    = JoinFGPS ? JoinFGPS->GetPlayingPlatformName() : TEXT("Unknown");
	// Also track the platform for the %Platform% placeholder in PlayerLeaveMessage.
	TrackedPlayerPlatforms.Add(Controller, JoinPlatformName);

	const FString JoinPlatformId = GetPlayerPlatformId(JoinPS);
	const FString JoinAddr = (Controller && Controller->NetConnection)
		? Controller->NetConnection->LowLevelGetRemoteAddress(true)
		: TEXT("(no connection)");
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Player '%s' joined the server (platform='%s', platformId='%s', addr=%s)."),
	       *PlayerName, *JoinPlatformName, *JoinPlatformId, *JoinAddr);

	if (!Config.PlayerJoinMessage.IsEmpty())
	{
		FString Notice = Config.PlayerJoinMessage;
		Notice = Notice.Replace(TEXT("%PlayerName%"), *PlayerName);
		Notice = Notice.Replace(TEXT("%Platform%"),   *JoinPlatformName);
		for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
		{
			SendMessageToChannel(ChanId, Notice);
		}
	}

	// Notify other mods via GameplayEvents that a player joined.
	DispatchDiscordGameplayEvent(FDiscordGameplayTags::PlayerJoined(), PlayerName);

	// ── ReliableMessaging relay handler ──────────────────────────────────────
	// Register a per-player handler so client-side mods can post to Discord by
	// calling UReliableMessagingPlayerComponent::SendMessage(
	//     EDiscordRelayChannel::ForwardToDiscord, UTF8Payload) on the client.
	// The server component is looked up via the static accessor and may be null
	// if the ReliableMessaging subsystem has not yet established the per-player
	// connection (e.g. on some listen-server configurations).
	if (UReliableMessagingPlayerComponent* RMComp =
	        UReliableMessagingPlayerComponent::GetFromPlayer(Controller))
	{
		RMComp->RegisterMessageHandler(
			EDiscordRelayChannel::ForwardToDiscord,
			UReliableMessagingPlayerComponent::FOnBulkDataReplicationPayloadReceived::CreateWeakLambda(
				this,
				[this, PlayerName](TArray<uint8>&& Payload)
				{
					if (Payload.IsEmpty() || !bGatewayReady)
					{
						return;
					}

					// Ensure null-termination so UTF8_TO_TCHAR reads a bounded string.
					Payload.Add(0);
					const FString Message = FString(UTF8_TO_TCHAR(
					    reinterpret_cast<const ANSICHAR*>(Payload.GetData())));
					if (Message.IsEmpty())
					{
						return;
					}

					UE_LOG(LogDiscordBridge, Log,
					       TEXT("DiscordBridge: ReliableMessaging relay from '%s': %s"),
					       *PlayerName, *Message);

					// Apply the same GameToDiscordFormat used for in-game chat
					// so the Discord message style is consistent.
					FString Formatted = Config.GameToDiscordFormat;
					Formatted = Formatted.Replace(TEXT("%ServerName%"), *Config.ServerName);
					Formatted = Formatted.Replace(TEXT("%PlayerName%"), *PlayerName);
					Formatted = Formatted.Replace(TEXT("%Message%"),    *Message);

					for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
					{
						SendMessageToChannel(ChanId, Formatted);
					}
				}));
	}

	// ── EOS-unavailable warning ───────────────────────────────────────────────
	// When the EOS platform is not operational the player's platform ban ID
	// cannot be retrieved and platform-ID ban enforcement is inactive at join
	// time.  Notify admins in Discord so they are aware of this for every
	// player that connects, matching the information shown by `!ban id lookup`.
	if (!IsEOSPlatformOperational() && JoinPlatformId.IsEmpty())
	{
		// JoinPlatformName is already fetched at the top of this function.
		const FString EosPuidWarn = FString::Printf(
			TEXT(":warning: Player **%s** is connected (platform: %s) but their "
			     "platform ban ID cannot be retrieved – EOS platform is not operational."),
			*PlayerName, *JoinPlatformName);
		const FString EosBanWarn = FString::Printf(
			TEXT(":warning: **EOS platform is unavailable** – platform-ID bans are "
			     "stored but will NOT be enforced at join time. "
			     "Run `%s eos` for diagnostics."),
			*Config.ServerInfoCommandPrefix);

		const TArray<FString> MainChanIds = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
		const TArray<FString> BanChanIds  = FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

		for (const FString& ChanId : MainChanIds)
		{
			SendMessageToChannel(ChanId, EosPuidWarn);
			SendMessageToChannel(ChanId, EosBanWarn);
		}
		for (const FString& ChanId : BanChanIds)
		{
			if (!MainChanIds.Contains(ChanId))
			{
				SendMessageToChannel(ChanId, EosPuidWarn);
				SendMessageToChannel(ChanId, EosBanWarn);
			}
		}
	}
}

void UDiscordBridgeSubsystem::OnLogout(AGameModeBase* /*GameMode*/,
                                        AController* Controller)
{
	APlayerController* PC = Cast<APlayerController>(Controller);
	if (!PC || PC->IsLocalController())
	{
		return;
	}

	// During server shutdown the world tears down all actors and fires Logout
	// for every connected player.  The server-offline notification already covers
	// this case, so suppress individual leave messages during world teardown to
	// avoid a flood of "left" messages immediately before "server offline".
	if (UWorld* World = GetWorld())
	{
		if (World->bIsTearingDown)
		{
			TrackedPlayerNames.Remove(PC);
			TrackedPlayerPlatforms.Remove(PC);
			return;
		}
	}

	// Only send a leave notification for players who were actually tracked
	// (i.e. passed all ban/whitelist checks).  Kicked players are not tracked.
	FString PlayerName;
	if (FString* Found = TrackedPlayerNames.Find(PC))
	{
		PlayerName = *Found;
		TrackedPlayerNames.Remove(PC);
	}

	// Retrieve and remove the stored platform name; falls back to empty string
	// when no entry exists (e.g. the player was tracked before this feature).
	FString PlayerPlatform;
	if (FString* FoundPlatform = TrackedPlayerPlatforms.Find(PC))
	{
		PlayerPlatform = *FoundPlatform;
		TrackedPlayerPlatforms.Remove(PC);
	}

	if (PlayerName.IsEmpty())
	{
		return;
	}

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Player '%s' left the server."), *PlayerName);

	// Notify other mods via GameplayEvents that a tracked player left.
	DispatchDiscordGameplayEvent(FDiscordGameplayTags::PlayerLeft(), PlayerName);

	if (Config.PlayerLeaveMessage.IsEmpty())
	{
		return;
	}

	FString Notice = Config.PlayerLeaveMessage;
	Notice = Notice.Replace(TEXT("%PlayerName%"), *PlayerName);
	Notice = Notice.Replace(TEXT("%Platform%"),   *PlayerPlatform);
	for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
	{
		SendMessageToChannel(ChanId, Notice);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Server-info Discord commands  (!server players / status / help)
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleServerInfoCommand(const FString& SubCommand,
                                                       const FString& ResponseChannelId)
{
	// Normalise the verb so both "players" and "online" work, and so that an
	// empty sub-command still produces the status summary by default.
	const FString Verb = SubCommand.TrimStartAndEnd().ToLower();

	FString Response;

	if (Verb == TEXT("players") || Verb == TEXT("online"))
	{
		// Collect connected player names from the game state.
		TArray<FString> Names;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				for (APlayerState* PS : GS->PlayerArray)
				{
					if (!PS) { continue; }
					const FString Name = PS->GetPlayerName();
					if (!Name.IsEmpty())
					{
						Names.Add(Name);
					}
				}
			}
		}

		if (Names.Num() == 0)
		{
			Response = TEXT(":zzz: No players are currently online.");
		}
		else
		{
			FString List;
			for (const FString& Name : Names)
			{
				List += TEXT("\n• ") + Name;
			}
			Response = FString::Printf(
				TEXT(":busts_in_silhouette: **%d** player%s online:%s"),
				Names.Num(),
				Names.Num() == 1 ? TEXT("") : TEXT("s"),
				*List);
		}
	}
	else if (Verb == TEXT("status") || Verb.IsEmpty())
	{
		int32 PlayerCount = 0;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				PlayerCount = GS->PlayerArray.Num();
			}
		}

		const FString ServerLabel = Config.ServerName.IsEmpty()
			? TEXT("Satisfactory Server")
			: Config.ServerName;

		Response = FString::Printf(
			TEXT(":green_circle: **%s** is **online**\n"
			     ":busts_in_silhouette: Players online: **%d**"),
			*ServerLabel,
			PlayerCount);

		// Append an EOS warning if the platform is currently unavailable.
		// Ban-by-platform-ID and per-player EOS diagnostics depend on a live
		// EOS session interface; alert operators early so they can investigate.
		if (!IsEOSPlatformOperational())
		{
			Response += FString::Printf(
				TEXT("\n:warning: EOS platform unavailable – ban-by-ID will not function. "
				     "Use `%s eos` for diagnostics."),
				*Config.ServerInfoCommandPrefix);
		}
	}
	else if (Verb == TEXT("eos"))
	{
		// BuildPlatformDiagnostics() uses OnlineIntegration and GConfig directly.
		const int32 PlatformIdCount = FBanManager::GetAllPlatformIds().Num();
		const FString BanLine = FBanManager::IsEnabled()
			? FString::Printf(
			      TEXT(":hammer: Ban system: **enabled** — %d platform ID%s currently banned (Steam & Epic)"),
			      PlatformIdCount, PlatformIdCount == 1 ? TEXT("") : TEXT("s"))
			: FString::Printf(
			      TEXT(":unlock: Ban system: **disabled** — %d platform ID%s in list (not enforced)"),
			      PlatformIdCount, PlatformIdCount == 1 ? TEXT("") : TEXT("s"));
		Response = BuildPlatformDiagnostics() + TEXT("\n**Ban system:**\n") + BanLine;
	}
	else if (Verb == TEXT("help"))
	{
		// Build the help text dynamically based on which features are configured.
		FString Help = TEXT(":information_source: **DiscordBridge commands**\n");

		if (!Config.ServerInfoCommandPrefix.IsEmpty())
		{
			Help += TEXT("\n**Server info** (anyone can use):\n");
			Help += FString::Printf(TEXT("`%s players` – list online players\n"), *Config.ServerInfoCommandPrefix);
			Help += FString::Printf(TEXT("`%s status` – server status and player count\n"), *Config.ServerInfoCommandPrefix);
			Help += FString::Printf(TEXT("`%s eos` – EOS platform diagnostics\n"), *Config.ServerInfoCommandPrefix);
			Help += FString::Printf(TEXT("`%s help` – show this message\n"), *Config.ServerInfoCommandPrefix);
		}

		if (!Config.WhitelistCommandPrefix.IsEmpty() && !Config.WhitelistCommandRoleId.IsEmpty())
		{
			Help += TEXT("\n**Whitelist** (requires whitelist-admin role):\n");
			Help += FString::Printf(TEXT("`%s on/off` – enable or disable the whitelist\n"), *Config.WhitelistCommandPrefix);
			Help += FString::Printf(TEXT("`%s add/remove <name>` – manage players\n"), *Config.WhitelistCommandPrefix);
			Help += FString::Printf(TEXT("`%s list` – list whitelisted players\n"), *Config.WhitelistCommandPrefix);
			Help += FString::Printf(TEXT("`%s status` – show whitelist/ban state\n"), *Config.WhitelistCommandPrefix);
		}

		if (!Config.BanCommandPrefix.IsEmpty() && !Config.BanCommandRoleId.IsEmpty() && Config.bBanCommandsEnabled)
		{
			Help += TEXT("\n**Ban system** (requires ban-admin role):\n");
			Help += FString::Printf(TEXT("`%s players` – list connected players with platform + ban ID\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s check <name>` – check if a player is banned\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s add <name>` – ban by name (also auto-bans by platform ID if connected)\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s remove <name>` – remove name ban\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s id lookup <name>` – find a connected player's platform ban ID\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s id add/remove <id>` – manage bans by platform ID (Steam64 or EOS PUID)\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s list` – list banned player names\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s on/off` – enable or disable ban enforcement\n"), *Config.BanCommandPrefix);
			Help += FString::Printf(TEXT("`%s status` – show ban/whitelist/EOS state\n"), *Config.BanCommandPrefix);
		}

		if (!Config.KickCommandPrefix.IsEmpty() && !Config.KickCommandRoleId.IsEmpty() && Config.bKickCommandsEnabled)
		{
			Help += TEXT("\n**Kick** (requires kick-admin role):\n");
			Help += FString::Printf(TEXT("`%s <name>` – kick a player (no ban, can reconnect immediately)\n"), *Config.KickCommandPrefix);
			Help += FString::Printf(TEXT("`%s <name> <reason>` – kick with a custom reason\n"), *Config.KickCommandPrefix);
		}

		if (!Config.ServerControlCommandPrefix.IsEmpty() && !Config.ServerControlCommandRoleId.IsEmpty())
		{
			Help += TEXT("\n**Server control** (requires server-admin role):\n");
			Help += FString::Printf(TEXT("`%s start` – confirm the server is online\n"), *Config.ServerControlCommandPrefix);
			Help += FString::Printf(TEXT("`%s stop` – gracefully shut down the server\n"), *Config.ServerControlCommandPrefix);
			Help += FString::Printf(TEXT("`%s restart` – restart the server\n"), *Config.ServerControlCommandPrefix);
			Help += FString::Printf(TEXT("`%s ticket-panel` – post the ticket button panel\n"), *Config.ServerControlCommandPrefix);
		}

		Response = Help.TrimEnd();
	}
	else
	{
		Response = FString::Printf(
			TEXT(":question: Unknown sub-command `%s`. "
			     "Try `%s status`, `%s players`, `%s eos`, or `%s help`."),
			*SubCommand,
			*Config.ServerInfoCommandPrefix,
			*Config.ServerInfoCommandPrefix,
			*Config.ServerInfoCommandPrefix,
			*Config.ServerInfoCommandPrefix);
	}

	SendMessageToChannel(ResponseChannelId, Response);
}

// ─────────────────────────────────────────────────────────────────────────────
// Admin server control command handler  (!admin start / stop / restart)
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleServerControlCommand(const FString& SubCommand,
                                                          const FString& DiscordUsername,
                                                          const FString& ResponseChannelId)
{
	const FString Verb = SubCommand.TrimStartAndEnd().ToLower();

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Server control command '%s' from '%s'"),
	       *Verb, *DiscordUsername);

	if (Verb == TEXT("start"))
	{
		// The server is already running (this code only executes on a live server).
		const FString ServerLabel = Config.ServerName.IsEmpty()
			? TEXT("The server")
			: FString::Printf(TEXT("**%s**"), *Config.ServerName);
		SendMessageToChannel(ResponseChannelId,
		    FString::Printf(TEXT(":green_circle: %s is already **online**."), *ServerLabel));
		return;
	}

	if (Verb == TEXT("stop"))
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Server STOP requested by Discord user '%s'."),
		       *DiscordUsername);

		const FString ServerLabel = Config.ServerName.IsEmpty()
			? TEXT("Server")
			: Config.ServerName;
		SendMessageToChannel(ResponseChannelId,
		    FString::Printf(
		        TEXT(":red_circle: **%s** is shutting down... (requested by %s)\n"
		             ":information_source: The server process will exit with code **0**. "
		             "A process supervisor configured with `Restart=on-failure` (systemd) "
		             "or `--restart on-failure` (Docker) will **not** restart it."),
		        *ServerLabel, *DiscordUsername));

		// Defer the actual exit by 3 seconds so the HTTP message has time to
		// be delivered to Discord before the process terminates.
		// Exit code 0 signals a clean, intentional stop.  Process supervisors
		// that use Restart=on-failure / --restart on-failure will NOT restart.
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [](float) -> bool
			{
				FGenericPlatformMisc::RequestExit(/*bForce=*/false);
				return false; // one-shot
			}),
			3.0f);
		return;
	}

	if (Verb == TEXT("restart"))
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Server RESTART requested by Discord user '%s'."),
		       *DiscordUsername);

		const FString ServerLabel = Config.ServerName.IsEmpty()
			? TEXT("Server")
			: Config.ServerName;
		SendMessageToChannel(ResponseChannelId,
		    FString::Printf(
		        TEXT(":arrows_counterclockwise: **%s** is restarting... (requested by %s)\n"
		             ":information_source: The server process will exit with code **1**. "
		             "Make sure your process supervisor uses `Restart=on-failure` (systemd) "
		             "or `--restart on-failure` (Docker) so it restarts on that code. "
		             "If it uses `Restart=always`, it will also restart after `!admin stop` — "
		             "switch to `Restart=on-failure` to fix that."),
		        *ServerLabel, *DiscordUsername));

		// Defer the exit so the HTTP message is delivered first.
		// Exit code 1 signals an intentional restart request.  Process supervisors
		// configured with Restart=on-failure / --restart on-failure will restart
		// the server automatically, while a plain stop (exit 0) will not.
		FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [](float) -> bool
			{
				FGenericPlatformMisc::RequestExitWithStatus(/*bForce=*/false, /*ReturnCode=*/1);
				return false; // one-shot
			}),
			3.0f);
		return;
	}

	if (Verb == TEXT("ticket-panel"))
	{
		// Post the interactive ticket panel (message with clickable buttons) to
		// the configured TicketPanelChannelId channel (or the main channel when
		// TicketPanelChannelId is not set).
		const TArray<FString> PanelChannels = Config.TicketPanelChannelId.IsEmpty()
			? FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId)
			: FDiscordBridgeConfig::ParseChannelIds(Config.TicketPanelChannelId);

		for (const FString& PanelChan : PanelChannels)
		{
			PostTicketPanel(PanelChan, ResponseChannelId);
		}
		return;
	}

	// Unknown sub-command – print usage.
	SendMessageToChannel(ResponseChannelId,
	    FString::Printf(
	        TEXT(":question: Unknown sub-command `%s`. "
	             "Available admin commands: `%s start`, `%s stop`, `%s restart`, `%s ticket-panel`."),
	        *SubCommand,
	        *Config.ServerControlCommandPrefix,
	        *Config.ServerControlCommandPrefix,
	        *Config.ServerControlCommandPrefix,
	        *Config.ServerControlCommandPrefix));
}

void UDiscordBridgeSubsystem::HandleWhitelistCommand(const FString& SubCommand,
                                                      const FString& DiscordUsername,
                                                      const FString& ResponseChannelId)
{
	UE_LOG(LogDiscordBridge, Log,
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
			                "Set it to the Discord ID of the whitelist role.");
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
	// If no explicit response channel was provided, fall back to the first main channel.
	const TArray<FString> FallbackChannels = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
	const FString ReplyChannel = ResponseChannelId.IsEmpty()
		? (FallbackChannels.Num() > 0 ? FallbackChannels[0] : TEXT(""))
		: ResponseChannelId;
	SendMessageToChannel(ReplyChannel, Response);
}

void UDiscordBridgeSubsystem::ModifyDiscordRole(const FString& UserId, const FString& RoleId, bool bGrant)
{
	if (RoleId.IsEmpty() || GuildId.IsEmpty() || Config.BotToken.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Warning,
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
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: Role %s request failed for user '%s'."),
				       bGrantCopy ? TEXT("grant") : TEXT("revoke"), *UserIdCopy);
				return;
			}
			// 204 No Content is the success response for both PUT and DELETE role endpoints.
			if (Resp->GetResponseCode() != 204)
			{
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge: Role %s for user '%s' returned HTTP %d: %s"),
				       bGrantCopy ? TEXT("grant") : TEXT("revoke"), *UserIdCopy,
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
			else
			{
				UE_LOG(LogDiscordBridge, Log,
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

	UE_LOG(LogDiscordBridge, Verbose,
	       TEXT("DiscordBridge: Whitelist role cache updated for user '%s' (%s)."),
	       *UserId, *FString::Join(Names, TEXT(", ")));
}

void UDiscordBridgeSubsystem::FetchWhitelistRoleMembers(int32 RetryCount)
{
	if (Config.WhitelistRoleId.IsEmpty() || GuildId.IsEmpty() || Config.BotToken.IsEmpty())
	{
		return;
	}

	// Mark the fetch as pending so that OnPostLogin can defer whitelist kicks
	// for players who join before this HTTP response arrives.
	bWhitelistRoleFetchPending = true;

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

	static constexpr int32 MaxFetchRetries = 3;

	Request->OnProcessRequestComplete().BindWeakLambda(
		this,
		[this, RetryCount](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				// Network / SSL failure – retry with exponential back-off if
				// we have not yet exhausted the retry budget.  Keep
				// bWhitelistRoleFetchPending = true so OnPostLogin continues
				// to defer whitelist kicks while the retry is in-flight.
				if (RetryCount < MaxFetchRetries)
				{
					const float RetryDelay = static_cast<float>(RetryCount + 1) * 10.0f;
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge: FetchWhitelistRoleMembers failed – "
					            "retrying in %.0f s (attempt %d of %d)."),
					       RetryDelay, RetryCount + 1, MaxFetchRetries);

					FTSTicker::GetCoreTicker().AddTicker(
						FTickerDelegate::CreateWeakLambda(this, [this, RetryCount](float) -> bool
						{
							FetchWhitelistRoleMembers(RetryCount + 1);
							return false; // one-shot
						}),
						RetryDelay);
				}
				else
				{
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge: FetchWhitelistRoleMembers failed after %d retries. "
					            "Players with WhitelistRoleId will not be automatically whitelisted "
					            "until the next reconnect."),
					       MaxFetchRetries);
					bWhitelistRoleFetchPending = false;
				}
				return;
			}

			// Always clear the pending flag once a valid HTTP response arrives,
			// regardless of the response code.
			bWhitelistRoleFetchPending = false;

			if (Resp->GetResponseCode() != 200)
			{
				UE_LOG(LogDiscordBridge, Warning,
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
				UE_LOG(LogDiscordBridge, Warning,
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

			UE_LOG(LogDiscordBridge, Log,
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
	UE_LOG(LogDiscordBridge, Log,
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
			Response = TEXT(":warning: Usage: `!ban add <PlayerName>`\n"
			                "Bans the player by name **and** automatically bans their platform ID "
			                "(Steam or Epic Games) if they are currently connected.");
		}
		else if (FBanManager::BanPlayer(Arg))
		{
			// After adding the name ban, also try to auto-ban by platform ID if
			// the player is currently connected.  This means admins no longer
			// need to run `!ban id lookup` + `!ban id add` as separate steps.
			FString AutoPlatformId;
			FString AutoPlatformName;
			bool bPlayerConnected  = false;
			bool bPuidResolving    = false;
			bool bAutoIdBanned     = false;
			bool bIdAlreadyBanned  = false;

			if (UWorld* World = GetWorld())
			{
				if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
				{
					const FString AddTarget = Arg.ToLower();
					for (APlayerState* ConnPS : GS->PlayerArray)
					{
						if (!ConnPS) { continue; }
						if (ConnPS->GetPlayerName().ToLower() != AddTarget) { continue; }
						bPlayerConnected = true;
						AutoPlatformId   = GetPlayerPlatformId(ConnPS);
						const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
						AutoPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
						break;
					}
				}
			}

			if (bPlayerConnected && !AutoPlatformId.IsEmpty())
			{
				if (FBanManager::BanPlatformId(AutoPlatformId))
				{
					bAutoIdBanned = true;
				}
				else
				{
					bIdAlreadyBanned = true; // platform ID was already on the ban list
				}
			}
			else if (bPlayerConnected && AutoPlatformId.IsEmpty())
			{
				bPuidResolving = true;
			}

			// Build a note about the platform-ID ban result.
			FString PlatformIdNote;
			const bool bEosOk = IsEOSPlatformOperational();
			if (bAutoIdBanned)
			{
				PlatformIdNote = FString::Printf(
					TEXT("\n:satellite: Platform ID `%s` (%s) also banned automatically."),
					*AutoPlatformId, *AutoPlatformName);
			}
			else if (bIdAlreadyBanned)
			{
				PlatformIdNote = FString::Printf(
					TEXT("\n:satellite: Platform ID `%s` (%s) was already on the ban list."),
					*AutoPlatformId, *AutoPlatformName);
			}
			else if (bPuidResolving && bEosOk)
			{
				PlatformIdNote = FString::Printf(
					TEXT("\n:warning: Player is connected (%s) but their platform ban ID is not yet available. "
					     "Use `%s id lookup %s` then `%s id add <id>` once it resolves."),
					*AutoPlatformName,
					*Config.BanCommandPrefix, *Arg, *Config.BanCommandPrefix);
			}
			else if (bPuidResolving && !bEosOk)
			{
				PlatformIdNote = FString::Printf(
					TEXT("\n:warning: EOS platform is unavailable – could not auto-ban by platform ID. "
					     "Run `%s eos` for diagnostics."),
					*Config.ServerInfoCommandPrefix);
			}
			else // !bPlayerConnected
			{
				PlatformIdNote = TEXT("\n:information_source: Player is not currently connected — "
				                     "banned by name only. Use `!ban id add <id>` "
				                     "to also ban their platform ID.");
			}

			if (FBanManager::IsEnabled())
			{
				const int32 Kicked = KickConnectedBannedPlayers(Arg);
				Response = FString::Printf(
					TEXT(":hammer: **%s** has been banned from the server.%s%s"),
					*Arg,
					Kicked > 0 ? TEXT(" They have been kicked.") : TEXT(""),
					*PlatformIdNote);
			}
			else
			{
				Response = FString::Printf(
					TEXT(":hammer: **%s** has been added to the ban list.\n"
					     ":warning: The ban system is currently **disabled** — run `!ban on` to enforce bans.%s"),
					*Arg, *PlatformIdNote);
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
		// Platform status – reports EOSIntegration subsystem health, the
		// active platform (Steam or EOS), and the counts of Steam64 ID bans
		// vs EOS PUID bans so admins can see at a glance which ban types are
		// stored and which will actually be enforced at player join time.
		const int32 SteamBanCount = FBanManager::GetPlatformIdsByType(
			FBanManager::EPlatformIdType::Steam).Num();
		const int32 EosPuidBanCount = FBanManager::GetPlatformIdsByType(
			FBanManager::EPlatformIdType::Epic).Num();
		const bool bPlatformOk = IsEOSPlatformOperational();
		FString EosPlatformState;
		if (!bPlatformOk)
		{
			if (IsEOSPlatformConfigured())
			{
				EosPlatformState = FString::Printf(
					TEXT(":warning: Platform (EOS): **degraded** – use `%s eos` for diagnostics\n"
					     "  Steam64 bans: %d **enforced via fallback** | EOS PUID bans: %d stored, **inactive**"),
					*Config.ServerInfoCommandPrefix, SteamBanCount, EosPuidBanCount);
			}
			else
			{
				EosPlatformState = FString::Printf(
					TEXT(":no_entry_sign: Platform: **not configured** – all platform-ID bans inactive\n"
					     "  Steam64 bans: %d stored (not enforced) | EOS PUID bans: %d stored (not enforced)"),
					SteamBanCount, EosPuidBanCount);
			}
		}
		else if (IsSteamPlatformConfigured())
		{
			FString PlatDetail = FString::Printf(
				TEXT(":satellite: Platform (Steam): **active**\n"
				     "  Steam64 bans: %d enforced"),
				SteamBanCount);
			if (EosPuidBanCount > 0)
			{
				PlatDetail += FString::Printf(
					TEXT(" | EOS PUID bans: %d stored (:information_source: EOS PUIDs are not used on Steam-mode servers – these bans will not be enforced)"),
					EosPuidBanCount);
			}
			EosPlatformState = PlatDetail;
		}
		else
		{
			FString PlatDetail = FString::Printf(
				TEXT(":satellite: Platform (EOS): **active** – EOS PUIDs cover Steam & Epic accounts\n"
				     "  EOS PUID bans: %d enforced"),
				EosPuidBanCount);
			if (SteamBanCount > 0)
			{
				PlatDetail += FString::Printf(
					TEXT(" | Steam64 bans: %d stored (:warning: Steam64 IDs may not match EOS PUIDs – use `%s id lookup <name>` to get the correct EOS PUIDs to ban)"),
					SteamBanCount, *Config.BanCommandPrefix);
			}
			EosPlatformState = PlatDetail;
		}
		Response = FString::Printf(
			TEXT("**Server access control (each system is independent):**\n%s\n%s\n%s\n"
			     ":clock3: Periodic ban scan: %s"),
			*BanState, *WhitelistState, *EosPlatformState,
			Config.BanScanIntervalSeconds > 0.0f
				? *FString::Printf(TEXT("**every %.0f s** (catches out-of-band ban-list edits)"),
				                   FMath::Max(Config.BanScanIntervalSeconds, 30.0f))
				: TEXT("**disabled** (set `BanScanIntervalSeconds` in `DefaultBan.ini` to enable)"));
	}
	else if (Verb == TEXT("players"))
	{
		// List all currently connected players with their platform and ban ID.
		// This is the primary "player lookup" command: admins can see everyone who
		// is online, which platform they are on (Steam/Epic/None), and their
		// platform ban ID — all in one Discord message.  Use the ID with
		// `!ban id add <id>` (or `!ban steam add <id>`) to issue a platform-ID ban.
		TArray<FString> Lines;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				for (APlayerState* ConnPS : GS->PlayerArray)
				{
					if (!ConnPS) { continue; }
					const FString ConnName = ConnPS->GetPlayerName();
					const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
					const FString PlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
					const FString PlatformId   = GetPlayerPlatformId(ConnPS);

					const FString NameBanFlag = FBanManager::IsBanned(ConnName)
					                            ? TEXT(" :hammer: BANNED") : TEXT("");
					const FString IdBanFlag   = (!PlatformId.IsEmpty() && FBanManager::IsPlatformIdBanned(PlatformId))
					                            ? TEXT(" :hammer: BANNED (ID)") : TEXT("");

					if (PlatformId.IsEmpty())
					{
						Lines.Add(FString::Printf(TEXT("• **%s** | %s | ID: *resolving\xe2\x80\xa6*%s%s"),
							*ConnName, *PlatformName, *NameBanFlag, *IdBanFlag));
					}
					else
					{
						const FString IdTypeLabel = FBanManager::GetPlatformTypeLabel(PlatformId);
						Lines.Add(FString::Printf(TEXT("• **%s** | %s | %s: `%s`%s%s"),
							*ConnName, *PlatformName, *IdTypeLabel, *PlatformId,
							*NameBanFlag, *IdBanFlag));
					}
				}
			}
		}

		if (Lines.Num() == 0)
		{
			Response = TEXT(":zzz: No players are currently connected.");
		}
		else
		{
			const FString EosNote = IsEOSPlatformOperational()
				? TEXT("")
				: FString::Printf(
				      TEXT("\n:warning: EOS unavailable – platform IDs may not resolve. Use `%s eos` for diagnostics."),
				      *Config.ServerInfoCommandPrefix);
			Response = FString::Printf(
				TEXT(":busts_in_silhouette: **%d** player%s connected:\n%s%s"),
				Lines.Num(), Lines.Num() == 1 ? TEXT("") : TEXT("s"),
				*FString::Join(Lines, TEXT("\n")), *EosNote);
		}
	}
	else if (Verb == TEXT("check"))
	{
		// Check whether a player name is on the ban list, and also check their
		// current PUID if they are connected right now.
		// Usage: !ban check <PlayerName>
		if (Arg.IsEmpty())
		{
			Response = TEXT(":warning: Usage: `!ban check <PlayerName>`\n"
			                "Checks if the named player is banned by name or by platform ID "
			                "(if they are currently connected).");
		}
		else
		{
			const bool bNameBanned = FBanManager::IsBanned(Arg);

			// If the player is currently connected, also check their platform ban ID.
			FString ConnectedPlatformId;
			FString ConnectedPlatformName;
			bool bConnected = false;
			const FString CheckTarget = Arg.ToLower();
			if (UWorld* World = GetWorld())
			{
				if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
				{
					for (APlayerState* ConnPS : GS->PlayerArray)
					{
						if (!ConnPS) { continue; }
						if (ConnPS->GetPlayerName().ToLower() != CheckTarget) { continue; }
						ConnectedPlatformId   = GetPlayerPlatformId(ConnPS);
						const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
						ConnectedPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
						bConnected = true;
						break;
					}
				}
			}

			const bool bIdBanned = !ConnectedPlatformId.IsEmpty()
			                       && FBanManager::IsPlatformIdBanned(ConnectedPlatformId);

			FString StatusLine;
			if (bNameBanned && bIdBanned)
			{
				StatusLine = TEXT(":hammer: **BANNED** by both name and platform ID.");
			}
			else if (bNameBanned)
			{
				StatusLine = TEXT(":hammer: **BANNED** by name.");
			}
			else if (bIdBanned)
			{
				StatusLine = FString::Printf(
					TEXT(":hammer: **BANNED** by platform ID (`%s`)."),
					*ConnectedPlatformId);
			}
			else
			{
				StatusLine = TEXT(":white_check_mark: **Not banned.**");
			}

			FString ConnectedLine;
			if (bConnected)
			{
				if (ConnectedPlatformId.IsEmpty())
				{
					ConnectedLine = FString::Printf(
						TEXT("\n:satellite: Currently **connected** (platform: %s) – platform ID resolving\xe2\x80\xa6"),
						*ConnectedPlatformName);
				}
				else
				{
					const FString ConnIdLabel = FBanManager::GetPlatformTypeLabel(ConnectedPlatformId);
					ConnectedLine = FString::Printf(
						TEXT("\n:satellite: Currently **connected** (platform: %s, %s: `%s`)"),
						*ConnectedPlatformName, *ConnIdLabel, *ConnectedPlatformId);
				}
			}
			else
			{
				ConnectedLine = TEXT("\n:zzz: Not currently connected.");
			}

			Response = FString::Printf(
				TEXT(":mag_right: **Ban check for '%s':**\n%s%s"),
				*Arg, *StatusLine, *ConnectedLine);
		}
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
			                "Set it to the Discord ID of the ban admin role you want to grant/revoke.");
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
	else if (Verb == TEXT("id"))
	{
		// Platform ID (Steam / Epic Games) ban sub-commands.
		// EOS is the platform service on CSS UnrealEngine-CSS: both Steam and
		// EGS players are assigned an EOS Product User ID (PUID) at login.
		// The PUID is the stable identifier used in the platform_ids ban list.
		// Usage: !ban id lookup <name> | !ban id add <id> | !ban id remove <id> | !ban id list
		FString IdVerb, TargetId;
		if (!Arg.Split(TEXT(" "), &IdVerb, &TargetId, ESearchCase::IgnoreCase))
		{
			IdVerb   = Arg.TrimStartAndEnd();
			TargetId = TEXT("");
		}
		IdVerb   = IdVerb.TrimStartAndEnd().ToLower();
		TargetId = FBanManager::NormalizePlatformId(TargetId.TrimStartAndEnd());

		// EOS warning prefix – appended to any id sub-command response when EOS
		// is unavailable so admins are never left wondering why bans don't stick.
		const FString EosWarn = IsEOSPlatformOperational()
			? TEXT("")
			: FString::Printf(
			      TEXT("\n:warning: **EOS platform is unavailable** – platform-ID bans are "
			           "stored but will NOT be enforced at join time. "
			           "Run `%s eos` for diagnostics."),
			      *Config.ServerInfoCommandPrefix);

		if (IdVerb == TEXT("lookup"))
		{
			// Look up the platform ban ID (EOS PUID or Steam64 ID) of a connected
			// player by in-game name.  Primary workflow: find the player in-game →
			// lookup → use the suggested ban command from the response.
			// On EOS-mode servers the ID is an EOS PUID (covers Steam + Epic);
			// on Steam-mode servers it is a Steam64 ID.
			if (TargetId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban id lookup <PlayerName>`\n"
				                "Returns the platform ban ID of the named player if they are "
				                "currently connected — an **EOS PUID** on EOS-mode servers "
				                "(`DefaultPlatformService=EOS`, covers both Steam and Epic) or "
				                "a **Steam64 ID** on Steam-mode servers. "
				                "The bot shows the correct `!ban id add` or `!ban steam add` "
				                "command to use with the returned ID.");
			}
			else
			{
				const FString LookupTarget = TargetId.ToLower();
				FString FoundId;
				FString FoundPlatformName;
				bool bFound = false;

				if (UWorld* World = GetWorld())
				{
					if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
					{
						for (APlayerState* ConnPS : GS->PlayerArray)
						{
							if (!ConnPS) { continue; }
							if (ConnPS->GetPlayerName().ToLower() != LookupTarget) { continue; }
							FoundId = GetPlayerPlatformId(ConnPS);
							const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
							FoundPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
							bFound = true;
							break;
						}
					}
				}

				if (!bFound)
				{
					Response = FString::Printf(
						TEXT(":person_shrugging: No connected player found with name **%s**."),
						*TargetId);
				}
				else if (FoundId.IsEmpty())
				{
					if (EosWarn.IsEmpty())
					{
						// EOS is operational – the platform ID is temporarily unavailable
						// (EOS session hasn't registered yet).  Retrying in a moment
						// should succeed once mIsOnline resolves.
						Response = FString::Printf(
							TEXT(":warning: Player **%s** is connected (platform: %s) but their "
							     "platform ban ID is not yet available.\n"
							     "This happens when the EOS session hasn't registered yet. "
							     "Wait a few seconds and try again."),
							*TargetId, *FoundPlatformName);
					}
					else
					{
						// EOS is NOT operational – the platform ban ID will not become
						// available by waiting.  Show the EOS warning instead of the
						// misleading "wait and retry" advice.
						Response = FString::Printf(
							TEXT(":warning: Player **%s** is connected (platform: %s) but their "
							     "platform ban ID cannot be retrieved – EOS service not operational.%s"),
							*TargetId, *FoundPlatformName, *EosWarn);
					}
				}
				else
				{
					// Use the correct ID-type label (Steam64 ID vs EOS PUID) and the
					// matching ban sub-command so the response is always accurate.
					const FString IdTypeLabel = FBanManager::GetPlatformTypeLabel(FoundId);
					const FString BanSubCmd   = FBanManager::IsValidSteam64Id(FoundId)
						? TEXT("steam add") : TEXT("id add");
					Response = FString::Printf(
						TEXT(":mag: **%s** (platform: **%s**)\n%s: `%s`\n"
						     "To ban by platform ID: `%s %s %s`"),
						*TargetId, *FoundPlatformName, *IdTypeLabel, *FoundId,
						*Config.BanCommandPrefix, *BanSubCmd, *FoundId);
					if (!EosWarn.IsEmpty())
					{
						Response += EosWarn;
					}
				}
			}
		}
		else if (IdVerb == TEXT("add"))
		{
			if (TargetId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban id add <platform_id>`\n"
				                "• **Steam64 ID** (17 digits starting with 765, e.g. `76561198123456789`) — "
				                "use when the server runs with `DefaultPlatformService=Steam`.\n"
				                "• **EOS PUID** (hex string, e.g. `0002abcdef1234567890abcdef123456`) — "
				                "use when the server runs with `DefaultPlatformService=EOS` (covers both Steam and Epic accounts).\n"
				                "• **X-FactoryGame-PlayerId header value** — the raw binary-hex header is accepted and "
				                "automatically normalized (Steam: `06`+16 hex → Steam64 decimal; EOS: `01`+32 hex → EOS PUID).\n"
				                "Use `!ban id lookup <PlayerName>` or `!ban players` to find a connected player's ID.");
			}
			else if (FBanManager::BanPlatformId(TargetId))
			{
				const FString PlatformTypeLabel = FBanManager::GetPlatformTypeLabel(TargetId);
				if (FBanManager::IsEnabled())
				{
					// Kick any currently-connected player whose platform ID matches.
					// We scan the player list rather than calling KickConnectedBannedPlayers
					// (which matches by name) so we can provide an accurate kicked count.
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase* GM  = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							// Snapshot PlayerArray to prevent iterator invalidation:
							// KickPlayer() → Destroy() → RemovePlayerState() modifies
							// PlayerArray synchronously in CSS UnrealEngine-CSS (UE5.3).
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != TargetId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT(":hammer: Platform ID `%s` (**%s**) has been banned from the server.%s"),
						*TargetId, *PlatformTypeLabel,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT(":hammer: Platform ID `%s` (**%s**) has been added to the ban list.\n"
						     ":warning: The ban system is currently **disabled** — run `!ban on` to enforce bans."),
						*TargetId, *PlatformTypeLabel);
				}
				if (!EosWarn.IsEmpty())
				{
					Response += EosWarn;
				}
			}
			else
			{
				Response = FString::Printf(TEXT(":yellow_circle: Platform ID `%s` (**%s**) is already banned."),
				                           *TargetId, *FBanManager::GetPlatformTypeLabel(TargetId));
				if (!EosWarn.IsEmpty())
				{
					Response += EosWarn;
				}
			}
		}
		else if (IdVerb == TEXT("remove"))
		{
			if (TargetId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban id remove <platform_id>`");
			}
			else if (FBanManager::UnbanPlatformId(TargetId))
			{
				Response = FString::Printf(TEXT(":white_check_mark: Platform ID `%s` has been unbanned."), *TargetId);
			}
			else
			{
				Response = FString::Printf(TEXT(":yellow_circle: Platform ID `%s` was not on the ban list."), *TargetId);
			}
		}
		else if (IdVerb == TEXT("list"))
		{
			const TArray<FString> AllIds = FBanManager::GetAllPlatformIds();
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (AllIds.Num() == 0)
			{
				Response = FString::Printf(TEXT(":scroll: Ban system is **%s**. No platform IDs banned."), *Status);
			}
			else
			{
				// Group IDs by detected platform type so admins can tell which
				// entries are Steam64 IDs and which are EOS PUIDs at a glance.
				TArray<FString> Lines;
				for (const FString& Id : AllIds)
				{
					const FString Label = FBanManager::GetPlatformTypeLabel(Id);
					Lines.Add(FString::Printf(TEXT("`%s` (%s)"), *Id, *Label));
				}
				Response = FString::Printf(
					TEXT(":scroll: Ban system is **%s**. Banned platform IDs (%d):\n%s"),
					*Status, AllIds.Num(), *FString::Join(Lines, TEXT("\n")));
			}
			if (!EosWarn.IsEmpty())
			{
				Response += EosWarn;
			}
		}
		else
		{
			Response = FString::Printf(
				TEXT(":question: Usage:\n"
				     "• `%s id lookup <PlayerName>` – find a connected player's platform ban ID (EOS PUID or Steam64 ID)\n"
				     "• `%s id add <platform_id>` – ban by Steam64 ID or EOS PUID\n"
				     "• `%s id remove <platform_id>` – unban by Steam64 ID or EOS PUID\n"
				     "• `%s id list` – list all banned IDs (labelled Steam / EOS PUID)\n"
				     "Tip: use `%s steam` or `%s epic` for platform-specific sub-commands."),
				*Config.BanCommandPrefix, *Config.BanCommandPrefix,
				*Config.BanCommandPrefix, *Config.BanCommandPrefix,
				*Config.BanCommandPrefix, *Config.BanCommandPrefix);
		}
	}
	else if (Verb == TEXT("steam"))
	{
		// ── Steam64 ID ban sub-commands ───────────────────────────────────────
		// Dedicated commands for banning players by their Steam64 ID.
		// Steam64 IDs are 17-digit decimal numbers starting with "765".
		// Use these commands when the server runs with DefaultPlatformService=Steam
		// or when you obtained a player's Steam64 ID from their Steam profile.
		//
		// Usage:
		//   !ban steam add <steam64_id>    – ban a Steam player by Steam64 ID
		//   !ban steam remove <steam64_id> – unban by Steam64 ID
		//   !ban steam list                – list all Steam64 ID bans
		FString SteamVerb, SteamId;
		if (!Arg.Split(TEXT(" "), &SteamVerb, &SteamId, ESearchCase::IgnoreCase))
		{
			SteamVerb = Arg.TrimStartAndEnd();
			SteamId   = TEXT("");
		}
		SteamVerb = SteamVerb.TrimStartAndEnd().ToLower();
		SteamId   = FBanManager::NormalizePlatformId(SteamId.TrimStartAndEnd());

		if (SteamVerb == TEXT("add"))
		{
			if (SteamId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban steam add <steam64_id>`\n"
				                "Steam64 IDs are 17-digit numbers starting with `765`, "
				                "e.g. `76561198123456789`.\n"
				                "Find a player's Steam64 ID on their Steam profile URL "
				                "or via `!ban players` when they are connected.");
			}
			else if (!FBanManager::IsValidSteam64Id(SteamId))
			{
				Response = FString::Printf(
					TEXT(":x: `%s` is not a valid Steam64 ID.\n"
					     "Steam64 IDs are exactly **17 decimal digits** and start with **765** "
					     "(e.g. `76561198123456789`).\n"
					     "For EOS Product User IDs use `%s id add <eos_puid>` or `%s epic add <eos_puid>` instead."),
					*SteamId, *Config.BanCommandPrefix, *Config.BanCommandPrefix);
			}
			else if (FBanManager::BanPlatformId(SteamId))
			{
				if (FBanManager::IsEnabled())
				{
					// Kick any connected player whose Steam64 ID matches.
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase* GM  = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != SteamId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT(":hammer: Steam64 ID `%s` has been banned from the server.%s"),
						*SteamId,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT(":hammer: Steam64 ID `%s` has been added to the ban list.\n"
						     ":warning: The ban system is currently **disabled** — run `!ban on` to enforce bans."),
						*SteamId);
				}
			}
			else
			{
				Response = FString::Printf(
					TEXT(":yellow_circle: Steam64 ID `%s` is already banned."), *SteamId);
			}
		}
		else if (SteamVerb == TEXT("remove"))
		{
			if (SteamId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban steam remove <steam64_id>`");
			}
			else if (!FBanManager::IsValidSteam64Id(SteamId))
			{
				Response = FString::Printf(
					TEXT(":x: `%s` is not a valid Steam64 ID (must be 17 decimal digits starting with 765)."),
					*SteamId);
			}
			else if (FBanManager::UnbanPlatformId(SteamId))
			{
				Response = FString::Printf(
					TEXT(":white_check_mark: Steam64 ID `%s` has been unbanned."), *SteamId);
			}
			else
			{
				Response = FString::Printf(
					TEXT(":yellow_circle: Steam64 ID `%s` was not on the ban list."), *SteamId);
			}
		}
		else if (SteamVerb == TEXT("list"))
		{
			const TArray<FString> SteamIds = FBanManager::GetPlatformIdsByType(
				FBanManager::EPlatformIdType::Steam);
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (SteamIds.Num() == 0)
			{
				Response = FString::Printf(
					TEXT(":scroll: Ban system is **%s**. No Steam64 IDs banned."), *Status);
			}
			else
			{
				TArray<FString> Lines;
				for (const FString& Id : SteamIds)
				{
					Lines.Add(FString::Printf(TEXT("`%s`"), *Id));
				}
				Response = FString::Printf(
					TEXT(":scroll: Ban system is **%s**. Banned Steam64 IDs (%d):\n%s"),
					*Status, SteamIds.Num(), *FString::Join(Lines, TEXT("\n")));
			}
		}
		else
		{
			Response = FString::Printf(
				TEXT(":question: Steam ban sub-commands:\n"
				     "• `%s steam add <steam64_id>` – ban a Steam player by Steam64 ID (17-digit, starts with 765)\n"
				     "• `%s steam remove <steam64_id>` – unban by Steam64 ID\n"
				     "• `%s steam list` – list all banned Steam64 IDs\n"
				     "Steam64 IDs look like: `76561198123456789`\n"
				     "For EOS PUID bans (used when DefaultPlatformService=EOS) use `%s epic` or `%s id`."),
				*Config.BanCommandPrefix, *Config.BanCommandPrefix, *Config.BanCommandPrefix,
				*Config.BanCommandPrefix, *Config.BanCommandPrefix);
		}
	}
	else if (Verb == TEXT("epic"))
	{
		// ── EOS PUID ban sub-commands ─────────────────────────────────────────
		// Dedicated commands for banning players by their EOS Product User ID.
		// EOS PUIDs are hex alphanumeric strings used by Epic Online Services.
		// When the server runs with DefaultPlatformService=EOS, BOTH Steam and
		// Epic Games players receive an EOS PUID — use these commands for either.
		//
		// Usage:
		//   !ban epic add <eos_puid>    – ban by EOS PUID (Steam or Epic account)
		//   !ban epic remove <eos_puid> – unban by EOS PUID
		//   !ban epic list              – list all EOS PUID bans
		FString EpicVerb, EpicId;
		if (!Arg.Split(TEXT(" "), &EpicVerb, &EpicId, ESearchCase::IgnoreCase))
		{
			EpicVerb = Arg.TrimStartAndEnd();
			EpicId   = TEXT("");
		}
		EpicVerb = EpicVerb.TrimStartAndEnd().ToLower();
		EpicId   = FBanManager::NormalizePlatformId(EpicId.TrimStartAndEnd());

		// EOS availability warning – appended when EOS platform is not operational.
		const FString EosWarnEpic = IsEOSPlatformOperational()
			? TEXT("")
			: FString::Printf(
			      TEXT("\n:warning: **EOS platform is unavailable** – platform-ID bans are "
			           "stored but will NOT be enforced at join time. "
			           "Run `%s eos` for diagnostics."),
			      *Config.ServerInfoCommandPrefix);

		if (EpicVerb == TEXT("add"))
		{
			if (EpicId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban epic add <eos_puid>`\n"
				                "EOS Product User IDs are hex alphanumeric strings "
				                "(e.g. `0002abcdef1234567890abcdef123456`).\n"
				                "Use `!ban players` or `!ban id lookup <PlayerName>` to find a "
				                "connected player's EOS PUID.\n"
				                "EOS PUIDs work for **both** Steam and Epic Games accounts when "
				                "the server uses `DefaultPlatformService=EOS`.");
			}
			else if (FBanManager::IsValidSteam64Id(EpicId))
			{
				// The supplied ID looks like a Steam64 ID — give helpful guidance.
				Response = FString::Printf(
					TEXT(":warning: `%s` looks like a **Steam64 ID**, not an EOS PUID.\n"
					     "• If your server uses `DefaultPlatformService=Steam`, use `%s steam add %s` instead.\n"
					     "• If your server uses `DefaultPlatformService=EOS`, use `%s id lookup <PlayerName>` "
					     "to get the player's EOS PUID, then `%s epic add <eos_puid>`."),
					*EpicId,
					*Config.BanCommandPrefix, *EpicId,
					*Config.BanCommandPrefix, *Config.BanCommandPrefix);
			}
			else if (FBanManager::BanPlatformId(EpicId))
			{
				if (FBanManager::IsEnabled())
				{
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase* GM  = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != EpicId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT(":hammer: EOS PUID `%s` has been banned from the server.%s"),
						*EpicId,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT(":hammer: EOS PUID `%s` has been added to the ban list.\n"
						     ":warning: The ban system is currently **disabled** — run `!ban on` to enforce bans."),
						*EpicId);
				}
				if (!EosWarnEpic.IsEmpty())
				{
					Response += EosWarnEpic;
				}
			}
			else
			{
				Response = FString::Printf(
					TEXT(":yellow_circle: EOS PUID `%s` is already banned."), *EpicId);
				if (!EosWarnEpic.IsEmpty())
				{
					Response += EosWarnEpic;
				}
			}
		}
		else if (EpicVerb == TEXT("remove"))
		{
			if (EpicId.IsEmpty())
			{
				Response = TEXT(":warning: Usage: `!ban epic remove <eos_puid>`");
			}
			else if (FBanManager::UnbanPlatformId(EpicId))
			{
				Response = FString::Printf(
					TEXT(":white_check_mark: EOS PUID `%s` has been unbanned."), *EpicId);
			}
			else
			{
				Response = FString::Printf(
					TEXT(":yellow_circle: EOS PUID `%s` was not on the ban list."), *EpicId);
			}
		}
		else if (EpicVerb == TEXT("list"))
		{
			const TArray<FString> EpicIds = FBanManager::GetPlatformIdsByType(
				FBanManager::EPlatformIdType::Epic);
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (EpicIds.Num() == 0)
			{
				Response = FString::Printf(
					TEXT(":scroll: Ban system is **%s**. No EOS PUIDs banned."), *Status);
			}
			else
			{
				TArray<FString> Lines;
				for (const FString& Id : EpicIds)
				{
					Lines.Add(FString::Printf(TEXT("`%s`"), *Id));
				}
				Response = FString::Printf(
					TEXT(":scroll: Ban system is **%s**. Banned EOS PUIDs (%d):\n%s"),
					*Status, EpicIds.Num(), *FString::Join(Lines, TEXT("\n")));
			}
			if (!EosWarnEpic.IsEmpty())
			{
				Response += EosWarnEpic;
			}
		}
		else
		{
			Response = FString::Printf(
				TEXT(":question: Epic/EOS ban sub-commands:\n"
				     "• `%s epic add <eos_puid>` – ban by EOS PUID (works for Steam & Epic accounts on EOS servers)\n"
				     "• `%s epic remove <eos_puid>` – unban by EOS PUID\n"
				     "• `%s epic list` – list all banned EOS PUIDs\n"
				     "EOS PUIDs look like: `0002abcdef1234567890abcdef123456`\n"
				     "Use `!ban players` or `!ban id lookup <name>` to find a connected player's PUID.\n"
				     "For Steam64 ID bans (DefaultPlatformService=Steam servers) use `%s steam`."),
				*Config.BanCommandPrefix, *Config.BanCommandPrefix, *Config.BanCommandPrefix,
				*Config.BanCommandPrefix);
		}
	}
	else
	{
		Response = TEXT(":question: Unknown ban command. Available sub-commands:\n"
		                "**Name banning:** `add <name>`, `remove <name>`, `list`, `check <name>`\n"
		                "**Platform ID banning (generic):** `id lookup <name>`, `id add <id>`, `id remove <id>`, `id list`\n"
		                "**Steam64 ID banning:** `steam add <steam64_id>`, `steam remove <steam64_id>`, `steam list`\n"
		                "**EOS PUID banning (Epic/EOS):** `epic add <eos_puid>`, `epic remove <eos_puid>`, `epic list`\n"
		                "**Status & control:** `on`, `off`, `status`, `players`\n"
		                "**Role management:** `role add <discord_id>`, `role remove <discord_id>`");
	}

	// Send the response back to Discord on the channel where the command was issued.
	// If no explicit response channel was provided, fall back to the first main channel.
	const TArray<FString> FallbackChannels = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
	const FString EffectiveChannel = ResponseChannelId.IsEmpty()
		? (FallbackChannels.Num() > 0 ? FallbackChannels[0] : TEXT(""))
		: ResponseChannelId;
	SendMessageToChannel(EffectiveChannel, Response);
}

// ─────────────────────────────────────────────────────────────────────────────
// In-game chat command helpers
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleKickCommand(const FString& SubCommand,
                                                const FString& DiscordUsername,
                                                const FString& ResponseChannelId)
{
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Kick command from '%s': '%s'"), *DiscordUsername, *SubCommand);

	if (SubCommand.IsEmpty())
	{
		SendMessageToChannel(ResponseChannelId,
		    FString::Printf(TEXT(":warning: Usage: `%s <PlayerName>` or `%s <PlayerName> <reason>`\n"
		                        "Kicks the player from the server without banning them."),
		                    *Config.KickCommandPrefix, *Config.KickCommandPrefix));
		return;
	}

	// Parse optional reason: everything after the first word is the reason.
	FString TargetName;
	FString Reason;
	if (!SubCommand.Split(TEXT(" "), &TargetName, &Reason, ESearchCase::IgnoreCase))
	{
		TargetName = SubCommand.TrimStartAndEnd();
		Reason     = TEXT("");
	}
	TargetName = TargetName.TrimStartAndEnd();
	Reason     = Reason.TrimStartAndEnd();

	// Use the configured default reason when none was provided in the command.
	const FString EffectiveReason = Reason.IsEmpty()
		? (Config.KickReason.IsEmpty() ? TEXT("You have been kicked from the server by an admin.") : Config.KickReason)
		: Reason;

	// Find the player and kick them.
	UWorld* World = GetWorld();
	if (!World)
	{
		SendMessageToChannel(ResponseChannelId, TEXT(":x: Could not kick player – game world is unavailable."));
		return;
	}

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS)
	{
		SendMessageToChannel(ResponseChannelId, TEXT(":x: Could not kick player – game state is unavailable."));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode<AGameModeBase>();
	if (!GM || !GM->GameSession)
	{
		SendMessageToChannel(ResponseChannelId, TEXT(":x: Could not kick player – game session is unavailable."));
		return;
	}

	const FString LowerTarget = TargetName.ToLower();

	// Snapshot the array to avoid invalidation when KickPlayer() is called.
	APlayerController* FoundPC = nullptr;
	const TArray<APlayerState*> Snapshot = GS->PlayerArray;
	for (APlayerState* PS : Snapshot)
	{
		if (!PS) { continue; }
		if (PS->GetPlayerName().ToLower() != LowerTarget) { continue; }
		APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
		if (!PC || PC->IsLocalController()) { continue; }
		FoundPC = PC;
		break;
	}

	if (!FoundPC)
	{
		SendMessageToChannel(ResponseChannelId,
		    FString::Printf(TEXT(":yellow_circle: Player **%s** is not currently connected."), *TargetName));
		return;
	}

	UE_LOG(LogDiscordBridge, Warning,
	       TEXT("DiscordBridge: Kicking player '%s' (requested by Discord user '%s'). Reason: %s"),
	       *TargetName, *DiscordUsername, *EffectiveReason);

	GM->GameSession->KickPlayer(FoundPC, FText::FromString(EffectiveReason));

	// Send the kick notification to Discord.
	FString DiscordNotice = Config.KickDiscordMessage;
	if (!DiscordNotice.IsEmpty())
	{
		DiscordNotice.ReplaceInline(TEXT("%PlayerName%"), *TargetName, ESearchCase::IgnoreCase);
		DiscordNotice.ReplaceInline(TEXT("%Reason%"),     *EffectiveReason, ESearchCase::IgnoreCase);
		const TArray<FString> AllChannels = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
		for (const FString& Ch : AllChannels)
		{
			SendMessageToChannel(Ch, DiscordNotice);
		}
	}

	SendMessageToChannel(ResponseChannelId,
	    FString::Printf(TEXT(":boot: **%s** has been kicked. Reason: %s"), *TargetName, *EffectiveReason));
}

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

	// Snapshot PlayerArray before iterating to prevent iterator invalidation.
	// In CSS UnrealEngine-CSS (UE5.3), AGameSession::KickPlayer() calls
	// APlayerController::Destroy() which synchronously triggers
	// AGameModeBase::Logout() → AGameStateBase::RemovePlayerState(), removing
	// the entry from PlayerArray.  Iterating a live PlayerArray and calling
	// KickPlayer() inside the loop therefore causes undefined behaviour / crashes.
	// Iterating a snapshotted copy is safe: the copy is not modified by kicks,
	// and UE's garbage-collector does not free actor memory mid-frame.
	const TArray<APlayerState*> PlayerArraySnapshot = GS->PlayerArray;
	for (APlayerState* PS : PlayerArraySnapshot)
	{
		if (!PS)
		{
			continue;
		}

		const FString ConnectedName = PS->GetPlayerName();
		const FString PlatformId    = GetPlayerPlatformId(PS);

		// When a specific name was requested, only match that player by name.
		// Otherwise kick anyone whose name or platform ID is on the ban list.
		//
		// Important: do NOT skip players whose ConnectedName is empty.  In
		// Satisfactory (CSS UE5.3) the display name is populated asynchronously
		// via Server_SetPlayerNames after PostLogin.  A Steam or EGS player
		// banned by platform ID must be caught even before their name resolves.
		const bool bMatch = bTargetSpecific
			? (!ConnectedName.IsEmpty() && ConnectedName.ToLower() == LowerTarget)
			: ((!ConnectedName.IsEmpty() && FBanManager::IsBanned(ConnectedName)) ||
			   (!PlatformId.IsEmpty() && FBanManager::IsPlatformIdBanned(PlatformId)));

		if (!bMatch)
		{
			continue;
		}

		APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
		if (!PC || PC->IsLocalController())
		{
			continue;
		}

		// Include the CSS UE platform name (Steam / Epic / None) in the log so
		// admins can identify which online platform the banned player is using.
		const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(PS);
		const FString PlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge BanSystem: kicking connected banned player '%s' (platform: %s, ID: %s)"),
		       *ConnectedName, *PlatformName, *PlatformId);

		GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
		++KickedCount;
	}

	return KickedCount;
}

void UDiscordBridgeSubsystem::NotifyLoginBanReject(const FString& PlatformId)
{
	if (PlatformId.IsEmpty() || Config.BanLoginRejectDiscordMessage.IsEmpty())
	{
		return; // Notifications disabled or no ID provided.
	}

	// Rate-limit: suppress repeated notifications for the same platform ID
	// within a 60-second window to prevent Discord spam when a banned player
	// retries the connection rapidly.
	const FString LowerId = PlatformId.ToLower();
	const double  NowSec  = FPlatformTime::Seconds();
	constexpr double CooldownSeconds = 60.0;
	if (const double* LastTime = LastLoginRejectNotifyTimeByPlatformId.Find(LowerId))
	{
		if (NowSec - *LastTime < CooldownSeconds)
		{
			UE_LOG(LogDiscordBridge, Verbose,
			       TEXT("DiscordBridge: NotifyLoginBanReject – rate-limit: suppressing "
			            "duplicate notification for platform ID '%s' (last sent %.0f s ago)."),
			       *PlatformId, NowSec - *LastTime);
			return;
		}
	}
	LastLoginRejectNotifyTimeByPlatformId.Add(LowerId, NowSec);

	// Build the notification from the configured template.
	// Placeholders: %PlatformId%, %PlatformType%.
	const FString PlatformTypeLabel = FBanManager::GetPlatformTypeLabel(PlatformId);
	FString Notice = Config.BanLoginRejectDiscordMessage;
	Notice = Notice.Replace(TEXT("%PlatformId%"),   *PlatformId);
	Notice = Notice.Replace(TEXT("%PlatformType%"), *PlatformTypeLabel);

	UE_LOG(LogDiscordBridge, Warning,
	       TEXT("DiscordBridge: NotifyLoginBanReject – sending login-reject notification "
	            "for platform ID '%s' (%s)."),
	       *PlatformId, *PlatformTypeLabel);

	const TArray<FString> MainChannelIds = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
	const TArray<FString> BanChannelIds  = FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

	for (const FString& ChanId : MainChannelIds)
	{
		SendMessageToChannel(ChanId, Notice);
	}
	// Also notify the dedicated ban channel(s) (if configured) so admins have
	// a focused audit log of all ban-related events including login rejections.
	for (const FString& ChanId : BanChannelIds)
	{
		if (!MainChannelIds.Contains(ChanId))
		{
			SendMessageToChannel(ChanId, Notice);
		}
	}
}

void UDiscordBridgeSubsystem::ScheduleEosPuidFollowupCheck(
    APlayerController* Controller,
    const FString&     CapturedSteamId,
    const FString&     PlayerName)
{
	if (!Controller || !::IsValid(Controller))
	{
		return;
	}

	UWorld* EosDeferWorld = GetWorld();
	if (!EosDeferWorld)
	{
		return;
	}

	TWeakObjectPtr<APlayerController> EosDeferWeakPC(Controller);
	TWeakObjectPtr<UWorld>            EosDeferWeakWorld(EosDeferWorld);
	TSharedRef<FTimerHandle>          EosDeferHandle = MakeShared<FTimerHandle>();
	TSharedRef<int32>                 EosDeferLeft   = MakeShared<int32>(120);

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: scheduling EOS PUID followup check for '%s' "
	            "(Steam64='%s') – will check once EOS upgrades the ID."),
	       *PlayerName, *CapturedSteamId);

	EosDeferWorld->GetTimerManager().SetTimer(*EosDeferHandle,
		FTimerDelegate::CreateWeakLambda(this,
			[this, EosDeferWeakPC, EosDeferWeakWorld, EosDeferHandle, EosDeferLeft,
			 CapturedSteamId]()
		{
			UWorld* W = EosDeferWeakWorld.Get();

			if (!EosDeferWeakPC.IsValid())
			{
				// Player already disconnected – stop the timer.
				if (W) { W->GetTimerManager().ClearTimer(*EosDeferHandle); }
				return;
			}

			APlayerController* EosDeferPC = EosDeferWeakPC.Get();

			// Guard: abort if the connection closed while waiting
			// (covers leave-then-rejoin scenarios).
			if (!EosDeferPC->NetConnection ||
			    EosDeferPC->NetConnection->GetConnectionState() == USOCK_Closed ||
			    EosDeferPC->NetConnection->GetConnectionState() == USOCK_Invalid)
			{
				if (W) { W->GetTimerManager().ClearTimer(*EosDeferHandle); }
				return;
			}

			const APlayerState* EosDeferPS = EosDeferPC->GetPlayerState<APlayerState>();
			// Guard: PlayerState raw pointer may be non-null but GC'd.
			if (!::IsValid(EosDeferPS))
			{
				if (W) { W->GetTimerManager().ClearTimer(*EosDeferHandle); }
				return;
			}

			const FString CurrentPId = GetPlayerPlatformId(EosDeferPS);

			// EOS has upgraded the ID: the player now has an EOS PUID
			// instead of the Steam64 ID we captured at join time.
			// The format will be different (hex vs 17-digit decimal).
			if (!CurrentPId.IsEmpty() &&
			    CurrentPId.ToLower() != CapturedSteamId.ToLower())
			{
				if (W) { W->GetTimerManager().ClearTimer(*EosDeferHandle); }

				// Read live player details for accurate logs/messages.
				const FString EosDeferName = EosDeferPS->GetPlayerName();
				const FString EosDeferAddr = EosDeferPC->NetConnection
					? EosDeferPC->NetConnection->LowLevelGetRemoteAddress(true)
					: TEXT("(disconnected)");
				const AFGPlayerState* EosDeferFGPS =
					Cast<const AFGPlayerState>(EosDeferPS);
				const FString EosDeferPlatform =
					EosDeferFGPS ? EosDeferFGPS->GetPlayingPlatformName()
					             : TEXT("Unknown");

				// Cross-platform ban check: now that we have both the
				// Steam64 ID and the resolved EOS PUID, check the ban
				// list against EITHER identifier.  A ban by Steam64 or
				// by EOS PUID must kick the player regardless of which
				// ID they present at their next login.
				const bool bEosPuidBanned =
					FBanManager::IsEnabled() && FBanManager::IsPlatformIdBanned(CurrentPId);
				const bool bSteam64Banned =
					FBanManager::IsEnabled() && FBanManager::IsPlatformIdBanned(CapturedSteamId);

				// Cross-platform ID linking: automatically ban the
				// partner ID so the ban is enforced regardless of
				// which identifier the player presents on the next
				// login (e.g. if EOS fully warms up and the Steam64
				// fast-path no longer fires on reconnect).
				if (FBanManager::IsEnabled())
				{
					if (bSteam64Banned && !bEosPuidBanned)
					{
						// Steam64 is banned; also ban the EOS PUID so
						// the player cannot bypass by reconnecting
						// without the Steam64 fast-path phase.
						if (FBanManager::BanPlatformId(CurrentPId))
						{
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge BanSystem: cross-platform link – "
							            "added EOS PUID '%s' to ban list because Steam64 "
							            "'%s' is banned (player '%s', %s). "
							            "Ban now covers both IDs."),
							       *CurrentPId, *CapturedSteamId,
							       *EosDeferName, *EosDeferAddr);

							// Notify the ban channel so admins have an audit trail.
							const TArray<FString> BanChanIds =
								FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);
							const TArray<FString> MainChanIds =
								FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
							const FString LinkMsg = FString::Printf(
								TEXT(":link: Cross-platform ban link: EOS PUID `%s` "
								     "auto-added to ban list (Steam64 `%s` was already "
								     "banned for **%s**). Ban now covers both IDs."),
								*CurrentPId, *CapturedSteamId, *EosDeferName);
							for (const FString& ChanId : BanChanIds)
							{
								SendMessageToChannel(ChanId, LinkMsg);
							}
							if (BanChanIds.IsEmpty())
							{
								for (const FString& ChanId : MainChanIds)
								{
									SendMessageToChannel(ChanId, LinkMsg);
								}
							}
						}
					}
					else if (bEosPuidBanned && !bSteam64Banned)
					{
						// EOS PUID is banned; also ban the Steam64 so
						// future joins during the Steam64 fast-path
						// phase are caught at primary check immediately.
						if (FBanManager::BanPlatformId(CapturedSteamId))
						{
							UE_LOG(LogDiscordBridge, Log,
							       TEXT("DiscordBridge BanSystem: cross-platform link – "
							            "added Steam64 '%s' to ban list because EOS PUID "
							            "'%s' is banned (player '%s', %s). "
							            "Ban now covers both IDs."),
							       *CapturedSteamId, *CurrentPId,
							       *EosDeferName, *EosDeferAddr);
						}
					}
				}

				if (bEosPuidBanned || bSteam64Banned)
				{
					UE_LOG(LogDiscordBridge, Warning,
					       TEXT("DiscordBridge BanSystem: kicking '%s' – EOS PUID followup "
					            "ban (EOS PUID='%s', original Steam64='%s', "
					            "platform=%s, addr=%s)."),
					       *EosDeferName, *CurrentPId, *CapturedSteamId,
					       *EosDeferPlatform, *EosDeferAddr);

					AGameModeBase* EosDeferGM = nullptr;
					if (W) { EosDeferGM = W->GetAuthGameMode<AGameModeBase>(); }

					if (EosDeferGM && EosDeferGM->GameSession)
					{
						const FString KickReason = Config.BanKickReason.IsEmpty()
							? TEXT("You are banned from this server.")
							: Config.BanKickReason;
						EosDeferGM->GameSession->KickPlayer(
							EosDeferPC, FText::FromString(KickReason));
					}

					// Remove from tracking to suppress the normal leave
					// notification for this kicked-by-ban player.
					TrackedPlayerNames.Remove(EosDeferPC);

					// Mirror the ban-kick Discord message if configured.
					if (!Config.BanKickDiscordMessage.IsEmpty())
					{
						FString Notice = Config.BanKickDiscordMessage;
						Notice = Notice.Replace(TEXT("%PlayerName%"), *EosDeferName);

						const TArray<FString> MainChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
						const TArray<FString> BanChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

						for (const FString& ChanId : MainChanIds)
						{
							SendMessageToChannel(ChanId, Notice);
						}
						for (const FString& ChanId : BanChanIds)
						{
							if (!MainChanIds.Contains(ChanId))
							{
								SendMessageToChannel(ChanId, Notice);
							}
						}
					}
				}
				else
				{
					UE_LOG(LogDiscordBridge, Log,
					       TEXT("DiscordBridge: EOS PUID followup check: '%s' not "
					            "banned by EOS PUID '%s' or Steam64 '%s' (addr=%s)."),
					       *EosDeferName, *CurrentPId, *CapturedSteamId, *EosDeferAddr);
				}
				return;
			}

			// EOS PUID still not available or still returning the same Steam64 ID
			// – retry up to the limit.
			--(*EosDeferLeft);
			if (*EosDeferLeft <= 0)
			{
				if (W) { W->GetTimerManager().ClearTimer(*EosDeferHandle); }
				UE_LOG(LogDiscordBridge, Log,
				       TEXT("DiscordBridge: EOS PUID followup check timed out for "
				            "'%s' (Steam64='%s') – EOS did not upgrade the ID within "
				            "60 s; EOS PUID ban entries cannot be checked for this player."),
				       *EosDeferPS->GetPlayerName(), *CapturedSteamId);
			}
		}),
		0.5f, true);
}

// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::ScheduleNativePlatformIdCrossCheck(
	APlayerController* Controller,
	const FString&     CapturedPuid,
	const FString&     PlayerName)
{
	if (!Controller || !::IsValid(Controller))
	{
		return;
	}

	UWorld* CrossWorld = GetWorld();
	if (!CrossWorld)
	{
		return;
	}

	TWeakObjectPtr<APlayerController> CrossWeakPC(Controller);
	TWeakObjectPtr<UWorld>            CrossWeakWorld(CrossWorld);
	TSharedRef<FTimerHandle>          CrossHandle = MakeShared<FTimerHandle>();
	TSharedRef<int32>                 CrossLeft   = MakeShared<int32>(20);

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: scheduling Steam64 cross-check for '%s' "
	            "(EOS PUID='%s') – verifying Steam64-based bans via component."),
	       *PlayerName, *CapturedPuid);

	CrossWorld->GetTimerManager().SetTimer(*CrossHandle,
		FTimerDelegate::CreateWeakLambda(this,
			[this, CrossWeakPC, CrossWeakWorld, CrossHandle, CrossLeft, CapturedPuid]()
		{
			UWorld* W = CrossWeakWorld.Get();

			if (!CrossWeakPC.IsValid())
			{
				if (W) { W->GetTimerManager().ClearTimer(*CrossHandle); }
				return;
			}

			APlayerController* CrossPC = CrossWeakPC.Get();

			if (!CrossPC->NetConnection ||
			    CrossPC->NetConnection->GetConnectionState() == USOCK_Closed ||
			    CrossPC->NetConnection->GetConnectionState() == USOCK_Invalid)
			{
				if (W) { W->GetTimerManager().ClearTimer(*CrossHandle); }
				return;
			}

			const APlayerState* CrossPS = CrossPC->GetPlayerState<APlayerState>();
			if (!::IsValid(CrossPS))
			{
				if (W) { W->GetTimerManager().ClearTimer(*CrossHandle); }
				return;
			}

			// Use the component directly – GetPlayerPlatformIdFromComponent() always
			// returns the native platform account (Steam64 for Steam players), while
			// GetPlayerPlatformId() would return the EOS PUID (same as CapturedPuid).
			const FString ComponentId = GetPlayerPlatformIdFromComponent(CrossPC);
			if (ComponentId.IsEmpty())
			{
				// Server RPC has not yet arrived – retry.
				--(*CrossLeft);
				if (*CrossLeft <= 0)
				{
					if (W) { W->GetTimerManager().ClearTimer(*CrossHandle); }
					UE_LOG(LogDiscordBridge, Log,
					       TEXT("DiscordBridge: Steam64 cross-check timed out for "
					            "'%s' (EOS PUID='%s') – component returned no data "
					            "within 10 s; likely an EGS-only player."),
					       *CrossPS->GetPlayerName(), *CapturedPuid);
				}
				return;
			}

			// Component data is available – stop the timer now.
			if (W) { W->GetTimerManager().ClearTimer(*CrossHandle); }

			// If the component returned the same EOS PUID (EGS-only player with no
			// native Steam account), there is no Steam64 to cross-check.
			if (ComponentId.ToLower() == CapturedPuid.ToLower())
			{
				UE_LOG(LogDiscordBridge, Verbose,
				       TEXT("DiscordBridge: Steam64 cross-check: component returned "
				            "EOS PUID '%s' for '%s' – EGS-only player, no Steam64."),
				       *ComponentId, *CrossPS->GetPlayerName());
				return;
			}

			// ComponentId is the native platform ID (Steam64 for Steam players).
			const FString CrossName = CrossPS->GetPlayerName();
			const FString CrossAddr = CrossPC->NetConnection
				? CrossPC->NetConnection->LowLevelGetRemoteAddress(true)
				: TEXT("(disconnected)");
			const AFGPlayerState* CrossFGPS = Cast<const AFGPlayerState>(CrossPS);
			const FString CrossPlatform = CrossFGPS
				? CrossFGPS->GetPlayingPlatformName() : TEXT("Unknown");

			const bool bSteam64Banned =
				FBanManager::IsEnabled() && FBanManager::IsPlatformIdBanned(ComponentId);
			const bool bEosPuidBanned =
				FBanManager::IsEnabled() && FBanManager::IsPlatformIdBanned(CapturedPuid);

			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Steam64 cross-check for '%s': "
			            "Steam64='%s' banned=%s, EOS PUID='%s' banned=%s "
			            "(platform=%s, addr=%s)."),
			       *CrossName, *ComponentId,
			       bSteam64Banned ? TEXT("YES") : TEXT("NO"),
			       *CapturedPuid,
			       bEosPuidBanned ? TEXT("YES") : TEXT("NO"),
			       *CrossPlatform, *CrossAddr);

			// ── Cross-platform ID linking ─────────────────────────────────────
			// Mirror the same cross-linking performed in ScheduleEosPuidFollowupCheck():
			// if either identifier is banned, automatically ban the other so every
			// future join (regardless of which ID path fires) enforces the ban.
			if (FBanManager::IsEnabled())
			{
				if (bSteam64Banned && !bEosPuidBanned)
				{
					if (FBanManager::BanPlatformId(CapturedPuid))
					{
						UE_LOG(LogDiscordBridge, Log,
						       TEXT("DiscordBridge BanSystem: cross-platform link – "
						            "added EOS PUID '%s' to ban list because Steam64 "
						            "'%s' is banned (player '%s', %s). "
						            "Ban now covers both IDs."),
						       *CapturedPuid, *ComponentId, *CrossName, *CrossAddr);

						const TArray<FString> BanChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);
						const TArray<FString> MainChanIds =
							FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
						const FString LinkMsg = FString::Printf(
							TEXT(":link: Cross-platform ban link: EOS PUID `%s` "
							     "auto-added to ban list (Steam64 `%s` was already "
							     "banned for **%s**). Ban now covers both IDs."),
							*CapturedPuid, *ComponentId, *CrossName);
						for (const FString& ChanId : BanChanIds)
						{
							SendMessageToChannel(ChanId, LinkMsg);
						}
						if (BanChanIds.IsEmpty())
						{
							for (const FString& ChanId : MainChanIds)
							{
								SendMessageToChannel(ChanId, LinkMsg);
							}
						}
					}
				}
				else if (bEosPuidBanned && !bSteam64Banned)
				{
					if (FBanManager::BanPlatformId(ComponentId))
					{
						UE_LOG(LogDiscordBridge, Log,
						       TEXT("DiscordBridge BanSystem: cross-platform link – "
						            "added Steam64 '%s' to ban list because EOS PUID "
						            "'%s' is banned (player '%s', %s). "
						            "Ban now covers both IDs."),
						       *ComponentId, *CapturedPuid, *CrossName, *CrossAddr);
					}
				}
			}

			if (bSteam64Banned || bEosPuidBanned)
			{
				UE_LOG(LogDiscordBridge, Warning,
				       TEXT("DiscordBridge BanSystem: kicking '%s' – Steam64 cross-check "
				            "ban detected (Steam64='%s', EOS PUID='%s', "
				            "platform=%s, addr=%s)."),
				       *CrossName, *ComponentId, *CapturedPuid,
				       *CrossPlatform, *CrossAddr);

				AGameModeBase* CrossGM = nullptr;
				if (W) { CrossGM = W->GetAuthGameMode<AGameModeBase>(); }

				if (CrossGM && CrossGM->GameSession)
				{
					const FString KickReason = Config.BanKickReason.IsEmpty()
						? TEXT("You are banned from this server.")
						: Config.BanKickReason;
					CrossGM->GameSession->KickPlayer(
						CrossPC, FText::FromString(KickReason));
				}

				// Remove from tracking to suppress the normal leave notification.
				TrackedPlayerNames.Remove(CrossPC);

				if (!Config.BanKickDiscordMessage.IsEmpty())
				{
					FString Notice = Config.BanKickDiscordMessage;
					Notice = Notice.Replace(TEXT("%PlayerName%"), *CrossName);

					const TArray<FString> MainChanIds =
						FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
					const TArray<FString> BanChanIds =
						FDiscordBridgeConfig::ParseChannelIds(Config.BanChannelId);

					for (const FString& ChanId : MainChanIds)
					{
						SendMessageToChannel(ChanId, Notice);
					}
					for (const FString& ChanId : BanChanIds)
					{
						if (!MainChanIds.Contains(ChanId))
						{
							SendMessageToChannel(ChanId, Notice);
						}
					}
				}
			}
		}),
		0.5f, true);
}

void UDiscordBridgeSubsystem::HandleInGameWhitelistCommand(const FString& SubCommand)
{
	UE_LOG(LogDiscordBridge, Log,
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
	UE_LOG(LogDiscordBridge, Log,
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
			Response = TEXT("Usage: !ban add <PlayerName>  "
			                "(also auto-bans by platform ID if the player is currently connected)");
		}
		else if (FBanManager::BanPlayer(Arg))
		{
			// Also try to auto-ban by platform ID if the player is currently connected.
			FString AutoPlatformId;
			FString AutoPlatformName;
			bool bPlayerConnected  = false;
			bool bPuidResolving    = false;
			bool bAutoIdBanned     = false;
			bool bIdAlreadyBanned  = false;

			if (UWorld* World = GetWorld())
			{
				if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
				{
					const FString AddTarget = Arg.ToLower();
					for (APlayerState* ConnPS : GS->PlayerArray)
					{
						if (!ConnPS) { continue; }
						if (ConnPS->GetPlayerName().ToLower() != AddTarget) { continue; }
						bPlayerConnected = true;
						AutoPlatformId   = GetPlayerPlatformId(ConnPS);
						const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
						AutoPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
						break;
					}
				}
			}

			if (bPlayerConnected && !AutoPlatformId.IsEmpty())
			{
				if (FBanManager::BanPlatformId(AutoPlatformId))
				{
					bAutoIdBanned = true;
				}
				else
				{
					bIdAlreadyBanned = true;
				}
			}
			else if (bPlayerConnected && AutoPlatformId.IsEmpty())
			{
				bPuidResolving = true;
			}

			const bool bEosOk = IsEOSPlatformOperational();
			FString PlatformIdNote;
			if (bAutoIdBanned)
			{
				PlatformIdNote = FString::Printf(
					TEXT(" Platform ID %s (%s) also banned automatically."),
					*AutoPlatformId, *AutoPlatformName);
			}
			else if (bIdAlreadyBanned)
			{
				PlatformIdNote = FString::Printf(
					TEXT(" Platform ID %s (%s) was already banned."),
					*AutoPlatformId, *AutoPlatformName);
			}
			else if (bPuidResolving && bEosOk)
			{
				PlatformIdNote = FString::Printf(
					TEXT(" WARNING: player is connected (%s) but platform ban ID not yet available. "
					     "Use !ban id lookup %s then !ban id add <id> once it resolves."),
					*AutoPlatformName, *Arg);
			}
			else if (bPuidResolving && !bEosOk)
			{
				PlatformIdNote = TEXT(" WARNING: EOS platform unavailable — cannot auto-ban by platform ID.");
			}
			else // !bPlayerConnected
			{
				PlatformIdNote = TEXT(" (Player not connected — banned by name only.)");
			}

			if (FBanManager::IsEnabled())
			{
				const int32 Kicked = KickConnectedBannedPlayers(Arg);
				Response = FString::Printf(
					TEXT("%s has been banned from the server.%s%s"),
					*Arg,
					Kicked > 0 ? TEXT(" They have been kicked.") : TEXT(""),
					*PlatformIdNote);
			}
			else
			{
				Response = FString::Printf(
					TEXT("%s has been added to the ban list. "
					     "WARNING: ban system is disabled — run !ban on to enforce bans.%s"),
					*Arg, *PlatformIdNote);
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
		// Platform status – reports EOSIntegration subsystem health, the
		// active platform type, and separate counts of Steam64 ID bans and
		// EOS PUID bans so admins can tell which types are actually enforced.
		const int32 InGameSteamCount  = FBanManager::GetPlatformIdsByType(
			FBanManager::EPlatformIdType::Steam).Num();
		const int32 InGameEosPuidCount = FBanManager::GetPlatformIdsByType(
			FBanManager::EPlatformIdType::Epic).Num();
		const bool bStatusPlatformOk = IsEOSPlatformOperational();
		FString EosState;
		if (!bStatusPlatformOk)
		{
			if (IsEOSPlatformConfigured())
			{
				EosState = FString::Printf(
					TEXT("Platform (EOS): DEGRADED | Steam64 bans:%d (fallback) | EOS PUID bans:%d INACTIVE"),
					InGameSteamCount, InGameEosPuidCount);
			}
			else
			{
				EosState = FString::Printf(
					TEXT("Platform: not configured | Steam64:%d/EOS:%d stored, inactive"),
					InGameSteamCount, InGameEosPuidCount);
			}
		}
		else if (IsSteamPlatformConfigured())
		{
			EosState = FString::Printf(
				TEXT("Platform (Steam): active | Steam64 bans:%d enforced | EOS PUID bans:%d (not applicable on Steam servers)"),
				InGameSteamCount, InGameEosPuidCount);
		}
		else
		{
			EosState = FString::Printf(
				TEXT("Platform (EOS): active | EOS PUID bans:%d enforced | Steam64 bans:%d (may not match EOS PUIDs – use !ban id lookup)"),
				InGameEosPuidCount, InGameSteamCount);
		}
		Response = FString::Printf(
			TEXT("Ban system: %s | Whitelist: %s | %s | Periodic scan: %s  (each system is independent)"),
			*BanState, *WhitelistState, *EosState,
			Config.BanScanIntervalSeconds > 0.0f
				? *FString::Printf(TEXT("every %.0f s"),
				                   FMath::Max(Config.BanScanIntervalSeconds, 30.0f))
				: TEXT("disabled"));
	}
	else if (Verb == TEXT("players"))
	{
		// List connected players with platform and ban ID for in-game admin lookup.
		TArray<FString> Lines;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				for (APlayerState* ConnPS : GS->PlayerArray)
				{
					if (!ConnPS) { continue; }
					const FString ConnName = ConnPS->GetPlayerName();
					const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
					const FString PlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
					const FString PlatformId   = GetPlayerPlatformId(ConnPS);

					const FString NameBanFlag = FBanManager::IsBanned(ConnName)      ? TEXT(" [BANNED]")    : TEXT("");
					const FString IdBanFlag   = (!PlatformId.IsEmpty() && FBanManager::IsPlatformIdBanned(PlatformId))
					                            ? TEXT(" [BANNED-ID]") : TEXT("");

					if (PlatformId.IsEmpty())
					{
						Lines.Add(FString::Printf(TEXT("%s | %s | resolving...%s%s"),
							*ConnName, *PlatformName, *NameBanFlag, *IdBanFlag));
					}
					else
					{
						const FString InGameIdLabel = FBanManager::GetPlatformTypeLabel(PlatformId);
						Lines.Add(FString::Printf(TEXT("%s | %s | %s: %s%s%s"),
							*ConnName, *PlatformName, *InGameIdLabel, *PlatformId,
							*NameBanFlag, *IdBanFlag));
					}
				}
			}
		}

		if (Lines.Num() == 0)
		{
			Response = TEXT("No players currently connected.");
		}
		else
		{
			Response = FString::Printf(
				TEXT("%d player(s) connected: %s"),
				Lines.Num(), *FString::Join(Lines, TEXT(" | ")));
		}
	}
	else if (Verb == TEXT("check"))
	{
		// Check if a player name is banned; also check their platform ban ID if connected.
		// Usage: !ban check <PlayerName>
		if (Arg.IsEmpty())
		{
			Response = TEXT("Usage: !ban check <PlayerName> – checks ban status by name and platform ID");
		}
		else
		{
			const bool bNameBanned = FBanManager::IsBanned(Arg);

			FString ConnectedPlatformId;
			FString ConnectedPlatformName;
			bool bConnected = false;
			const FString CheckTarget = Arg.ToLower();
			if (UWorld* World = GetWorld())
			{
				if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
				{
					for (APlayerState* ConnPS : GS->PlayerArray)
					{
						if (!ConnPS) { continue; }
						if (ConnPS->GetPlayerName().ToLower() != CheckTarget) { continue; }
						ConnectedPlatformId   = GetPlayerPlatformId(ConnPS);
						const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
						ConnectedPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
						bConnected = true;
						break;
					}
				}
			}

			const bool bIdBanned = !ConnectedPlatformId.IsEmpty()
			                       && FBanManager::IsPlatformIdBanned(ConnectedPlatformId);

			FString BanStatus;
			if (bNameBanned && bIdBanned)       { BanStatus = TEXT("BANNED (name+ID)"); }
			else if (bNameBanned)               { BanStatus = TEXT("BANNED (name)"); }
			else if (bIdBanned)                 { BanStatus = FString::Printf(TEXT("BANNED (ID: %s)"), *ConnectedPlatformId); }
			else                                { BanStatus = TEXT("not banned"); }

			const FString ConnStatus = bConnected
				? FString::Printf(TEXT("connected (%s, %s)"),
				                  *ConnectedPlatformName,
				                  ConnectedPlatformId.IsEmpty() ? TEXT("platform ID resolving") : *ConnectedPlatformId)
				: TEXT("not connected");

			Response = FString::Printf(
				TEXT("Ban check '%s': %s | %s"),
				*Arg, *BanStatus, *ConnStatus);
		}
	}
	else if (Verb == TEXT("id"))
	{
		// Platform ID (Steam / Epic Games) ban sub-commands via in-game chat.
		// EOS is the platform service on CSS UnrealEngine-CSS: both Steam and
		// EGS players share an EOS PUID that is used as the ban key.
		// Usage: !ban id lookup <name> | !ban id add <id> | !ban id remove <id> | !ban id list
		FString IdVerb, TargetId;
		if (!Arg.Split(TEXT(" "), &IdVerb, &TargetId, ESearchCase::IgnoreCase))
		{
			IdVerb   = Arg.TrimStartAndEnd();
			TargetId = TEXT("");
		}
		IdVerb   = IdVerb.TrimStartAndEnd().ToLower();
		TargetId = FBanManager::NormalizePlatformId(TargetId.TrimStartAndEnd());

		const FString EosWarnNote = IsEOSPlatformOperational()
			? TEXT("")
			: FString::Printf(
			      TEXT(" WARNING: EOS unavailable – ban stored but NOT enforced at join. "
			           "Use %s eos for details."),
			      *Config.InGameServerInfoCommandPrefix);

		if (IdVerb == TEXT("lookup"))
		{
			// Look up the platform ban ID (EOS PUID or Steam64 ID) of a connected
			// player by name.  On EOS-mode servers the result is an EOS PUID;
			// on Steam-mode servers it is a Steam64 ID.
			if (TargetId.IsEmpty())
			{
				Response = TEXT("Usage: !ban id lookup <PlayerName>  "
				                "– shows platform ban ID (EOS PUID on EOS-mode servers, "
				                "Steam64 ID on Steam-mode servers) and the ban command to use");
			}
			else
			{
				const FString LookupTarget = TargetId.ToLower();
				FString FoundId;
				FString FoundPlatformName;
				bool bFound = false;

				if (UWorld* World = GetWorld())
				{
					if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
					{
						for (APlayerState* ConnPS : GS->PlayerArray)
						{
							if (!ConnPS) { continue; }
							if (ConnPS->GetPlayerName().ToLower() != LookupTarget) { continue; }
							FoundId = GetPlayerPlatformId(ConnPS);
							const AFGPlayerState* FGPS = Cast<const AFGPlayerState>(ConnPS);
							FoundPlatformName = FGPS ? FGPS->GetPlayingPlatformName() : TEXT("Unknown");
							bFound = true;
							break;
						}
					}
				}

				if (!bFound)
				{
					Response = FString::Printf(
						TEXT("No connected player found with name '%s'."), *TargetId);
				}
				else if (FoundId.IsEmpty())
				{
					if (EosWarnNote.IsEmpty())
					{
						// EOS is operational – platform ban ID temporarily unavailable.
						// mIsOnline may not have resolved yet; retrying should work.
						Response = FString::Printf(
							TEXT("Player '%s' (%s) is connected but platform ban ID is not yet available. "
							     "Wait a moment and retry."),
							*TargetId, *FoundPlatformName);
					}
					else
					{
						// EOS is NOT operational – waiting will not help.
						Response = FString::Printf(
							TEXT("Player '%s' (%s) is connected but platform ban ID is unavailable "
							     "(EOS service not operational).%s"),
							*TargetId, *FoundPlatformName, *EosWarnNote);
					}
				}
				else
				{
					// Use the correct ID-type label and ban command based on ID format.
					const FString InGameIdLabel   = FBanManager::GetPlatformTypeLabel(FoundId);
					const FString InGameBanSubCmd = FBanManager::IsValidSteam64Id(FoundId)
						? TEXT("steam add") : TEXT("id add");
					Response = FString::Printf(
						TEXT("'%s' (%s) %s: %s  – to ban: !ban %s %s"),
						*TargetId, *FoundPlatformName, *InGameIdLabel, *FoundId,
						*InGameBanSubCmd, *FoundId);
					if (!EosWarnNote.IsEmpty())
					{
						Response += EosWarnNote;
					}
				}
			}
		}
		else if (IdVerb == TEXT("add"))
		{
			if (TargetId.IsEmpty())
			{
				Response = TEXT("Usage: !ban id add <platform_id>\n"
				                "  Steam64 ID: 17-digit number starting with 765 (e.g. 76561198123456789)\n"
				                "  EOS PUID:   hex string (e.g. 0002abcdef1234567890abcdef123456)\n"
				                "  X-FactoryGame-PlayerId header: raw binary-hex also accepted (auto-normalized).");
			}
			else if (FBanManager::BanPlatformId(TargetId))
			{
				const FString PlatformLabel = FBanManager::GetPlatformTypeLabel(TargetId);
				if (FBanManager::IsEnabled())
				{
					// Kick any currently-connected player whose platform ID matches.
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase*  GM = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							// Snapshot PlayerArray to prevent iterator invalidation:
							// KickPlayer() → Destroy() → RemovePlayerState() modifies
							// PlayerArray synchronously in CSS UnrealEngine-CSS (UE5.3).
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != TargetId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT("Platform ID %s (%s) has been banned.%s"),
						*TargetId, *PlatformLabel,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT("Platform ID %s (%s) has been added to the ban list. "
						     "WARNING: ban system is disabled — run !ban on to enforce bans."),
						*TargetId, *PlatformLabel);
				}
				if (!EosWarnNote.IsEmpty())
				{
					Response += EosWarnNote;
				}
			}
			else
			{
				Response = FString::Printf(TEXT("Platform ID %s (%s) is already banned."),
				                           *TargetId, *FBanManager::GetPlatformTypeLabel(TargetId));
			}
		}
		else if (IdVerb == TEXT("remove"))
		{
			if (TargetId.IsEmpty())
			{
				Response = TEXT("Usage: !ban id remove <platform_id>");
			}
			else if (FBanManager::UnbanPlatformId(TargetId))
			{
				Response = FString::Printf(TEXT("Platform ID %s has been unbanned."), *TargetId);
			}
			else
			{
				Response = FString::Printf(TEXT("Platform ID %s was not on the ban list."), *TargetId);
			}
		}
		else if (IdVerb == TEXT("list"))
		{
			const TArray<FString> AllIds = FBanManager::GetAllPlatformIds();
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (AllIds.Num() == 0)
			{
				Response = FString::Printf(TEXT("Ban system is %s. No platform IDs banned."), *Status);
			}
			else
			{
				TArray<FString> Lines;
				for (const FString& Id : AllIds)
				{
					Lines.Add(FString::Printf(TEXT("%s (%s)"), *Id, *FBanManager::GetPlatformTypeLabel(Id)));
				}
				Response = FString::Printf(
					TEXT("Ban system is %s. Banned platform IDs (%d): %s"),
					*Status, AllIds.Num(), *FString::Join(Lines, TEXT(", ")));
			}
			if (!EosWarnNote.IsEmpty())
			{
				Response += EosWarnNote;
			}
		}
		else
		{
			Response = TEXT("Usage: !ban id lookup <name>, !ban id add <platform_id>, "
			                "!ban id remove <platform_id>, !ban id list\n"
			                "Tip: use !ban steam or !ban epic for platform-specific sub-commands.");
		}
	}
	else if (Verb == TEXT("steam"))
	{
		// In-game: Steam64 ID ban sub-commands.
		// Usage: !ban steam add <steam64_id> | !ban steam remove <steam64_id> | !ban steam list
		FString SteamVerb, SteamId;
		if (!Arg.Split(TEXT(" "), &SteamVerb, &SteamId, ESearchCase::IgnoreCase))
		{
			SteamVerb = Arg.TrimStartAndEnd();
			SteamId   = TEXT("");
		}
		SteamVerb = SteamVerb.TrimStartAndEnd().ToLower();
		SteamId   = FBanManager::NormalizePlatformId(SteamId.TrimStartAndEnd());

		if (SteamVerb == TEXT("add"))
		{
			if (SteamId.IsEmpty())
			{
				Response = TEXT("Usage: !ban steam add <steam64_id>  "
				                "(17-digit number starting with 765, e.g. 76561198123456789)");
			}
			else if (!FBanManager::IsValidSteam64Id(SteamId))
			{
				Response = FString::Printf(
					TEXT("'%s' is not a valid Steam64 ID (must be 17 decimal digits starting with 765). "
					     "For EOS PUIDs use !ban epic add <eos_puid>."),
					*SteamId);
			}
			else if (FBanManager::BanPlatformId(SteamId))
			{
				if (FBanManager::IsEnabled())
				{
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase*  GM = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != SteamId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT("Steam64 ID %s has been banned.%s"),
						*SteamId,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT("Steam64 ID %s has been added to the ban list. "
						     "WARNING: ban system is disabled — run !ban on to enforce bans."),
						*SteamId);
				}
			}
			else
			{
				Response = FString::Printf(TEXT("Steam64 ID %s is already banned."), *SteamId);
			}
		}
		else if (SteamVerb == TEXT("remove"))
		{
			if (SteamId.IsEmpty())
			{
				Response = TEXT("Usage: !ban steam remove <steam64_id>");
			}
			else if (!FBanManager::IsValidSteam64Id(SteamId))
			{
				Response = FString::Printf(
					TEXT("'%s' is not a valid Steam64 ID (must be 17 decimal digits starting with 765)."),
					*SteamId);
			}
			else if (FBanManager::UnbanPlatformId(SteamId))
			{
				Response = FString::Printf(TEXT("Steam64 ID %s has been unbanned."), *SteamId);
			}
			else
			{
				Response = FString::Printf(TEXT("Steam64 ID %s was not on the ban list."), *SteamId);
			}
		}
		else if (SteamVerb == TEXT("list"))
		{
			const TArray<FString> SteamIds = FBanManager::GetPlatformIdsByType(
				FBanManager::EPlatformIdType::Steam);
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (SteamIds.Num() == 0)
			{
				Response = FString::Printf(TEXT("Ban system is %s. No Steam64 IDs banned."), *Status);
			}
			else
			{
				Response = FString::Printf(
					TEXT("Ban system is %s. Banned Steam64 IDs (%d): %s"),
					*Status, SteamIds.Num(), *FString::Join(SteamIds, TEXT(", ")));
			}
		}
		else
		{
			Response = TEXT("Usage: !ban steam add <steam64_id>, !ban steam remove <steam64_id>, !ban steam list\n"
			                "Steam64 IDs are 17-digit numbers starting with 765.");
		}
	}
	else if (Verb == TEXT("epic"))
	{
		// In-game: EOS PUID ban sub-commands.
		// Usage: !ban epic add <eos_puid> | !ban epic remove <eos_puid> | !ban epic list
		FString EpicVerb, EpicId;
		if (!Arg.Split(TEXT(" "), &EpicVerb, &EpicId, ESearchCase::IgnoreCase))
		{
			EpicVerb = Arg.TrimStartAndEnd();
			EpicId   = TEXT("");
		}
		EpicVerb = EpicVerb.TrimStartAndEnd().ToLower();
		EpicId   = FBanManager::NormalizePlatformId(EpicId.TrimStartAndEnd());

		const FString EosWarnEpic = IsEOSPlatformOperational()
			? TEXT("")
			: TEXT(" WARNING: EOS unavailable – ban stored but NOT enforced at join.");

		if (EpicVerb == TEXT("add"))
		{
			if (EpicId.IsEmpty())
			{
				Response = TEXT("Usage: !ban epic add <eos_puid>  "
				                "(EOS Product User ID — covers Steam and Epic on EOS servers. "
				                "Use !ban id lookup <name> to find it.)");
			}
			else if (FBanManager::IsValidSteam64Id(EpicId))
			{
				Response = FString::Printf(
					TEXT("'%s' looks like a Steam64 ID, not an EOS PUID. "
					     "For Steam-mode servers use !ban steam add %s instead."),
					*EpicId, *EpicId);
			}
			else if (FBanManager::BanPlatformId(EpicId))
			{
				if (FBanManager::IsEnabled())
				{
					UWorld* World = GetWorld();
					int32 Kicked = 0;
					if (World)
					{
						AGameStateBase* GS = World->GetGameState<AGameStateBase>();
						AGameModeBase*  GM = World->GetAuthGameMode<AGameModeBase>();
						if (GS && GM && GM->GameSession)
						{
							const FString KickReason = Config.BanKickReason.IsEmpty()
								? TEXT("You are banned from this server.")
								: Config.BanKickReason;
							for (APlayerState* ConnPS : TArray<APlayerState*>(GS->PlayerArray))
							{
								if (!ConnPS) { continue; }
								const FString ConnId = GetPlayerPlatformId(ConnPS);
								if (ConnId.IsEmpty() || ConnId.ToLower() != EpicId.ToLower()) { continue; }
								APlayerController* PC = Cast<APlayerController>(ConnPS->GetOwner());
								if (!PC || PC->IsLocalController()) { continue; }
								GM->GameSession->KickPlayer(PC, FText::FromString(KickReason));
								++Kicked;
							}
						}
					}
					Response = FString::Printf(
						TEXT("EOS PUID %s has been banned.%s"),
						*EpicId,
						Kicked > 0 ? TEXT(" The player has been kicked.") : TEXT(""));
				}
				else
				{
					Response = FString::Printf(
						TEXT("EOS PUID %s has been added to the ban list. "
						     "WARNING: ban system is disabled — run !ban on to enforce bans."),
						*EpicId);
				}
				if (!EosWarnEpic.IsEmpty())
				{
					Response += EosWarnEpic;
				}
			}
			else
			{
				Response = FString::Printf(TEXT("EOS PUID %s is already banned."), *EpicId);
			}
		}
		else if (EpicVerb == TEXT("remove"))
		{
			if (EpicId.IsEmpty())
			{
				Response = TEXT("Usage: !ban epic remove <eos_puid>");
			}
			else if (FBanManager::UnbanPlatformId(EpicId))
			{
				Response = FString::Printf(TEXT("EOS PUID %s has been unbanned."), *EpicId);
			}
			else
			{
				Response = FString::Printf(TEXT("EOS PUID %s was not on the ban list."), *EpicId);
			}
		}
		else if (EpicVerb == TEXT("list"))
		{
			const TArray<FString> EpicIds = FBanManager::GetPlatformIdsByType(
				FBanManager::EPlatformIdType::Epic);
			const FString Status = FBanManager::IsEnabled() ? TEXT("ENABLED") : TEXT("disabled");
			if (EpicIds.Num() == 0)
			{
				Response = FString::Printf(TEXT("Ban system is %s. No EOS PUIDs banned."), *Status);
			}
			else
			{
				Response = FString::Printf(
					TEXT("Ban system is %s. Banned EOS PUIDs (%d): %s"),
					*Status, EpicIds.Num(), *FString::Join(EpicIds, TEXT(", ")));
			}
			if (!EosWarnEpic.IsEmpty())
			{
				Response += EosWarnEpic;
			}
		}
		else
		{
			Response = TEXT("Usage: !ban epic add <eos_puid>, !ban epic remove <eos_puid>, !ban epic list\n"
			                "EOS PUIDs are hex strings used on EOS servers for both Steam and Epic accounts.");
		}
	}
	else
	{
		Response = TEXT("Unknown ban command. Available sub-commands:\n"
		                "Name: add <name>, remove <name>, list, check <name>\n"
		                "Platform ID: id lookup <name>, id add <id>, id remove <id>, id list\n"
		                "Steam64 ID: steam add <steam64_id>, steam remove <steam64_id>, steam list\n"
		                "EOS PUID: epic add <eos_puid>, epic remove <eos_puid>, epic list\n"
		                "Status: on, off, status, players");
	}

	SendGameChatStatusMessage(Response);
}

// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleInGameKickCommand(const FString& SubCommand)
{
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: In-game kick command: '%s'"), *SubCommand);

	if (SubCommand.IsEmpty())
	{
		SendGameChatStatusMessage(FString::Printf(
		    TEXT("Usage: %s <PlayerName> [reason]  (kicks without banning)"),
		    *Config.InGameKickCommandPrefix));
		return;
	}

	// Parse optional reason: everything after the first word is the reason.
	FString TargetName;
	FString Reason;
	if (!SubCommand.Split(TEXT(" "), &TargetName, &Reason, ESearchCase::IgnoreCase))
	{
		TargetName = SubCommand.TrimStartAndEnd();
		Reason     = TEXT("");
	}
	TargetName = TargetName.TrimStartAndEnd();
	Reason     = Reason.TrimStartAndEnd();

	const FString EffectiveReason = Reason.IsEmpty()
		? (Config.KickReason.IsEmpty() ? TEXT("You have been kicked from the server by an admin.") : Config.KickReason)
		: Reason;

	UWorld* World = GetWorld();
	if (!World)
	{
		SendGameChatStatusMessage(TEXT("Error: game world unavailable."));
		return;
	}

	AGameStateBase* GS = World->GetGameState<AGameStateBase>();
	if (!GS)
	{
		SendGameChatStatusMessage(TEXT("Error: game state unavailable."));
		return;
	}

	AGameModeBase* GM = World->GetAuthGameMode<AGameModeBase>();
	if (!GM || !GM->GameSession)
	{
		SendGameChatStatusMessage(TEXT("Error: game session unavailable."));
		return;
	}

	const FString LowerTarget = TargetName.ToLower();
	APlayerController* FoundPC = nullptr;
	const TArray<APlayerState*> Snapshot = GS->PlayerArray;
	for (APlayerState* PS : Snapshot)
	{
		if (!PS) { continue; }
		if (PS->GetPlayerName().ToLower() != LowerTarget) { continue; }
		APlayerController* PC = Cast<APlayerController>(PS->GetOwner());
		if (!PC || PC->IsLocalController()) { continue; }
		FoundPC = PC;
		break;
	}

	if (!FoundPC)
	{
		SendGameChatStatusMessage(FString::Printf(
		    TEXT("Player '%s' is not currently connected."), *TargetName));
		return;
	}

	UE_LOG(LogDiscordBridge, Warning,
	       TEXT("DiscordBridge: In-game kick: kicking '%s'. Reason: %s"),
	       *TargetName, *EffectiveReason);

	GM->GameSession->KickPlayer(FoundPC, FText::FromString(EffectiveReason));

	SendGameChatStatusMessage(FString::Printf(
	    TEXT("Kicked '%s'. Reason: %s"), *TargetName, *EffectiveReason));

	// Post a Discord notification if configured.
	FString DiscordNotice = Config.KickDiscordMessage;
	if (!DiscordNotice.IsEmpty())
	{
		DiscordNotice.ReplaceInline(TEXT("%PlayerName%"), *TargetName, ESearchCase::IgnoreCase);
		DiscordNotice.ReplaceInline(TEXT("%Reason%"),     *EffectiveReason, ESearchCase::IgnoreCase);
		const TArray<FString> AllChannels = FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId);
		for (const FString& Ch : AllChannels)
		{
			SendMessageToChannel(Ch, DiscordNotice);
		}
	}
}

// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleInGameServerInfoCommand(const FString& SubCommand)
{
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: In-game server-info command: '%s'"), *SubCommand);

	const FString Verb = SubCommand.TrimStartAndEnd().ToLower();

	FString Response;

	if (Verb == TEXT("players") || Verb == TEXT("online"))
	{
		TArray<FString> Names;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				for (APlayerState* PS : GS->PlayerArray)
				{
					if (!PS) { continue; }
					const FString Name = PS->GetPlayerName();
					if (!Name.IsEmpty()) { Names.Add(Name); }
				}
			}
		}

		if (Names.Num() == 0)
		{
			Response = TEXT("No players are currently online.");
		}
		else
		{
			Response = FString::Printf(
				TEXT("%d player(s) online: %s"),
				Names.Num(), *FString::Join(Names, TEXT(", ")));
		}
	}
	else if (Verb == TEXT("eos"))
	{
		// BuildPlatformDiagnostics() uses OnlineIntegration and GConfig directly.
		const int32 PlatformIdCount = FBanManager::GetAllPlatformIds().Num();
		const FString BanLine = FBanManager::IsEnabled()
			? FString::Printf(TEXT("Ban system: enabled — %d platform ID%s currently banned (Steam & Epic)"),
			                  PlatformIdCount, PlatformIdCount == 1 ? TEXT("") : TEXT("s"))
			: FString::Printf(TEXT("Ban system: disabled — %d platform ID%s in list (not enforced)"),
			                  PlatformIdCount, PlatformIdCount == 1 ? TEXT("") : TEXT("s"));
		Response = BuildPlatformDiagnostics(/*bPlainText=*/true) + TEXT("\nBan system:\n") + BanLine;
	}
	else if (Verb == TEXT("status") || Verb.IsEmpty())
	{
		int32 PlayerCount = 0;
		if (UWorld* World = GetWorld())
		{
			if (AGameStateBase* GS = World->GetGameState<AGameStateBase>())
			{
				PlayerCount = GS->PlayerArray.Num();
			}
		}

		const FString ServerLabel = Config.ServerName.IsEmpty()
			? TEXT("Satisfactory Server")
			: Config.ServerName;

		Response = FString::Printf(
			TEXT("%s is online | Players: %d"),
			*ServerLabel, PlayerCount);

		// Append a plain-text platform warning when the platform is unavailable.
		if (!IsEOSPlatformOperational())
		{
			Response += FString::Printf(
				TEXT(" | WARNING: EOS platform unavailable – ban-by-ID inactive. "
				     "Use %s eos for diagnostics."),
				*Config.InGameServerInfoCommandPrefix);
		}
	}
	else if (Verb == TEXT("help"))
	{
		FString Help = FString::Printf(
			TEXT("Server-info commands (%s):\n"),
			*Config.InGameServerInfoCommandPrefix);
		Help += FString::Printf(TEXT("  %s players – list online players\n"),
		                        *Config.InGameServerInfoCommandPrefix);
		Help += FString::Printf(TEXT("  %s status  – server status and player count\n"),
		                        *Config.InGameServerInfoCommandPrefix);
		Help += FString::Printf(TEXT("  %s eos     – EOS/platform diagnostics\n"),
		                        *Config.InGameServerInfoCommandPrefix);
		Help += FString::Printf(TEXT("  %s help    – show this message"),
		                        *Config.InGameServerInfoCommandPrefix);
		Response = Help;
	}
	else
	{
		Response = FString::Printf(
			TEXT("Unknown server-info command '%s'. Try: %s help"),
			*SubCommand, *Config.InGameServerInfoCommandPrefix);
	}

	SendGameChatStatusMessage(Response);
}

// =============================================================================
// Button-based ticket panel – interaction handling and channel management
// =============================================================================

// ─────────────────────────────────────────────────────────────────────────────
// Helper: sanitize a Discord username into a valid channel-name segment.
// Discord channel names must be 1-100 chars, lowercase, containing only
// letters, digits, and dashes.  Spaces and underscores become dashes;
// everything else is stripped.  The result is clamped to 40 characters.
// ─────────────────────────────────────────────────────────────────────────────
static FString SanitizeUsernameForChannel(const FString& Username)
{
	FString Result;
	for (TCHAR C : Username)
	{
		if (FChar::IsAlnum(C))
		{
			Result.AppendChar(FChar::ToLower(C));
		}
		else if (C == TCHAR(' ') || C == TCHAR('_') || C == TCHAR('-'))
		{
			Result.AppendChar(TCHAR('-'));
		}
		// All other characters are silently dropped.
	}

	// Collapse consecutive dashes and trim leading/trailing dashes.
	while (Result.Contains(TEXT("--")))
	{
		Result = Result.Replace(TEXT("--"), TEXT("-"), ESearchCase::CaseSensitive);
	}
	Result = Result.TrimStartAndEnd();
	if (Result.StartsWith(TEXT("-")))
	{
		Result = Result.Mid(1);
	}
	if (Result.EndsWith(TEXT("-")))
	{
		Result = Result.Left(Result.Len() - 1);
	}

	// Clamp to 40 characters so the full channel name fits within Discord's
	// 100-character limit (prefix "ticket-whitelist-" is 18 chars at most).
	if (Result.Len() > 40)
	{
		Result = Result.Left(40);
	}

	// Fall back to "user" when the username contained no valid characters.
	return Result.IsEmpty() ? TEXT("user") : Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: send a pre-built JSON message body to a Discord channel via REST.
// Used by PostTicketPanel and CreateTicketChannel for messages that contain
// components (buttons) in addition to plain text.
// ─────────────────────────────────────────────────────────────────────────────
static void SendMessageBodyToChannelImpl(const FString& BotToken,
                                         const FString& TargetChannelId,
                                         const TSharedPtr<FJsonObject>& MessageBody)
{
	if (BotToken.IsEmpty() || TargetChannelId.IsEmpty() || !MessageBody.IsValid())
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
	                   FString::Printf(TEXT("Bot %s"), *BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindLambda(
	[TargetChannelId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!bConnected || !Resp.IsValid())
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: HTTP request failed sending to channel %s."),
			       *TargetChannelId);
			return;
		}
		if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Discord REST returned %d sending to channel %s: %s"),
			       Resp->GetResponseCode(), *TargetChannelId,
			       *Resp->GetContentAsString());
		}
	});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Build and return the Discord message-components JSON array for the close
// button.  The custom_id embeds the opener's user ID so the interaction
// handler can verify who is allowed to close the ticket.
// ─────────────────────────────────────────────────────────────────────────────
static TSharedPtr<FJsonObject> MakeCloseButtonMessage(const FString& OpenerUserId)
{
	// Button object
	TSharedPtr<FJsonObject> CloseButton = MakeShared<FJsonObject>();
	CloseButton->SetNumberField(TEXT("type"), 2);   // BUTTON
	CloseButton->SetNumberField(TEXT("style"), 4);  // DANGER (red)
	CloseButton->SetStringField(TEXT("label"), TEXT("Close Ticket"));
	CloseButton->SetStringField(TEXT("custom_id"),
	FString::Printf(TEXT("ticket_close:%s"), *OpenerUserId));

	// Action row containing the button
	TSharedPtr<FJsonObject> ActionRow = MakeShared<FJsonObject>();
	ActionRow->SetNumberField(TEXT("type"), 1);
	ActionRow->SetArrayField(TEXT("components"),
	TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(CloseButton) });

	// Full message body
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"),
	TEXT(":lock: Click **Close Ticket** when the issue is resolved. "
	     "The channel will be deleted automatically.\n"
	     ":information_source: The ticket opener and any member with the "
	     "admin/support role can close this ticket."));
	Body->SetArrayField(TEXT("components"),
	TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(ActionRow) });

	return Body;
}

// ─────────────────────────────────────────────────────────────────────────────
// INTERACTION_CREATE gateway event handler
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleInteractionCreate(const TSharedPtr<FJsonObject>& DataObj)
{
	// interaction types we handle:
	//   3 = MESSAGE_COMPONENT  (button click on the ticket panel)
	//   5 = MODAL_SUBMIT       (user submitted the pre-ticket reason modal)
	double TypeD = 0.0;
	DataObj->TryGetNumberField(TEXT("type"), TypeD);
	const int32 InteractionType = static_cast<int32>(TypeD);

	if (InteractionType != 3 && InteractionType != 5)
	{
		return;
	}

	FString InteractionId;
	FString InteractionToken;
	DataObj->TryGetStringField(TEXT("id"),    InteractionId);
	DataObj->TryGetStringField(TEXT("token"), InteractionToken);

	// Extract custom_id from the interaction data object.
	const TSharedPtr<FJsonObject>* InteractionDataPtr = nullptr;
	if (!DataObj->TryGetObjectField(TEXT("data"), InteractionDataPtr) || !InteractionDataPtr)
	{
		return;
	}
	FString CustomId;
	(*InteractionDataPtr)->TryGetStringField(TEXT("custom_id"), CustomId);

	// Only process custom IDs that belong to our ticket system.
	if (!CustomId.StartsWith(TEXT("ticket_")))
	{
		return;
	}

	// Extract the member object (guild interactions always include it).
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	DataObj->TryGetObjectField(TEXT("member"), MemberPtr);

	FString DiscordUserId;
	FString DiscordUsername;

	if (MemberPtr && (*MemberPtr).IsValid())
	{
		const TSharedPtr<FJsonObject>* UserPtr = nullptr;
		if ((*MemberPtr)->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
		{
			(*UserPtr)->TryGetStringField(TEXT("id"), DiscordUserId);

			// Display name priority: global_name > username
			if (!(*UserPtr)->TryGetStringField(TEXT("global_name"), DiscordUsername)
			    || DiscordUsername.IsEmpty())
			{
				(*UserPtr)->TryGetStringField(TEXT("username"), DiscordUsername);
			}
		}

		// Server nickname overrides global/username display name.
		FString Nick;
		if ((*MemberPtr)->TryGetStringField(TEXT("nick"), Nick) && !Nick.IsEmpty())
		{
			DiscordUsername = Nick;
		}
	}

	if (DiscordUsername.IsEmpty())
	{
		DiscordUsername = TEXT("Discord User");
	}

	// Collect the member's role IDs.
	TArray<FString> MemberRoles;
	if (MemberPtr && (*MemberPtr).IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
		{
			for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
			{
				FString RoleId;
				if (RoleVal->TryGetString(RoleId))
				{
					MemberRoles.Add(RoleId);
				}
			}
		}
	}

	FString SourceChannelId;
	DataObj->TryGetStringField(TEXT("channel_id"), SourceChannelId);

	if (InteractionType == 5) // MODAL_SUBMIT
	{
		HandleTicketModalSubmit(InteractionId, InteractionToken, CustomId,
		                        *InteractionDataPtr,
		                        DiscordUserId, DiscordUsername);
	}
	else // MESSAGE_COMPONENT (type 3)
	{
		HandleTicketButtonInteraction(InteractionId, InteractionToken, CustomId,
		                              DiscordUserId, DiscordUsername,
		                              MemberRoles, SourceChannelId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ticket button click handler
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleTicketButtonInteraction(
const FString& InteractionId,
const FString& InteractionToken,
const FString& CustomId,
const FString& DiscordUserId,
const FString& DiscordUsername,
const TArray<FString>& MemberRoles,
const FString& SourceChannelId)
{
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Ticket button '%s' clicked by '%s' (%s)"),
	       *CustomId, *DiscordUsername, *DiscordUserId);

	// ── Close-ticket button ───────────────────────────────────────────────────
	if (CustomId.StartsWith(TEXT("ticket_close:")))
	{
		// custom_id format: ticket_close:{opener_user_id}
		const FString OpenerUserId = CustomId.Mid(FCString::Strlen(TEXT("ticket_close:")));

		// The source channel of the interaction IS the ticket channel.
		// Verify it is actually tracked as an active ticket channel.
		if (!TicketChannelToOpener.Contains(SourceChannelId))
		{
			// The channel may no longer be tracked (e.g. after a server restart).
			// Still allow the close so the channel gets cleaned up.
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Close button in untracked channel %s – proceeding with deletion."),
			       *SourceChannelId);
		}

		// Authorisation: ticket opener OR a member holding the admin/support role
		// OR the guild owner (who always has full administrative access).
		const bool bIsOpener = (!DiscordUserId.IsEmpty() && DiscordUserId == OpenerUserId);
		bool bIsAdmin = (!GuildOwnerId.IsEmpty() && DiscordUserId == GuildOwnerId);
		if (!bIsAdmin && !Config.TicketNotifyRoleId.IsEmpty())
		{
			bIsAdmin = MemberRoles.Contains(Config.TicketNotifyRoleId);
		}

		if (!bIsOpener && !bIsAdmin)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Only the ticket opener or an admin with the support role can close this ticket."),
			/*bEphemeral=*/true);
			return;
		}

		// Acknowledge the interaction silently (type 6 = DEFERRED_UPDATE_MESSAGE)
		// before deleting the channel so Discord does not show an error.
		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/6, TEXT(""), /*bEphemeral=*/false);

		// Delete the ticket channel.
		DeleteDiscordChannel(SourceChannelId);
		return;
	}

	// ── Open-ticket buttons ───────────────────────────────────────────────────

	// Prevent duplicate tickets: one active ticket per Discord user at a time.
	if (OpenerToTicketChannel.Contains(DiscordUserId))
	{
		const FString ExistingChanId = OpenerToTicketChannel[DiscordUserId];
		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		FString::Printf(
		TEXT(":warning: You already have an open ticket (<#%s>). "
		     "Please continue there, or close it before opening a new one."),
		*ExistingChanId),
		/*bEphemeral=*/true);
		return;
	}

	if (CustomId == TEXT("ticket_wl"))
	{
		if (!Config.bTicketWhitelistEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Whitelist request tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		ShowTicketReasonModal(InteractionId, InteractionToken,
		TEXT("ticket_modal:wl"),
		TEXT("Whitelist Request"),
		TEXT("Describe why you want to join (optional)"));
	}
	else if (CustomId == TEXT("ticket_help"))
	{
		if (!Config.bTicketHelpEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Help tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		ShowTicketReasonModal(InteractionId, InteractionToken,
		TEXT("ticket_modal:help"),
		TEXT("Help / Support"),
		TEXT("Briefly describe your issue (optional)"));
	}
	else if (CustomId == TEXT("ticket_report"))
	{
		if (!Config.bTicketReportEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Player report tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		ShowTicketReasonModal(InteractionId, InteractionToken,
		TEXT("ticket_modal:report"),
		TEXT("Report a Player"),
		TEXT("Describe the incident and the player you are reporting (optional)"));
	}
	else if (CustomId.StartsWith(TEXT("ticket_cr_")))
	{
		// Custom reason button: custom_id format is "ticket_cr_N" where N is the index.
		const int32 ReasonIndex = FCString::Atoi(*CustomId.Mid(FCString::Strlen(TEXT("ticket_cr_"))));

		if (!Config.CustomTicketReasons.IsValidIndex(ReasonIndex))
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Custom ticket reason index %d out of range (have %d reasons)."),
			       ReasonIndex, Config.CustomTicketReasons.Num());
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: This ticket type is no longer available. "
			     "Please contact an admin directly."),
			/*bEphemeral=*/true);
			return;
		}

		FString Label, Desc;
		Config.CustomTicketReasons[ReasonIndex].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			Label = TEXT("Custom");
		}

		// Modal title is limited to 45 characters by Discord.
		FString ModalTitle = Label.Left(45);

		ShowTicketReasonModal(InteractionId, InteractionToken,
		FString::Printf(TEXT("ticket_modal:cr_%d"), ReasonIndex),
		ModalTitle,
		TEXT("Provide any details about your request (optional)"));
	}
	else
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Unrecognised ticket button custom_id: %s"), *CustomId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Respond to a Discord interaction via REST
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::RespondToInteraction(
const FString& InteractionId,
const FString& InteractionToken,
int32 ResponseType,
const FString& Content,
bool bEphemeral)
{
	if (Config.BotToken.IsEmpty() || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	// Build the "data" object (only needed for type 4 – sending a message).
	TSharedPtr<FJsonObject> ResponseData = MakeShared<FJsonObject>();
	if (ResponseType == 4 && !Content.IsEmpty())
	{
		ResponseData->SetStringField(TEXT("content"), Content);
		if (bEphemeral)
		{
			// Discord flags bitmask: 64 = EPHEMERAL (only visible to the clicker).
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
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Interaction response request failed (id=%s)."),
			       *InteractionId);
			return;
		}
		// 200 OK or 204 No Content are both valid success codes for callbacks.
		if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 204)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Interaction callback returned HTTP %d: %s"),
			       Resp->GetResponseCode(), *Resp->GetContentAsString());
		}
	});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Show a ticket reason modal (Discord popup form) before creating a ticket
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::ShowTicketReasonModal(
const FString& InteractionId,
const FString& InteractionToken,
const FString& ModalCustomId,
const FString& ModalTitle,
const FString& PlaceholderText)
{
	if (Config.BotToken.IsEmpty() || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	// Build the text input component (type 4 = TEXT_INPUT, style 2 = PARAGRAPH).
	TSharedPtr<FJsonObject> TextInput = MakeShared<FJsonObject>();
	TextInput->SetNumberField(TEXT("type"),        4); // TEXT_INPUT
	TextInput->SetStringField(TEXT("custom_id"),   TEXT("ticket_reason"));
	TextInput->SetNumberField(TEXT("style"),       2); // PARAGRAPH (multi-line)
	TextInput->SetStringField(TEXT("label"),       TEXT("Reason"));
	TextInput->SetStringField(TEXT("placeholder"), PlaceholderText);
	TextInput->SetBoolField  (TEXT("required"),    false);
	TextInput->SetNumberField(TEXT("max_length"),  1000);

	// Wrap the input in an action row (type 1 = ACTION_ROW).
	TSharedPtr<FJsonObject> ActionRow = MakeShared<FJsonObject>();
	ActionRow->SetNumberField(TEXT("type"), 1);
	TArray<TSharedPtr<FJsonValue>> InputComponents;
	InputComponents.Add(MakeShared<FJsonValueObject>(TextInput));
	ActionRow->SetArrayField(TEXT("components"), InputComponents);

	TArray<TSharedPtr<FJsonValue>> Rows;
	Rows.Add(MakeShared<FJsonValueObject>(ActionRow));

	// Build the modal data object.
	TSharedPtr<FJsonObject> ModalData = MakeShared<FJsonObject>();
	ModalData->SetStringField(TEXT("custom_id"),  ModalCustomId);
	ModalData->SetStringField(TEXT("title"),      ModalTitle);
	ModalData->SetArrayField (TEXT("components"), Rows);

	// Build the interaction callback body (type 9 = MODAL).
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), 9);
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
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: ShowTicketReasonModal request failed (id=%s)."),
			       *InteractionId);
			return;
		}
		if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 204)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: ShowTicketReasonModal callback returned HTTP %d: %s"),
			       Resp->GetResponseCode(), *Resp->GetContentAsString());
		}
	});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Handle MODAL_SUBMIT for ticket reason modal
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::HandleTicketModalSubmit(
const FString& InteractionId,
const FString& InteractionToken,
const FString& ModalCustomId,
const TSharedPtr<FJsonObject>& ModalData,
const FString& DiscordUserId,
const FString& DiscordUsername)
{
	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Ticket modal '%s' submitted by '%s' (%s)"),
	       *ModalCustomId, *DiscordUsername, *DiscordUserId);

	// Extract the user-supplied reason from the submitted modal components.
	// Structure: data.components[0].components[0].value (action row → text input).
	FString Reason;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (ModalData->TryGetArrayField(TEXT("components"), Rows) && Rows)
	{
		for (const TSharedPtr<FJsonValue>& RowVal : *Rows)
		{
			const TSharedPtr<FJsonObject>* RowObj = nullptr;
			if (!RowVal->TryGetObject(RowObj) || !RowObj) { continue; }

			const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
			if (!(*RowObj)->TryGetArrayField(TEXT("components"), Inputs) || !Inputs) { continue; }

			for (const TSharedPtr<FJsonValue>& InputVal : *Inputs)
			{
				const TSharedPtr<FJsonObject>* InputObj = nullptr;
				if (!InputVal->TryGetObject(InputObj) || !InputObj) { continue; }

				FString InputCustomId;
				(*InputObj)->TryGetStringField(TEXT("custom_id"), InputCustomId);
				if (InputCustomId == TEXT("ticket_reason"))
				{
					(*InputObj)->TryGetStringField(TEXT("value"), Reason);
					Reason.TrimStartAndEndInline();
				}
			}
		}
	}

	// Prevent duplicate tickets: one active ticket per Discord user at a time.
	if (OpenerToTicketChannel.Contains(DiscordUserId))
	{
		const FString ExistingChanId = OpenerToTicketChannel[DiscordUserId];
		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		FString::Printf(
		TEXT(":warning: You already have an open ticket (<#%s>). "
		     "Please continue there, or close it before opening a new one."),
		*ExistingChanId),
		/*bEphemeral=*/true);
		return;
	}

	// Dispatch to the correct ticket type based on the modal's custom_id.
	if (ModalCustomId == TEXT("ticket_modal:wl"))
	{
		if (!Config.bTicketWhitelistEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Whitelist request tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		TEXT(":white_check_mark: Opening your whitelist request ticket… "
		     "A private channel will appear shortly."),
		/*bEphemeral=*/true);

		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("whitelist"), Reason);
	}
	else if (ModalCustomId == TEXT("ticket_modal:help"))
	{
		if (!Config.bTicketHelpEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Help tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		TEXT(":white_check_mark: Opening your help ticket… "
		     "A private channel will appear shortly."),
		/*bEphemeral=*/true);

		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("help"), Reason);
	}
	else if (ModalCustomId == TEXT("ticket_modal:report"))
	{
		if (!Config.bTicketReportEnabled)
		{
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: Player report tickets are currently disabled."),
			/*bEphemeral=*/true);
			return;
		}

		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		TEXT(":white_check_mark: Opening your report ticket… "
		     "A private channel will appear shortly."),
		/*bEphemeral=*/true);

		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("report"), Reason);
	}
	else if (ModalCustomId.StartsWith(TEXT("ticket_modal:cr_")))
	{
		const int32 ReasonIndex =
		FCString::Atoi(*ModalCustomId.Mid(FCString::Strlen(TEXT("ticket_modal:cr_"))));

		if (!Config.CustomTicketReasons.IsValidIndex(ReasonIndex))
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: Modal submit – custom ticket reason index %d out of range."),
			       ReasonIndex);
			RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: This ticket type is no longer available. "
			     "Please contact an admin directly."),
			/*bEphemeral=*/true);
			return;
		}

		FString Label, Desc;
		Config.CustomTicketReasons[ReasonIndex].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			Label = TEXT("Custom");
		}

		const FString TypeSlug = SanitizeUsernameForChannel(Label);

		RespondToInteraction(InteractionId, InteractionToken,
		/*type=*/4,
		FString::Printf(
		TEXT(":white_check_mark: Opening your **%s** ticket… "
		     "A private channel will appear shortly."),
		*Label),
		/*bEphemeral=*/true);

		CreateTicketChannel(DiscordUserId, DiscordUsername, TypeSlug, Reason, Label, Desc);
	}
	else
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: Unrecognised ticket modal custom_id: %s"), *ModalCustomId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Create a private ticket channel in the guild
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::CreateTicketChannel(
const FString& OpenerUserId,
const FString& OpenerUsername,
const FString& TicketType,
const FString& ExtraInfo,
const FString& DisplayLabel,
const FString& DisplayDesc)
{
	if (Config.BotToken.IsEmpty() || GuildId.IsEmpty())
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: CreateTicketChannel – BotToken or GuildId is empty; cannot create channel."));
		return;
	}

	// Build a sanitized channel name: ticket-{type}-{username}
	const FString SafeName    = SanitizeUsernameForChannel(OpenerUsername);
	const FString ChannelName = FString::Printf(TEXT("ticket-%s-%s"), *TicketType, *SafeName);

	// ── Permission overwrites ────────────────────────────────────────────────
	// Discord permission bits used below:
	//   VIEW_CHANNEL         = 1024   (0x0000000000000400)
	//   SEND_MESSAGES        = 2048   (0x0000000000000800)
	//   READ_MESSAGE_HISTORY = 65536  (0x0000000000010000)
	// Combined allow value: 68608

	TArray<TSharedPtr<FJsonValue>> Overwrites;

	// 1. Deny VIEW_CHANNEL for @everyone (role ID == guild ID in Discord).
	{
		TSharedPtr<FJsonObject> EveryoneDeny = MakeShared<FJsonObject>();
		EveryoneDeny->SetStringField(TEXT("id"),    GuildId); // @everyone role has the guild's ID
		EveryoneDeny->SetNumberField(TEXT("type"),  0);       // 0 = role
		EveryoneDeny->SetStringField(TEXT("allow"), TEXT("0"));
		EveryoneDeny->SetStringField(TEXT("deny"),  TEXT("1024")); // VIEW_CHANNEL
		Overwrites.Add(MakeShared<FJsonValueObject>(EveryoneDeny));
	}

	// 2. Allow VIEW_CHANNEL + SEND_MESSAGES + READ_MESSAGE_HISTORY for the
	//    admin/support role (TicketNotifyRoleId), when configured.
	if (!Config.TicketNotifyRoleId.IsEmpty())
	{
		TSharedPtr<FJsonObject> AdminAllow = MakeShared<FJsonObject>();
		AdminAllow->SetStringField(TEXT("id"),    Config.TicketNotifyRoleId);
		AdminAllow->SetNumberField(TEXT("type"),  0);        // 0 = role
		AdminAllow->SetStringField(TEXT("allow"), TEXT("68608")); // VIEW | SEND | HISTORY
		AdminAllow->SetStringField(TEXT("deny"),  TEXT("0"));
		Overwrites.Add(MakeShared<FJsonValueObject>(AdminAllow));
	}

	// 3. Allow the same permissions for the ticket opener (by Discord user ID).
	if (!OpenerUserId.IsEmpty())
	{
		TSharedPtr<FJsonObject> UserAllow = MakeShared<FJsonObject>();
		UserAllow->SetStringField(TEXT("id"),    OpenerUserId);
		UserAllow->SetNumberField(TEXT("type"),  1);         // 1 = member
		UserAllow->SetStringField(TEXT("allow"), TEXT("68608")); // VIEW | SEND | HISTORY
		UserAllow->SetStringField(TEXT("deny"),  TEXT("0"));
		Overwrites.Add(MakeShared<FJsonValueObject>(UserAllow));
	}

	// ── Build the channel creation request body ───────────────────────────────
	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("name"), ChannelName);
	Body->SetNumberField(TEXT("type"), 0);             // 0 = GUILD_TEXT
	Body->SetArrayField(TEXT("permission_overwrites"), Overwrites);

	// Place the channel inside the configured category (when set).
	if (!Config.TicketCategoryId.IsEmpty())
	{
		Body->SetStringField(TEXT("parent_id"), Config.TicketCategoryId);
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

	// Capture by value everything the callback needs – the subsystem is
	// referenced via a WeakObjectPtr so we don't extend its lifetime.
	const FString BotTokenCopy        = Config.BotToken;
	const FString NotifyRoleIdCopy    = Config.TicketNotifyRoleId;
	const FString OpenerUserIdCopy    = OpenerUserId;
	const FString OpenerUsernameCopy  = OpenerUsername;
	const FString TicketTypeCopy      = TicketType;
	const FString ExtraInfoCopy       = ExtraInfo;
	const FString DisplayLabelCopy    = DisplayLabel;
	const FString DisplayDescCopy     = DisplayDesc;

	// Determine the admin/ticket channel ID to post the notification in.
	FString AdminChannelIdCopy;
	{
		const TArray<FString> TicketChannels = Config.TicketChannelId.IsEmpty()
			? FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId)
			: FDiscordBridgeConfig::ParseChannelIds(Config.TicketChannelId);
		if (TicketChannels.Num() > 0)
		{
			AdminChannelIdCopy = TicketChannels[0];
		}
	}

	Request->OnProcessRequestComplete().BindWeakLambda(
	this,
	[this, BotTokenCopy, AdminChannelIdCopy, NotifyRoleIdCopy,
	 OpenerUserIdCopy, OpenerUsernameCopy, TicketTypeCopy, ExtraInfoCopy,
	 DisplayLabelCopy, DisplayDescCopy]
	(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
	{
		if (!bConnected || !Resp.IsValid())
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: CreateTicketChannel HTTP request failed."));
			return;
		}
		if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 201)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: CreateTicketChannel returned HTTP %d: %s"),
			       Resp->GetResponseCode(), *Resp->GetContentAsString());
			return;
		}

		// Parse the response to get the new channel's ID.
		TSharedPtr<FJsonObject> ChannelObj;
		TSharedRef<TJsonReader<>> Reader =
		TJsonReaderFactory<>::Create(Resp->GetContentAsString());
		if (!FJsonSerializer::Deserialize(Reader, ChannelObj) || !ChannelObj.IsValid())
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: CreateTicketChannel – failed to parse channel response."));
			return;
		}

		FString NewChannelId;
		ChannelObj->TryGetStringField(TEXT("id"), NewChannelId);
		if (NewChannelId.IsEmpty())
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: CreateTicketChannel – response missing channel id."));
			return;
		}

		UE_LOG(LogDiscordBridge, Log,
		       TEXT("DiscordBridge: Created ticket channel %s for user '%s' (%s)."),
		       *NewChannelId, *OpenerUsernameCopy, *OpenerUserIdCopy);

		// Track the new ticket channel.
		TicketChannelToOpener.Add(NewChannelId, OpenerUserIdCopy);
		OpenerToTicketChannel.Add(OpenerUserIdCopy, NewChannelId);

		// ── Build the welcome message for the ticket channel ──────────────
		FString WelcomeContent;
		const FString MentionPrefix = NotifyRoleIdCopy.IsEmpty()
		? TEXT("")
		: FString::Printf(TEXT("<@&%s> "), *NotifyRoleIdCopy);
		const FString UserMention = OpenerUserIdCopy.IsEmpty()
		? FString::Printf(TEXT("**%s**"), *OpenerUsernameCopy)
		: FString::Printf(TEXT("<@%s>"), *OpenerUserIdCopy);

		if (TicketTypeCopy == TEXT("whitelist"))
		{
			WelcomeContent = FString::Printf(
			TEXT("%s:ticket: **Whitelist Request** from %s\n\n"
			     "Please tell us your **in-game name** so we can add you to the whitelist.\n"
			     "If you have a reason or any additional information, feel free to share it here.\n\n"
			     ":information_source: An admin will review your request and add you to the "
			     "whitelist using `!whitelist add <in-game-name>`."),
			*MentionPrefix, *UserMention);
		}
		else if (TicketTypeCopy == TEXT("help"))
		{
			WelcomeContent = FString::Printf(
			TEXT("%s:ticket: **Help / Support Request** from %s\n\n"
			     "Please describe your issue in as much detail as possible so we can help you quickly.\n\n"
			     ":information_source: An admin will respond here shortly."),
			*MentionPrefix, *UserMention);
		}
		else if (TicketTypeCopy == TEXT("report"))
		{
			WelcomeContent = FString::Printf(
			TEXT("%s:ticket: **Player Report** from %s\n\n"
			     "Please provide:\n"
			     "- The **in-game name** of the player you are reporting\n"
			     "- A **description** of the issue or incident\n"
			     "- Any **screenshots** or evidence if available\n\n"
			     ":information_source: An admin will review the report here."),
			*MentionPrefix, *UserMention);
		}
		else
		{
			// Custom ticket type: use DisplayLabel and DisplayDesc from the config entry.
			const FString LabelDisplay = DisplayLabelCopy.IsEmpty() ? TicketTypeCopy : DisplayLabelCopy;
			WelcomeContent = FString::Printf(
			TEXT("%s:ticket: **%s** from %s\n\n"),
			*MentionPrefix, *LabelDisplay, *UserMention);
			if (!DisplayDescCopy.IsEmpty())
			{
				WelcomeContent += DisplayDescCopy + TEXT("\n\n");
			}
			WelcomeContent += TEXT(":information_source: An admin will be with you shortly. "
			                        "Please describe your request here.");
		}

		// If the opener supplied a reason via the pre-ticket modal, include it
		// prominently in the welcome message so admins see it immediately.
		if (!ExtraInfoCopy.IsEmpty())
		{
			WelcomeContent += FString::Printf(TEXT("\n\n**Details provided:** %s"), *ExtraInfoCopy);
		}

		// Post the welcome message (plain text, no components).
		SendMessageBodyToChannelImpl(BotTokenCopy, NewChannelId,
		[&WelcomeContent]() -> TSharedPtr<FJsonObject>
		{
			TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
			Msg->SetStringField(TEXT("content"), WelcomeContent);
			return Msg;
		}());

		// Post the close-ticket button in the same channel so both the
		// opener and admins can close it without any commands.
		SendMessageBodyToChannelImpl(BotTokenCopy, NewChannelId,
		MakeCloseButtonMessage(OpenerUserIdCopy));

		// Also notify the admin/ticket channel (when configured and different
		// from the new ticket channel itself).
		if (!AdminChannelIdCopy.IsEmpty() && AdminChannelIdCopy != NewChannelId)
		{
			// For custom ticket types use the human-readable DisplayLabel in the notification.
			const FString NoticeTypeName = (!DisplayLabelCopy.IsEmpty())
			    ? DisplayLabelCopy
			    : TicketTypeCopy;
			const FString AdminNotice = FString::Printf(
			TEXT("%s:new: New **%s** ticket opened by %s: <#%s>"),
			NotifyRoleIdCopy.IsEmpty()
			? TEXT("")
			: *FString::Printf(TEXT("<@&%s> "), *NotifyRoleIdCopy),
			*NoticeTypeName, *UserMention, *NewChannelId);

			SendMessageBodyToChannelImpl(BotTokenCopy, AdminChannelIdCopy,
			[&AdminNotice]() -> TSharedPtr<FJsonObject>
			{
				TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
				Msg->SetStringField(TEXT("content"), AdminNotice);
				return Msg;
			}());
		}
	});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Delete a Discord channel (used to close ticket channels)
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::DeleteDiscordChannel(const FString& ChannelId)
{
	if (Config.BotToken.IsEmpty() || ChannelId.IsEmpty())
	{
		return;
	}

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Deleting ticket channel %s."), *ChannelId);

	// Remove from tracking maps immediately so a second close attempt does not
	// issue a redundant DELETE request.
	FString OpenerUserId;
	if (TicketChannelToOpener.RemoveAndCopyValue(ChannelId, OpenerUserId))
	{
		OpenerToTicketChannel.Remove(OpenerUserId);
	}

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
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: DeleteDiscordChannel HTTP request failed for channel %s."),
			       *ChannelId);
			return;
		}
		// 200 OK is returned with the deleted channel object on success.
		if (Resp->GetResponseCode() < 200 || Resp->GetResponseCode() >= 300)
		{
			UE_LOG(LogDiscordBridge, Warning,
			       TEXT("DiscordBridge: DeleteDiscordChannel returned HTTP %d for channel %s: %s"),
			       Resp->GetResponseCode(), *ChannelId, *Resp->GetContentAsString());
		}
		else
		{
			UE_LOG(LogDiscordBridge, Log,
			       TEXT("DiscordBridge: Ticket channel %s deleted successfully."), *ChannelId);
		}
	});

	Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
// Post the ticket panel (message with clickable buttons) to a Discord channel
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::PostTicketPanel(
const FString& PanelChannelId,
const FString& ResponseChannelId)
{
	if (Config.BotToken.IsEmpty() || PanelChannelId.IsEmpty())
	{
		if (!ResponseChannelId.IsEmpty())
		{
			SendMessageToChannel(ResponseChannelId,
			TEXT(":warning: Cannot post the ticket panel: bot token or panel channel ID is not configured."));
		}
		return;
	}

	// Build the panel content based on which ticket types are enabled.
	FString PanelContent =
	TEXT(":ticket: **Support Tickets**\n"
	     "Click one of the buttons below to open a support ticket. "
	     "A private channel will be created that only you and the support team can see.\n\n");

	if (Config.bTicketWhitelistEnabled)
	{
		PanelContent += TEXT(":clipboard: **Whitelist Request** – request to be added to the server whitelist\n");
	}
	if (Config.bTicketHelpEnabled)
	{
		PanelContent += TEXT(":question: **Help / Support** – ask for help or report a problem\n");
	}
	if (Config.bTicketReportEnabled)
	{
		PanelContent += TEXT(":warning: **Report a Player** – report another player to the admins\n");
	}

	// Append descriptions for each custom reason.
	for (int32 i = 0; i < Config.CustomTicketReasons.Num(); ++i)
	{
		FString Label, Desc;
		Config.CustomTicketReasons[i].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			continue;
		}
		PanelContent += FString::Printf(TEXT(":small_blue_diamond: **%s**"), *Label);
		if (!Desc.IsEmpty())
		{
			PanelContent += TEXT(" – ") + Desc;
		}
		PanelContent += TEXT("\n");
	}

	// Build all buttons: built-in types first, then custom reasons.
	TArray<TSharedPtr<FJsonValue>> AllButtons;

	if (Config.bTicketWhitelistEnabled)
	{
		TSharedPtr<FJsonObject> Btn = MakeShared<FJsonObject>();
		Btn->SetNumberField(TEXT("type"),      2); // BUTTON
		Btn->SetNumberField(TEXT("style"),     1); // PRIMARY (blurple)
		Btn->SetStringField(TEXT("label"),     TEXT("Whitelist Request"));
		Btn->SetStringField(TEXT("custom_id"), TEXT("ticket_wl"));
		AllButtons.Add(MakeShared<FJsonValueObject>(Btn));
	}

	if (Config.bTicketHelpEnabled)
	{
		TSharedPtr<FJsonObject> Btn = MakeShared<FJsonObject>();
		Btn->SetNumberField(TEXT("type"),      2); // BUTTON
		Btn->SetNumberField(TEXT("style"),     3); // SUCCESS (green)
		Btn->SetStringField(TEXT("label"),     TEXT("Help / Support"));
		Btn->SetStringField(TEXT("custom_id"), TEXT("ticket_help"));
		AllButtons.Add(MakeShared<FJsonValueObject>(Btn));
	}

	if (Config.bTicketReportEnabled)
	{
		TSharedPtr<FJsonObject> Btn = MakeShared<FJsonObject>();
		Btn->SetNumberField(TEXT("type"),      2); // BUTTON
		Btn->SetNumberField(TEXT("style"),     4); // DANGER (red)
		Btn->SetStringField(TEXT("label"),     TEXT("Report a Player"));
		Btn->SetStringField(TEXT("custom_id"), TEXT("ticket_report"));
		AllButtons.Add(MakeShared<FJsonValueObject>(Btn));
	}

	// Add custom reason buttons (style 2 = SECONDARY/grey to distinguish from built-ins).
	// Discord supports at most 25 buttons per message (5 rows x 5 buttons).
	for (int32 i = 0; i < Config.CustomTicketReasons.Num() && AllButtons.Num() < 25; ++i)
	{
		FString Label, Desc;
		Config.CustomTicketReasons[i].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			continue;
		}
		// Clamp label to 80 chars (Discord button label limit is 80 chars).
		if (Label.Len() > 80)
		{
			Label = Label.Left(80);
		}
		TSharedPtr<FJsonObject> Btn = MakeShared<FJsonObject>();
		Btn->SetNumberField(TEXT("type"),      2); // BUTTON
		Btn->SetNumberField(TEXT("style"),     2); // SECONDARY (grey)
		Btn->SetStringField(TEXT("label"),     Label);
		Btn->SetStringField(TEXT("custom_id"), FString::Printf(TEXT("ticket_cr_%d"), i));
		AllButtons.Add(MakeShared<FJsonValueObject>(Btn));
	}

	if (AllButtons.Num() == 0)
	{
		if (!ResponseChannelId.IsEmpty())
		{
			SendMessageToChannel(ResponseChannelId,
			TEXT(":warning: No ticket types are enabled. "
			     "Enable at least one via TicketWhitelistEnabled, TicketHelpEnabled, "
			     "or TicketReportEnabled in DefaultTickets.ini, or add a TicketReason= entry."));
		}
		return;
	}

	// Discord allows at most 5 action rows per message, 5 buttons per row (= 25 max).
	// Split all buttons into action rows of at most 5 buttons each.
	TArray<TSharedPtr<FJsonValue>> ActionRows;
	for (int32 Start = 0; Start < AllButtons.Num(); Start += 5)
	{
		TArray<TSharedPtr<FJsonValue>> RowButtons;
		const int32 End = FMath::Min(Start + 5, AllButtons.Num());
		for (int32 j = Start; j < End; ++j)
		{
			RowButtons.Add(AllButtons[j]);
		}
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("type"), 1); // ACTION_ROW
		Row->SetArrayField(TEXT("components"), RowButtons);
		ActionRows.Add(MakeShared<FJsonValueObject>(Row));
	}

	TSharedPtr<FJsonObject> MessageBody = MakeShared<FJsonObject>();
	MessageBody->SetStringField(TEXT("content"), PanelContent.TrimEnd());
	MessageBody->SetArrayField(TEXT("components"), ActionRows);

	SendMessageBodyToChannelImpl(Config.BotToken, PanelChannelId, MessageBody);

	if (!ResponseChannelId.IsEmpty() && ResponseChannelId != PanelChannelId)
	{
		SendMessageToChannel(ResponseChannelId,
		FString::Printf(TEXT(":white_check_mark: Ticket panel posted to <#%s>. "
		                     "Members can now click the buttons to open tickets."),
		                *PanelChannelId));
	}

	UE_LOG(LogDiscordBridge, Log,
	       TEXT("DiscordBridge: Ticket panel posted to channel %s."), *PanelChannelId);
}

// ─────────────────────────────────────────────────────────────────────────────
// GameplayEvents integration
// ─────────────────────────────────────────────────────────────────────────────

void UDiscordBridgeSubsystem::DispatchDiscordGameplayEvent(const FGameplayTag& Tag) const
{
	if (!Tag.IsValid())
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	if (UGameplayEventsSubsystem* GES = GI ? GI->GetSubsystem<UGameplayEventsSubsystem>() : nullptr)
	{
		GES->DispatchGameplayEvent(FGameplayEvent(Tag, {}));
	}
}

void UDiscordBridgeSubsystem::DispatchDiscordGameplayEvent(const FGameplayTag& Tag,
                                                            const FString& Payload) const
{
	if (!Tag.IsValid())
	{
		return;
	}
	UGameInstance* GI = GetGameInstance();
	if (UGameplayEventsSubsystem* GES = GI ? GI->GetSubsystem<UGameplayEventsSubsystem>() : nullptr)
	{
		GES->DispatchGameplayEvent(FGameplayEvent(Tag, {}, Payload));
	}
}

void UDiscordBridgeSubsystem::OnToDiscordGameplayEvent(const FGameplayEvent& Event)
{
	// Only handle the exact ToDiscord tag; ignore all other tags routed through
	// the same delegate (UGameplayEventsSubsystem broadcasts all events to all
	// subscribers, so each subscriber must filter by tag).
	if (Event.EventTag != FDiscordGameplayTags::ToDiscord())
	{
		return;
	}

	FString MessageText;
	if (!UGameplayEventsBlueprintLibrary::GetGameplayEventStringPayload(Event, MessageText)
	    || MessageText.IsEmpty())
	{
		return;
	}

	if (!bGatewayReady)
	{
		UE_LOG(LogDiscordBridge, Warning,
		       TEXT("DiscordBridge: GameplayEvent DiscordBridge.Message.ToDiscord received but "
		            "Gateway is not ready – message dropped: '%s'"),
		       *MessageText);
		return;
	}

	for (const FString& ChanId : FDiscordBridgeConfig::ParseChannelIds(Config.ChannelId))
	{
		SendMessageToChannel(ChanId, MessageText);
	}
}
