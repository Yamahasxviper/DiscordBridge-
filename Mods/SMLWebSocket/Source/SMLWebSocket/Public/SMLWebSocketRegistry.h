// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "SMLWebSocketRegistry.generated.h"

class USMLWebSocketClient;

/**
 * USMLWebSocketRegistry
 *
 * GameInstance-level subsystem that lets multiple mods share named WebSocket
 * client connections.  Instead of every mod managing its own lifecycle and
 * risking duplicate connections to the same host, mods register a named client
 * once and look it up by name thereafter.
 *
 * Example usage:
 *   USMLWebSocketRegistry* Reg = GI->GetSubsystem<USMLWebSocketRegistry>();
 *   Reg->RegisterClient(TEXT("DiscordBridge"), MyClient);
 *   ...
 *   USMLWebSocketClient* Client = Reg->GetClient(TEXT("DiscordBridge"));
 *   if (Client) Client->SendText(TEXT("hello"));
 */
UCLASS()
class SMLWEBSOCKET_API USMLWebSocketRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    /**
     * Register a named WebSocket client.
     * If a client is already registered under Name it is replaced.
     * The registry holds a strong reference; the client will not be
     * garbage-collected while registered.
     */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Registry")
    void RegisterClient(const FString& Name, USMLWebSocketClient* Client);

    /**
     * Retrieve the client registered under Name, or nullptr if none exists.
     */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Registry")
    USMLWebSocketClient* GetClient(const FString& Name) const;

    /**
     * Remove the client registered under Name from the registry.
     * Does NOT close the connection; the caller owns the client.
     */
    UFUNCTION(BlueprintCallable, Category="SML|WebSocket|Registry")
    void UnregisterClient(const FString& Name);

    /**
     * Returns true when a client is registered under the given name.
     */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Registry")
    bool HasClient(const FString& Name) const;

    /**
     * Returns all currently registered client names.
     */
    UFUNCTION(BlueprintPure, Category="SML|WebSocket|Registry")
    TArray<FString> GetRegisteredNames() const;

private:
    UPROPERTY()
    TMap<FString, USMLWebSocketClient*> Clients;
};
