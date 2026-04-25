// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanTypes.h"
#include "BanSyncClient.generated.h"

class USMLWebSocketClient;

DECLARE_LOG_CATEGORY_EXTERN(LogBanSyncClient, Log, All);

/**
 * UBanSyncClient
 *
 * Multi-server ban synchronisation subsystem.
 *
 * When PeerWebSocketUrls is configured in DefaultBanSystem.ini, this subsystem
 * opens one SMLWebSocketClient per peer URL and listens for incoming ban/unban
 * events broadcast by remote servers.  Whenever UBanDatabase applies a local
 * ban or unban, the module calls BroadcastBan() / BroadcastUnban() to push the
 * event to all connected peers.
 *
 * Incoming event format (JSON):
 *   { "type": "ban",   "uid": "EOS:xxx", "playerName": "Alice", "reason": "...",
 *     "bannedBy": "admin", "durationMinutes": 0, "category": "" }
 *   { "type": "unban", "uid": "EOS:xxx" }
 *
 * Configuration (DefaultBanSystem.ini):
 *   +PeerWebSocketUrls=ws://server2.example.com:9000/events
 */
UCLASS()
class BANSYSTEM_API UBanSyncClient : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    // ── USubsystem ───────────────────────────────────────────────────────────
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /**
     * Broadcast a ban event to all peer servers.
     * Safe to call from any thread.
     */
    void BroadcastBan(const FString& Uid, const FString& PlayerName,
                      const FString& Reason, const FString& BannedBy,
                      int32 DurationMinutes, const FString& Category = TEXT(""));

    /**
     * Broadcast an unban event to all peer servers.
     * Safe to call from any thread.
     */
    void BroadcastUnban(const FString& Uid, const FString& PlayerName = TEXT(""));

private:
    /** Handles an incoming JSON message from a peer. */
    UFUNCTION()
    void OnPeerMessage(const FString& Message);

    /** Called by UBanDatabase::OnBanAdded to forward local bans to peers. */
    void OnLocalBanAdded(const FBanEntry& Entry);

    /** Called by UBanDatabase::OnBanRemoved to forward local unbans to peers. */
    void OnLocalBanRemoved(const FString& Uid, const FString& PlayerName);

    /** Active WebSocket client connections to each peer URL. */
    UPROPERTY()
    TArray<USMLWebSocketClient*> PeerClients;

    /**
     * Re-entrancy guard set while OnPeerMessage is applying a peer-received ban.
     * Prevents OnLocalBanAdded from re-broadcasting that ban back to the peer
     * that just sent it (and causing an infinite broadcast loop).
     * Only ever accessed on the game thread.
     */
    bool bProcessingPeerBan = false;
};
