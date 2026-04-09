// Copyright Yamahasxviper. All Rights Reserved.

#include "BanWebSocketPusher.h"
#include "BanSystemConfig.h"
#include "SMLWebSocketClient.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogBanWebSocketPusher);

TWeakObjectPtr<UBanWebSocketPusher> UBanWebSocketPusher::ActiveInstance;

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanWebSocketPusher::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || !Cfg->bPushEventsToWebSocket || Cfg->WebSocketPushUrl.IsEmpty())
    {
        UE_LOG(LogBanWebSocketPusher, Log,
            TEXT("BanWebSocketPusher: WebSocket push disabled (bPushEventsToWebSocket=false or WebSocketPushUrl empty)."));
        return;
    }

    Client = NewObject<USMLWebSocketClient>(this);
    Client->bAutoReconnect = true;
    Client->ReconnectInitialDelaySeconds = 5.0f;
    Client->MaxReconnectDelaySeconds     = 60.0f;
    Client->Connect(Cfg->WebSocketPushUrl);

    ActiveInstance = this;

    UE_LOG(LogBanWebSocketPusher, Log,
        TEXT("BanWebSocketPusher: connecting to '%s'."), *Cfg->WebSocketPushUrl);
}

void UBanWebSocketPusher::Deinitialize()
{
    if (Client)
    {
        Client->Close();
        Client = nullptr;
    }

    if (ActiveInstance == this)
        ActiveInstance.Reset();

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Static push helper
// ─────────────────────────────────────────────────────────────────────────────

void UBanWebSocketPusher::PushEvent(const FString& EventType,
                                     const TSharedPtr<FJsonObject>& Fields)
{
    UBanWebSocketPusher* Self = ActiveInstance.Get();
    if (!Self || !Self->Client)
        return;

    // Build the envelope.
    TSharedPtr<FJsonObject> Envelope = MakeShared<FJsonObject>();
    Envelope->SetStringField(TEXT("event"),     EventType);
    Envelope->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());

    // Merge caller-supplied fields into the envelope.
    if (Fields.IsValid())
    {
        for (const auto& Pair : Fields->Values)
            Envelope->SetField(Pair.Key, Pair.Value);
    }

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Envelope.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanWebSocketPusher, Warning,
            TEXT("BanWebSocketPusher: failed to serialize event '%s'"), *EventType);
        return;
    }

    Self->Client->SendJson(JsonStr);
}
