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
    // Consume-once: if OnPeerMessage registered this UID before calling AddBan(),
    // suppress re-broadcasting and remove the guard entry.  Using a UID set means
    // the guard survives even when OnBanAdded fires asynchronously after AddBan()
    // returns — a plain bool would have been cleared too early in that case.
    if (PeerAppliedBanUids.Remove(Entry.Uid) > 0) return;

    // Skip IP bans (Platform=="IP") from sync by default to avoid interfering
    // with peer-specific CIDR bans.
    if (Entry.Platform == TEXT("IP")) return;

    // Compute remaining lifetime in minutes as a floating-point value so we
    // can distinguish "truly expired" (≤ 0.0) from "< 1 minute remaining".
    const double RemainingMinutesDbl = Entry.bIsPermanent
        ? 0.0
        : (Entry.ExpireDate - FDateTime::UtcNow()).GetTotalMinutes();

    // Do not sync a temporary ban that has already expired — the peer would
    // have no meaningful duration to enforce.
    if (!Entry.bIsPermanent && RemainingMinutesDbl <= 0.0)
        return;

    // Round UP fractional minutes so a ban with < 1 minute remaining maps to
    // 1 min rather than 0.  The previous static_cast<int32> (floor) caused
    // 1-minute bans to truncate to 0, which both triggered the early-return
    // guard above (ban silently never synced) and would have been read by the
    // receiver as a permanent ban had the guard not existed.  Ceiling also
    // prevents any ban from losing up to 59 seconds of enforced duration on
    // the receiving peer.
    const int32 DurationMinutes = Entry.bIsPermanent
        ? 0
        : FMath::Max(1, FMath::CeilToInt(RemainingMinutesDbl));

    BroadcastBan(Entry.Uid, Entry.PlayerName, Entry.Reason, Entry.BannedBy,
                 DurationMinutes, Entry.Category);
}

void UBanSyncClient::OnLocalBanRemoved(const FString& Uid, const FString& PlayerName)
{
    // Consume-once: suppress re-broadcast for peer-sourced removals (both the
    // unban path and the stale-record removal that precedes a peer-sourced update).
    if (PeerAppliedUnbanUids.Remove(Uid) > 0) return;

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
            // Register the UID in PeerAppliedUnbanUids so OnLocalBanRemoved does
            // not re-broadcast this peer-sourced removal back to peers.
            // Pass bSilent=true so BanDiscordSubsystem's BanRemovedHandle does NOT
            // post a spurious "✅ unbanned" message; the subsequent AddBan will
            // post the real update notification instead.
            PeerAppliedUnbanUids.Add(Uid);
            DB->RemoveBanByUid(Uid, /*bSilent=*/true);
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

        // Register the UID in PeerAppliedBanUids before calling AddBan() so that
        // OnLocalBanAdded suppresses re-broadcasting this peer-sourced ban.
        // Consume-once semantics: OnLocalBanAdded removes the entry when it fires,
        // so the guard holds whether OnBanAdded fires synchronously or asynchronously.
        PeerAppliedBanUids.Add(Uid);
        const bool bAdded = DB->AddBan(Ban);
        if (!bAdded)
        {
            // AddBan failed and will not fire OnBanAdded — clean up the pre-added entry
            // so it does not silently suppress a future legitimate local ban for this UID.
            PeerAppliedBanUids.Remove(Uid);
        }

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

        // Register the UID in PeerAppliedUnbanUids before calling RemoveBanByUid()
        // so OnLocalBanRemoved suppresses re-broadcasting this peer-sourced unban.
        PeerAppliedUnbanUids.Add(Uid);
        const bool bRemoved = DB->RemoveBanByUid(Uid);
        if (!bRemoved)
        {
            // Ban was not found — RemoveBanByUid will not fire OnBanRemoved,
            // so clean up the pre-added entry to avoid a spurious future suppression.
            PeerAppliedUnbanUids.Remove(Uid);
        }

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
