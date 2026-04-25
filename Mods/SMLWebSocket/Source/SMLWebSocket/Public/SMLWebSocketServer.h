// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "HAL/CriticalSection.h"
#include "UObject/Object.h"
#include "SMLWebSocketServer.generated.h"

class FSMLWebSocketServerRunnable;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketServerOnClientConnectedDelegate,
                                              const FString&, ClientId,
                                              const FString&, RemoteAddress);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketServerOnClientDisconnectedDelegate,
                                              const FString&, ClientId,
                                              const FString&, Reason);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketServerOnClientMessageDelegate,
                                              const FString&, ClientId,
                                              const FString&, Message);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FSMLWebSocketServerOnClientBinaryMessageDelegate,
                                              const FString&, ClientId,
                                              const TArray<uint8>&, Data);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FSMLWebSocketServerOnErrorDelegate,
                                             const FString&, ErrorMessage);

/**
 * USMLWebSocketServer
 *
 * Listens on a local TCP port and accepts incoming WebSocket connections.
 * Uses the same raw TCP + RFC 6455 handshake approach as the client.
 * Allows external tools and dashboards to connect to the game server without
 * the game server needing to initiate the outbound connection.
 *
 * Usage:
 *   1. Create an instance via CreateWebSocketServer().
 *   2. Bind to OnClientConnected, OnClientMessage, OnClientDisconnected.
 *   3. Call Listen(Port) to start accepting connections.
 *   4. Use BroadcastText() to send a message to all connected clients,
 *      or SendTextToClient() to target a specific client.
 *   5. Call StopListening() to shut down the server.
 */
UCLASS(BlueprintType, Blueprintable, Category="SML|WebSocket|Server")
class SMLWEBSOCKET_API USMLWebSocketServer : public UObject
{
    GENERATED_BODY()

public:
    USMLWebSocketServer();
    virtual ~USMLWebSocketServer() override;
    virtual void BeginDestroy() override;

    // ── Delegates ─────────────────────────────────────────────────────────────

    /** Fired when a new WebSocket client connects. ClientId is a unique string for this session. */
    UPROPERTY(BlueprintAssignable, Category="SML|WebSocket|Server")
    FSMLWebSocketServerOnClientConnectedDelegate OnClientConnected;

    /** Fired when a client disconnects. */
    UPROPERTY(BlueprintAssignable, Category="SML|WebSocket|Server")
    FSMLWebSocketServerOnClientDisconnectedDelegate OnClientDisconnected;

    /** Fired when a text message is received from a connected client. */
    UPROPERTY(BlueprintAssignable, Category="SML|WebSocket|Server")
    FSMLWebSocketServerOnClientMessageDelegate OnClientMessage;

    /** Fired when a binary message is received from a connected client. */
    UPROPERTY(BlueprintAssignable, Category="SML|WebSocket|Server")
    FSMLWebSocketServerOnClientBinaryMessageDelegate OnClientBinaryMessage;

    /** Fired when a server-level error occurs (e.g. bind failure). */
    UPROPERTY(BlueprintAssignable, Category="SML|WebSocket|Server")
    FSMLWebSocketServerOnErrorDelegate OnError;

    // ── Factory ───────────────────────────────────────────────────────────────

    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server", meta=(WorldContext="WorldContextObject"))
    static USMLWebSocketServer* CreateWebSocketServer(UObject* WorldContextObject);

    // ── Lifecycle ─────────────────────────────────────────────────────────────

    /**
     * Start listening for WebSocket connections on the given port.
     * Binds to 0.0.0.0 (all interfaces).
     * @param Port  TCP port to listen on (e.g. 9001).
     * @return true if the server successfully bound to the port.
     */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    bool Listen(int32 Port);

    /**
     * Optional API token for server-side authentication.
     * When non-empty, incoming WebSocket clients must include the HTTP header:
     *   Authorization: Bearer <token>
     * in their HTTP Upgrade request to be accepted. Clients that do not present
     * the correct token receive an HTTP 401 response and the connection is closed.
     * Leave empty (default) to accept all connections without authentication.
     * Set this before calling Listen().
     */
    UPROPERTY(BlueprintReadWrite, Category="SML|WebSocket|Server")
    FString ApiToken;

    /** Stop accepting new connections and close all active connections. */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    void StopListening();

    /** Returns true when the server is currently listening. */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Server")
    bool IsListening() const;

    // ── Messaging ─────────────────────────────────────────────────────────────

    /** Send a text message to all currently-connected clients. */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    void BroadcastText(const FString& Message);

    /**
     * Send a text message to a specific client by its ClientId.
     * ClientId is the string provided in OnClientConnected.
     * Does nothing if the ClientId is not connected.
     */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    void SendTextToClient(const FString& ClientId, const FString& Message);

    /**
     * Send a typed event message to all clients that have subscribed to the given
     * event type.  Clients that have not sent a subscription filter receive all
     * events (backward-compatible behaviour: no filter = receive everything).
     *
     * The subscription wire protocol is:
     *   Client → server:  {"op":"subscribe","events":["ban","chat","session"]}
     *   Client → server:  {"op":"unsubscribe","events":["chat"]}
     *   Client → server:  {"op":"subscribe_all"}   (opt back in to all events)
     *
     * @param EventType  The event category string (e.g. "ban", "chat", "session").
     * @param Message    The full JSON string to deliver.
     */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    void BroadcastEventText(const FString& EventType, const FString& Message);

    /** Disconnect a specific client. */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Server")
    void DisconnectClient(const FString& ClientId);

    /** Returns the number of currently-connected clients. */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Server")
    int32 GetConnectedClientCount() const;

    /** Returns the ClientIds of all currently-connected clients. */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Server")
    TArray<FString> GetConnectedClientIds() const;

private:
    friend class FSMLWebSocketServerRunnable;

    // Called from server runnable (on the game thread via AsyncTask).
    void Internal_OnClientConnected(const FString& ClientId, const FString& RemoteAddress);
    void Internal_OnClientDisconnected(const FString& ClientId, const FString& Reason);
    void Internal_OnClientMessage(const FString& ClientId, const FString& Message);
    void Internal_OnClientBinaryMessage(const FString& ClientId, const TArray<uint8>& Data);
    void Internal_OnError(const FString& ErrorMessage);

    TSharedPtr<FSMLWebSocketServerRunnable> ServerRunnable;
    FRunnableThread* ServerThread{nullptr};

    FThreadSafeBool bListening{false};

    mutable FCriticalSection ClientMutex;
    TSet<FString> ConnectedClientIds;

    /**
     * Per-client event subscription sets.
     * If a client's entry is absent or the set is empty, the client receives
     * all events (unfiltered / backward-compatible).
     * Protected by ClientMutex.
     */
    TMap<FString, TSet<FString>> ClientSubscriptions;
};
