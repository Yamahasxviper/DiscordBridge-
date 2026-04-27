// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "UObject/Object.h"
#include <atomic>
#include "SMLWebSocketClient.generated.h"

class FSMLWebSocketRunnable;

/**
 * Connection state of the WebSocket client.
 * Query via GetConnectionState() — safe to call from any thread.
 */
UENUM(BlueprintType)
enum class EWebSocketState : uint8
{
	/** Not connected and not attempting to connect. */
	Disconnected    UMETA(DisplayName="Disconnected"),
	/** TCP/TLS handshake and HTTP upgrade in progress. */
	Connecting      UMETA(DisplayName="Connecting"),
	/** WebSocket handshake succeeded; messages can be sent and received. */
	Connected       UMETA(DisplayName="Connected"),
	/** Close frame sent; waiting for the server's close frame. */
	Closing         UMETA(DisplayName="Closing"),
};

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSMLWebSocketOnConnectedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketOnMessageDelegate, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnBinaryMessageDelegate, const TArray<uint8>&, Data, bool, bIsFinal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnClosedDelegate, int32, StatusCode, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketOnErrorDelegate, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnReconnectingDelegate, int32, AttemptNumber, float, DelaySeconds);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSMLWebSocketOnReconnectedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnStateChangedDelegate, EWebSocketState, OldState, EWebSocketState, NewState);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketOnJsonMessageDelegate, const FString&, JsonString);

/**
 * Connection statistics returned by GetConnectionStats().
 */
USTRUCT(BlueprintType)
struct SMLWEBSOCKET_API FSMLWebSocketStats
{
	GENERATED_BODY()

	/** Total bytes sent since connection was established. */
	UPROPERTY(BlueprintReadOnly, Category="SML|WebSocket")
	int64 BytesSent = 0;

	/** Total bytes received since connection was established. */
	UPROPERTY(BlueprintReadOnly, Category="SML|WebSocket")
	int64 BytesReceived = 0;

	/** Number of text messages sent since connection was established. */
	UPROPERTY(BlueprintReadOnly, Category="SML|WebSocket")
	int32 MessagesSent = 0;

	/** Number of text messages received since connection was established. */
	UPROPERTY(BlueprintReadOnly, Category="SML|WebSocket")
	int32 MessagesReceived = 0;

	/** Seconds since the connection was established (0 when not connected). */
	UPROPERTY(BlueprintReadOnly, Category="SML|WebSocket")
	float ConnectedForSeconds = 0.0f;
};

/**
 * Custom WebSocket client with SSL/OpenSSL support and automatic reconnect.
 *
 * Implements the WebSocket protocol (RFC 6455) over TCP with optional TLS encryption.
 * Supports both ws:// (plain TCP) and wss:// (TLS via OpenSSL) connections.
 * When the remote server drops the connection the client will automatically wait
 * ReconnectInitialDelaySeconds, then retry (with exponential back-off capped at
 * MaxReconnectDelaySeconds). Call Close() to stop without reconnecting.
 *
 * Designed for use with Alpakit packages without requiring Unreal Engine's built-in
 * WebSocket module, which may not be present in Satisfactory's custom Unreal Engine build.
 *
 * All delegate callbacks are fired on the game thread.
 *
 * Usage (Blueprint):
 *   1. Call CreateWebSocketClient() to create an instance.
 *   2. Set bAutoReconnect / reconnect timing properties as desired.
 *   3. Bind your callbacks to OnConnected, OnMessage, OnClosed, OnError, OnReconnecting.
 *   4. Call Connect() with your ws:// or wss:// URL.
 *   5. Use SendText() / SendBinary() to exchange messages.
 *   6. Call Close() when done (prevents reconnect).
 */
UCLASS(BlueprintType, Blueprintable, Category="SML|WebSocket")
class SMLWEBSOCKET_API USMLWebSocketClient : public UObject
{
	GENERATED_BODY()

public:
	USMLWebSocketClient();
	virtual ~USMLWebSocketClient() override;
	virtual void BeginDestroy() override;

	// ── Delegates ────────────────────────────────────────────────────────────

	/** Called on the game thread when the WebSocket handshake succeeds and the connection is ready. */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnConnectedDelegate OnConnected;

	/** Called on the game thread when a UTF-8 text message is received. */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnMessageDelegate OnMessage;

	/** Called on the game thread when a binary message (or fragment) is received. */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnBinaryMessageDelegate OnBinaryMessage;

	/**
	 * Called on the game thread when the connection is closed.
	 * If bAutoReconnect is true and the close was not user-initiated, the client
	 * will attempt to reconnect; OnReconnecting will fire before each retry.
	 */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnClosedDelegate OnClosed;

	/** Called on the game thread when a connection or protocol error occurs. */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnErrorDelegate OnError;

	/**
	 * Called on the game thread just before a reconnect attempt begins.
	 * AttemptNumber starts at 1. DelaySeconds is the time the client will
	 * sleep before making the next connection attempt.
	 */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnReconnectingDelegate OnReconnecting;

	/**
	 * Called on the game thread when a reconnect attempt succeeds (distinct from
	 * OnConnected, which fires on both the initial connection and reconnects).
	 * Use this to re-send authentication payloads (e.g. Discord IDENTIFY) after
	 * a reconnect without sending them on the very first connection.
	 */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnReconnectedDelegate OnReconnected;

	/**
	 * Called on the game thread whenever the connection state transitions.
	 * Fires for every state change: Disconnected ↔ Connecting ↔ Connected ↔ Closing.
	 * This is a convenience alternative to binding all four individual delegates
	 * (OnConnected, OnClosed, OnError, OnReconnecting) when you just want to react
	 * to any state change in one place.
	 */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnStateChangedDelegate OnStateChanged;

	/**
	 * Called on the game thread when a received text frame parses as valid JSON.
	 * Fires in addition to OnMessage.  The JsonString parameter is the raw JSON
	 * string (same as the OnMessage payload); callers should deserialise it with
	 * FJsonSerializer.  Messages that fail JSON parsing are silently skipped.
	 */
	UPROPERTY(BlueprintAssignable, Category="SML|WebSocket")
	FSMLWebSocketOnJsonMessageDelegate OnJsonMessage;

	// ── Reconnect configuration ───────────────────────────────────────────────

	/**
	 * When true the client will automatically reconnect after any non-user-initiated
	 * disconnection (server crash, network drop, etc.).
	 * Calling Close() always prevents reconnect regardless of this setting.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	bool bAutoReconnect{true};

	/**
	 * Seconds to wait before the first reconnect attempt.
	 * Each subsequent attempt doubles this value up to MaxReconnectDelaySeconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0.1", UIMin="0.1"))
	float ReconnectInitialDelaySeconds{2.0f};

	/**
	 * Maximum seconds to wait between reconnect attempts after exponential back-off.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="1.0", UIMin="1.0"))
	float MaxReconnectDelaySeconds{30.0f};

	/**
	 * Maximum number of reconnect attempts. 0 = retry indefinitely.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0", UIMin="0"))
	int32 MaxReconnectAttempts{0};

	/**
	 * Maximum seconds to wait for the TCP/TLS connection to be established
	 * before aborting and firing OnError.  0 = no timeout (blocks indefinitely).
	 * Default: 10 seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0.0", UIMin="0.0"))
	float ConnectTimeoutSeconds{10.0f};

	/**
	 * When true, text and binary messages sent while disconnected are queued
	 * and flushed automatically when the connection (re)connects. Default: false.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	bool bQueueMessagesWhileDisconnected{false};

	/**
	 * Maximum number of text messages to keep in the disconnected-send queue.
	 * When the queue reaches this limit, the oldest message is dropped to make
	 * room for the new one (FIFO drop-from-front).
	 * Set to 0 for unlimited (default; original behaviour — can grow very large
	 * on long disconnects, so set a limit on high-traffic deployments).
	 * Default: 0.
	 * Only takes effect when bQueueMessagesWhileDisconnected is true.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0", UIMin="0"))
	int32 MaxQueuedMessages{0};

	// ── Ping / pong keep-alive ────────────────────────────────────────────────

	/**
	 * Interval in seconds between unsolicited Ping frames sent to the server.
	 * A Ping is sent after the connection has been idle (no frame received) for
	 * this many seconds.  Set to 0 to disable the keep-alive mechanism entirely.
	 * Default: 30 seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0.0", UIMin="0.0"))
	float PingIntervalSeconds{30.0f};

	/**
	 * Seconds to wait for a Pong after sending a Ping before treating the
	 * connection as stalled and triggering a reconnect.
	 * Only used when PingIntervalSeconds > 0.
	 * Default: 10 seconds.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="1.0", UIMin="1.0"))
	float PingTimeoutSeconds{10.0f};

	// ── Payload size guard ────────────────────────────────────────────────────

	/**
	 * Maximum allowed incoming WebSocket frame payload size in bytes.
	 * When an incoming frame's payload exceeds this value it is dropped,
	 * OnError is fired, and the connection is closed (triggering a reconnect
	 * if bAutoReconnect is true).
	 * Set to 0 to use the built-in hard ceiling (64 MB).
	 * Default: 0 (unlimited — uses internal 64 MB hard ceiling).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="0", UIMin="0"))
	int32 MaxMessageSizeBytes{0};

	// ── Proxy support ─────────────────────────────────────────────────────────

	/**
	 * Hostname or IP address of an HTTP/CONNECT proxy server.
	 * Leave empty to connect directly (default).
	 * When set, the client first connects to ProxyHost:ProxyPort and sends an
	 * HTTP CONNECT tunnel request to reach ParsedHost:ParsedPort, then performs
	 * the TLS handshake (for wss://) through the tunnel.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	FString ProxyHost;

	/**
	 * Port of the HTTP/CONNECT proxy server (default: 3128).
	 * Only used when ProxyHost is non-empty.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket", meta=(ClampMin="1", UIMin="1", ClampMax="65535"))
	int32 ProxyPort{3128};

	/**
	 * Optional username for HTTP CONNECT proxy authentication (Basic auth).
	 * Leave empty when the proxy does not require authentication.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	FString ProxyUser;

	/**
	 * Optional password for HTTP CONNECT proxy authentication (Basic auth).
	 * Leave empty when the proxy does not require authentication.
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	FString ProxyPassword;

	/**
	 * When true, the TLS certificate presented by the wss:// server is
	 * validated against the system CA bundle (SSL_VERIFY_PEER).
	 *
	 * NOTE: The Satisfactory dedicated-server environment does not ship a
	 * standard CA bundle, so this should remain false (the default) unless
	 * you have manually configured the server with a CA certificate store.
	 * Setting it to true with no CA bundle will cause every wss:// connection
	 * to fail with an SSL certificate-verification error.
	 *
	 * When false (default) the connection is still TLS-encrypted — only
	 * certificate-chain validation is skipped (SSL_VERIFY_NONE).
	 */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="SML|WebSocket")
	bool bVerifySSLCertificate{false};

	// ── Factory ───────────────────────────────────────────────────────────────

	/**
	 * Create a new WebSocket client instance.
	 *
	 * @param WorldContextObject  Any object that provides the outer for the new UObject.
	 * @return                    A new USMLWebSocketClient ready to use.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket", meta=(WorldContext="WorldContextObject"))
	static USMLWebSocketClient* CreateWebSocketClient(UObject* WorldContextObject);

	// ── Connection ────────────────────────────────────────────────────────────

	/**
	 * Connect to a WebSocket server.
	 *
	 * The URL must begin with ws:// (plain) or wss:// (TLS).
	 * Automatically performs the HTTP upgrade handshake.
	 * OnConnected is fired if the handshake succeeds; OnError is fired on failure.
	 * If bAutoReconnect is true and the connection is lost, the client retries
	 * automatically (with exponential back-off) until Close() is called or
	 * MaxReconnectAttempts is exhausted.
	 *
	 * @param Url          The WebSocket URL, e.g. "wss://example.com:443/chat".
	 * @param Protocols    Optional sub-protocol names to request (Sec-WebSocket-Protocol header).
	 * @param ExtraHeaders Additional HTTP headers to include in the upgrade request.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void Connect(const FString& Url, const TArray<FString>& Protocols, const TMap<FString, FString>& ExtraHeaders);

	// ── Sending ───────────────────────────────────────────────────────────────

	/**
	 * Send a UTF-8 text message to the server.
	 * The connection must be established (IsConnected() == true).
	 *
	 * @param Message  The text message to send.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void SendText(const FString& Message);

	/**
	 * Send raw binary data to the server.
	 * The connection must be established (IsConnected() == true).
	 *
	 * @param Data  The binary payload to send.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void SendBinary(const TArray<uint8>& Data);

	/**
	 * Send a binary message — move overload.
	 *
	 * Identical to SendBinary(const TArray<uint8>&) but moves the buffer rather
	 * than copying it.  Use this when the caller owns the buffer and no longer
	 * needs it after the call, to eliminate an unnecessary allocation/copy for
	 * high-frequency binary traffic.
	 *
	 * @param Data  The binary payload to send; ownership is transferred.
	 */
	void SendBinary(TArray<uint8>&& Data);

	// ── Lifecycle ─────────────────────────────────────────────────────────────

	/**
	 * Close the WebSocket connection gracefully and disable auto-reconnect.
	 *
	 * Sends a WebSocket Close frame (RFC 6455 §5.5.1).
	 * OnClosed is fired once the closing handshake completes.
	 * Auto-reconnect is suppressed for this call.
	 *
	 * @param Code    Close status code (1000 = normal closure, 1001 = going away, …).
	 * @param Reason  Human-readable reason string (≤123 bytes in UTF-8).
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void Close(int32 Code = 1000, const FString& Reason = TEXT(""));

	/**
	 * Returns true when the WebSocket handshake has completed and the
	 * connection can send/receive messages.
	 */
	UFUNCTION(BlueprintPure, Category="SML|WebSocket")
	bool IsConnected() const;

	/**
	 * Returns the current connection state.
	 * Safe to call from any thread (uses atomic reads internally).
	 */
	UFUNCTION(BlueprintPure, Category="SML|WebSocket")
	EWebSocketState GetConnectionState() const;

	// ── Queued message helpers ────────────────────────────────────────────────

	/**
	 * Returns the number of text messages currently queued for delivery
	 * (only non-zero when bQueueMessagesWhileDisconnected is true and the client
	 * is not currently connected).  Does not count queued binary messages.
	 * Thread-safe.
	 */
	UFUNCTION(BlueprintPure, Category="SML|WebSocket")
	int32 GetQueuedMessageCount() const;

	/**
	 * Discards all messages currently in the pending-send queue without sending them.
	 * Safe to call at any time; has no effect when the queue is empty.
	 * Thread-safe.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void ClearQueue();

	/**
	 * Immediately flushes all queued messages to the connected server.
	 * Has no effect when the client is not currently connected.
	 * Calls SendText() for each message in the queue; the queue is cleared
	 * after all messages have been dispatched.
	 * Thread-safe.
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void FlushQueue();

	// ── JSON helpers ──────────────────────────────────────────────────────────

	/**
	 * Convenience helper: serialises a JSON string and sends it as a UTF-8
	 * text frame.  Equivalent to calling SendText() with a pre-serialised
	 * JSON string.  Fires OnJsonMessage on the receiving end when the peer
	 * also uses SMLWebSocket.
	 *
	 * @param JsonString  A pre-serialised JSON string (e.g. from FJsonSerializer).
	 */
	UFUNCTION(BlueprintCallable, Category="SML|WebSocket")
	void SendJson(const FString& JsonString);

	// ── Diagnostics ───────────────────────────────────────────────────────────

	/**
	 * Returns a snapshot of connection statistics: bytes sent/received,
	 * messages sent/received, and seconds since last connect.
	 * All counters reset when a new connection is established.
	 */
	UFUNCTION(BlueprintPure, Category="SML|WebSocket")
	FSMLWebSocketStats GetConnectionStats() const;

private:
	friend class FSMLWebSocketRunnable;

	// Called from FSMLWebSocketRunnable (on the game thread via AsyncTask)
	void Internal_OnConnected();
	void Internal_OnMessage(const FString& Message);
	void Internal_OnBinaryMessage(const TArray<uint8>& Data, bool bIsFinal);
	void Internal_OnClosed(int32 StatusCode, const FString& Reason);
	void Internal_OnError(const FString& ErrorMessage);
	void Internal_OnReconnecting(int32 AttemptNumber, float DelaySeconds);

	/** Helper to fire OnStateChanged when the atomic ConnectionState changes. */
	void SetConnectionState(EWebSocketState NewState);

	void StopRunnable();

	TSharedPtr<FSMLWebSocketRunnable> Runnable;
	FRunnableThread* RunnableThread{nullptr};

	FThreadSafeBool bIsConnected{false};

	/** Tracks the fine-grained connection state; 0=Disconnected 1=Connecting 2=Connected 3=Closing. */
	std::atomic<uint8> ConnectionState{0u};

	/** True once the first successful connection has been made; used to distinguish reconnects. */
	bool bHasConnectedOnce{false};

	/**
	 * Outgoing messages queued while disconnected. Flushed on successful connect/reconnect.
	 * Thread-safe via QueueMutex.
	 */
	TArray<FString> PendingSendQueue;

	/**
	 * Outgoing binary payloads queued while disconnected (mirrors PendingSendQueue).
	 * Flushed alongside PendingSendQueue on successful connect/reconnect.
	 * Thread-safe via QueueMutex.
	 */
	TArray<TArray<uint8>> PendingSendBinaryQueue;

	mutable FCriticalSection QueueMutex;

	/**
	 * Monotonically-increasing counter that is incremented each time
	 * StopRunnable() is called.  Async game-thread callbacks captured by an
	 * FSMLWebSocketRunnable carry the generation value at their creation time
	 * and are silently dropped when it no longer matches, preventing stale
	 * events from a replaced connection from firing on the game thread.
	 */
	std::atomic<uint32> ConnectionGeneration{0};

	// ── Stats ─────────────────────────────────────────────────────────────────

	std::atomic<int64> StatBytesSent{0};
	std::atomic<int64> StatBytesReceived{0};
	std::atomic<int32> StatMessagesSent{0};
	std::atomic<int32> StatMessagesReceived{0};
	/** Absolute time (FPlatformTime::Seconds()) when the last connect was established. 0 = not connected. */
	double StatConnectTime{0.0};
};
