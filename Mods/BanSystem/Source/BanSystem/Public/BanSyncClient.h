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
     * Consume-once set of UIDs whose OnBanAdded delegate should be suppressed.
     * OnPeerMessage adds a UID here before calling DB->AddBan(); OnLocalBanAdded
     * removes the UID on its first invocation and returns without re-broadcasting.
     *
     * Using a UID set instead of a plain bool means the guard persists until the
     * delegate actually fires — it is immune to the race where AddBan() dispatches
     * OnBanAdded asynchronously (e.g. queued to the game thread) after returning,
     * which would have reset a bool guard too early and allowed an infinite loop.
     *
     * Only accessed on the game thread.
     */
    TSet<FString> PeerAppliedBanUids;

    /**
     * Consume-once set of UIDs whose OnBanRemoved delegate should be suppressed.
     * Used by both the unban path and the stale-record removal that precedes a
     * peer-sourced update.  Mirrors the semantics of PeerAppliedBanUids.
     *
     * Only accessed on the game thread.
     */
    TSet<FString> PeerAppliedUnbanUids;
};
