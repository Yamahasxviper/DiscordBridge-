// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "SMLWebSocketClient.generated.h"

class FSMLWebSocketRunnable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FSMLWebSocketOnConnectedDelegate);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketOnMessageDelegate, const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnBinaryMessageDelegate, const TArray<uint8>&, Data, bool, bIsFinal);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnClosedDelegate, int32, StatusCode, const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketOnErrorDelegate, const FString&, ErrorMessage);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketOnReconnectingDelegate, int32, AttemptNumber, float, DelaySeconds);

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

private:
	friend class FSMLWebSocketRunnable;

	// Called from FSMLWebSocketRunnable (on the game thread via AsyncTask)
	void Internal_OnConnected();
	void Internal_OnMessage(const FString& Message);
	void Internal_OnBinaryMessage(const TArray<uint8>& Data, bool bIsFinal);
	void Internal_OnClosed(int32 StatusCode, const FString& Reason);
	void Internal_OnError(const FString& ErrorMessage);
	void Internal_OnReconnecting(int32 AttemptNumber, float DelaySeconds);

	void StopRunnable();

	TSharedPtr<FSMLWebSocketRunnable> Runnable;
	FRunnableThread* RunnableThread{nullptr};

	FThreadSafeBool bIsConnected{false};
};
