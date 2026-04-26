// Copyright Yamahasxviper. All Rights Reserved.

#include "BanSyncClient.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanSystemConfig.h"
#include "BanAuditLog.h"
#include "SMLWebSocketClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogBanSyncClient);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanSyncClient::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Super::Initialize(Collection);

    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || Cfg->PeerWebSocketUrls.IsEmpty())
        return;

    for (const FString& Url : Cfg->PeerWebSocketUrls)
    {
        if (Url.IsEmpty()) continue;

        USMLWebSocketClient* Client = NewObject<USMLWebSocketClient>(this);
        if (!Client) continue;

        // Subscribe to incoming messages.
        Client->OnMessage.AddDynamic(this, &UBanSyncClient::OnPeerMessage);

        // Auto-reconnect with default parameters.
        Client->bAutoReconnect = true;
        Client->ReconnectInitialDelaySeconds = 5.0f;
        Client->MaxReconnectDelaySeconds     = 60.0f;
        Client->Connect(Url, TArray<FString>(), TMap<FString, FString>());

        PeerClients.Add(Client);

        UE_LOG(LogBanSyncClient, Log,
            TEXT("BanSyncClient: connecting to peer %s"), *Url);
    }

    // Subscribe to local ban/unban events so we can forward them to peers.
    UBanDatabase::OnBanAdded.AddUObject(this, &UBanSyncClient::OnLocalBanAdded);
    UBanDatabase::OnBanRemoved.AddUObject(this, &UBanSyncClient::OnLocalBanRemoved);
}

void UBanSyncClient::Deinitialize()
{
    UBanDatabase::OnBanAdded.RemoveAll(this);
    UBanDatabase::OnBanRemoved.RemoveAll(this);

    for (USMLWebSocketClient* Client : PeerClients)
    {
        if (Client)
            Client->Close();
    }
    PeerClients.Empty();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Outbound broadcast
// ─────────────────────────────────────────────────────────────────────────────

void UBanSyncClient::BroadcastBan(const FString& Uid, const FString& PlayerName,
                                   const FString& Reason, const FString& BannedBy,
                                   int32 DurationMinutes, const FString& Category)
{
    TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
    Msg->SetStringField(TEXT("type"),            TEXT("ban"));
    Msg->SetStringField(TEXT("uid"),             Uid);
    Msg->SetStringField(TEXT("playerName"),      PlayerName);
    Msg->SetStringField(TEXT("reason"),          Reason);
    Msg->SetStringField(TEXT("bannedBy"),        BannedBy);
    Msg->SetNumberField(TEXT("durationMinutes"), static_cast<double>(DurationMinutes));
    Msg->SetStringField(TEXT("category"),        Category);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Msg.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanSyncClient, Warning,
            TEXT("BanSyncClient: failed to serialize ban broadcast for %s"), *Uid);
        return;
    }

    for (USMLWebSocketClient* Client : PeerClients)
    {
        if (Client && Client->IsConnected())
            Client->SendText(JsonStr);
    }
}

void UBanSyncClient::BroadcastUnban(const FString& Uid, const FString& PlayerName)
{
    TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
    Msg->SetStringField(TEXT("type"),       TEXT("unban"));
    Msg->SetStringField(TEXT("uid"),        Uid);
    Msg->SetStringField(TEXT("playerName"), PlayerName);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Msg.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanSyncClient, Warning,
            TEXT("BanSyncClient: failed to serialize unban broadcast for %s"), *Uid);
        return;
    }

    for (USMLWebSocketClient* Client : PeerClients)
    {
        if (Client && Client->IsConnected())
            Client->SendText(JsonStr);
    }
}

void UBanSyncClient::OnLocalBanAdded(const FBanEntry& Entry)
{
    // Do not re-broadcast a ban that was just applied from a peer message.
    // Without this guard, OnPeerMessage → DB->AddBan() → OnBanAdded →
    // OnLocalBanAdded → BroadcastBan() would create an infinite loop.
    if (bProcessingPeerBan) return;

    // Skip IP bans (Platform=="IP") from sync by default to avoid interfering
    // with peer-specific CIDR bans.
    if (Entry.Platform == TEXT("IP")) return;

    const int32 DurationMinutes = Entry.bIsPermanent
        ? 0
        : FMath::CeilToInt((Entry.ExpireDate - FDateTime::UtcNow()).GetTotalMinutes());

    // Do not sync a temporary ban that has already expired — a duration of 0 or
    // less would be misread by the receiving peer as a permanent ban.
    if (!Entry.bIsPermanent && DurationMinutes <= 0)
        return;

    BroadcastBan(Entry.Uid, Entry.PlayerName, Entry.Reason, Entry.BannedBy,
                 FMath::Max(0, DurationMinutes), Entry.Category);
}

void UBanSyncClient::OnLocalBanRemoved(const FString& Uid, const FString& PlayerName)
{
    // Do not re-broadcast an unban that was just applied from a peer message.
    // Without this guard, OnPeerMessage → DB->RemoveBanByUid() → OnBanRemoved →
    // OnLocalBanRemoved → BroadcastUnban() → peer receives it → DB->RemoveBanByUid() → …
    // creates an infinite loop between the two servers whenever any unban fires.
    if (bProcessingPeerBan) return;

    BroadcastUnban(Uid, PlayerName);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Inbound message handler
// ─────────────────────────────────────────────────────────────────────────────

void UBanSyncClient::OnPeerMessage(const FString& Message)
{
    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid()) return;

    FString Type;
    if (!Root->TryGetStringField(TEXT("type"), Type)) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    if (Type == TEXT("ban"))
    {
        FString Uid, PlayerName, Reason, BannedBy, Category;
        Root->TryGetStringField(TEXT("uid"),        Uid);
        Root->TryGetStringField(TEXT("playerName"), PlayerName);
        Root->TryGetStringField(TEXT("reason"),     Reason);
        Root->TryGetStringField(TEXT("bannedBy"),   BannedBy);
        Root->TryGetStringField(TEXT("category"),   Category);

        double DurDbl = 0.0;
        Root->TryGetNumberField(TEXT("durationMinutes"), DurDbl);
        const int32 DurationMinutes = (DurDbl > 0.0 && DurDbl <= static_cast<double>(INT32_MAX))
            ? static_cast<int32>(DurDbl)
            : 0;

        if (Uid.IsEmpty()) return;

        // If the player is already banned, check whether key fields differ.
        // If they match exactly, skip (no change). If they differ (reason, duration,
        // category updated on the origin server), remove the stale entry so the
        // updated ban can be applied below.
        FBanEntry Existing;
        if (DB->IsCurrentlyBanned(Uid, Existing))
        {
            const FString IncomingReason   = Reason.IsEmpty() ? TEXT("Synced ban from peer server") : Reason;
            const FString IncomingBannedBy = BannedBy.IsEmpty() ? TEXT("peer") : BannedBy;
            const bool bPermanentMatch     = (Existing.bIsPermanent == (DurationMinutes <= 0));
            const bool bReasonMatch        = (Existing.Reason == IncomingReason);
            const bool bCategoryMatch      = (Existing.Category == Category);
            // Also compare expiry so that duration-only updates are not silently dropped.
            const FDateTime IncomingExpiry = (DurationMinutes <= 0)
                ? FDateTime(0)
                : FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);
            // Allow a 60-second tolerance to absorb transmission latency.
            const bool bExpiryMatch = Existing.bIsPermanent
                ? true
                : FMath::Abs((Existing.ExpireDate - IncomingExpiry).GetTotalSeconds()) < 60.0;
            if (bPermanentMatch && bReasonMatch && bCategoryMatch && bExpiryMatch)
                return; // Identical — nothing to update.
            // Fields changed — remove the stale record and fall through to re-add.
            // Guard against re-broadcasting the removal back to peers (B3 fix).
            bProcessingPeerBan = true;
            DB->RemoveBanByUid(Uid);
            bProcessingPeerBan = false;
        }

        FBanEntry Ban;
        Ban.Uid        = Uid;
        UBanDatabase::ParseUid(Uid, Ban.Platform, Ban.PlayerUID);
        Ban.PlayerName      = PlayerName;
        Ban.Reason          = Reason.IsEmpty() ? TEXT("Synced ban from peer server") : Reason;
        Ban.BannedBy        = BannedBy.IsEmpty() ? TEXT("peer") : BannedBy;
        const FDateTime Now = FDateTime::UtcNow();
        Ban.BanDate         = Now;
        Ban.Category        = Category;
        Ban.bIsPermanent    = (DurationMinutes <= 0);
        Ban.ExpireDate      = Ban.bIsPermanent
            ? FDateTime(0)
            : Now + FTimespan::FromMinutes(DurationMinutes);

        // Set the re-entrancy guard so that AddBan()'s OnBanAdded broadcast
        // does not cause OnLocalBanAdded to re-forward this ban back to peers.
        bProcessingPeerBan = true;
        const bool bAdded = DB->AddBan(Ban);
        bProcessingPeerBan = false;

        if (bAdded)
        {
            if (UWorld* World = GI->GetWorld())
                UBanEnforcer::KickConnectedPlayer(World, Uid, Ban.GetKickMessage());

            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("ban"), Uid, PlayerName,
                    TEXT("peer"), TEXT("peer"),
                    FString::Printf(TEXT("Synced from peer. Reason: %s"), *Ban.Reason));

            UE_LOG(LogBanSyncClient, Log,
                TEXT("BanSyncClient: applied synced ban for %s"), *Uid);
        }
    }
    else if (Type == TEXT("unban"))
    {
        FString Uid, PlayerName;
        Root->TryGetStringField(TEXT("uid"),        Uid);
        Root->TryGetStringField(TEXT("playerName"), PlayerName);

        if (Uid.IsEmpty()) return;

        // Guard against re-broadcasting this peer-sourced unban back to peers.
        bProcessingPeerBan = true;
        const bool bRemoved = DB->RemoveBanByUid(Uid);
        bProcessingPeerBan = false;

        if (bRemoved)
        {
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("unban"), Uid, PlayerName,
                    TEXT("peer"), TEXT("peer"),
                    TEXT("Synced unban from peer server"));

            UE_LOG(LogBanSyncClient, Log,
                TEXT("BanSyncClient: applied synced unban for %s"), *Uid);
        }
    }
}
