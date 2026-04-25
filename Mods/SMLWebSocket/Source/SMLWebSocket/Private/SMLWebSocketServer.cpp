// Copyright Yamahasxviper. All Rights Reserved.

#include "SMLWebSocketServer.h"
#include "SMLWebSocketServerRunnable.h"
#include "HAL/RunnableThread.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY_STATIC(LogSMLWebSocketServer, Log, All);

USMLWebSocketServer::USMLWebSocketServer()
{}

USMLWebSocketServer::~USMLWebSocketServer()
{
    StopListening();
}

void USMLWebSocketServer::BeginDestroy()
{
    StopListening();
    Super::BeginDestroy();
}

USMLWebSocketServer* USMLWebSocketServer::CreateWebSocketServer(UObject* WorldContextObject)
{
    UObject* Outer = WorldContextObject ? WorldContextObject : GetTransientPackage();
    return NewObject<USMLWebSocketServer>(Outer);
}

bool USMLWebSocketServer::Listen(int32 Port)
{
    if (bListening) return true;

    ServerRunnable = MakeShared<FSMLWebSocketServerRunnable>(this, Port, ApiToken);

    // Do NOT call Init() manually here. FRunnableThread::Create() calls Init() on
    // the new thread. Calling it a second time from the game thread would create and
    // bind the listen socket twice — leaking the first socket and potentially failing
    // to bind the port on the second attempt.
    ServerThread = FRunnableThread::Create(
        ServerRunnable.Get(),
        TEXT("SMLWebSocketServer"),
        0, TPri_BelowNormal);

    if (!ServerThread)
    {
        ServerRunnable.Reset();
        return false;
    }

    bListening = true;
    return true;
}

void USMLWebSocketServer::StopListening()
{
    if (!bListening) return;
    bListening = false;

    if (ServerRunnable.IsValid())
        ServerRunnable->Stop();

    if (ServerThread)
    {
        ServerThread->WaitForCompletion();
        delete ServerThread;
        ServerThread = nullptr;
    }

    ServerRunnable.Reset();
}

bool USMLWebSocketServer::IsListening() const
{
    return bListening;
}

void USMLWebSocketServer::BroadcastText(const FString& Message)
{
    if (ServerRunnable.IsValid())
        ServerRunnable->BroadcastText(Message);
}

void USMLWebSocketServer::SendTextToClient(const FString& ClientId, const FString& Message)
{
    if (ServerRunnable.IsValid())
        ServerRunnable->SendTextToClient(ClientId, Message);
}

void USMLWebSocketServer::DisconnectClient(const FString& ClientId)
{
    if (ServerRunnable.IsValid())
        ServerRunnable->DisconnectClient(ClientId);
}

int32 USMLWebSocketServer::GetConnectedClientCount() const
{
    return ServerRunnable.IsValid() ? ServerRunnable->GetClientCount() : 0;
}

TArray<FString> USMLWebSocketServer::GetConnectedClientIds() const
{
    if (ServerRunnable.IsValid()) return ServerRunnable->GetClientIds();
    return {};
}

// ── Internal callbacks ────────────────────────────────────────────────────────

void USMLWebSocketServer::BroadcastEventText(const FString& EventType, const FString& Message)
{
    if (!ServerRunnable.IsValid()) return;

    // Subscription sets store event types lowercased (see Internal_OnClientMessage).
    // Normalize EventType to lowercase before the lookup so callers don't need to.
    const FString NormEventType = EventType.ToLower();

    // Collect IDs of clients that should receive this event.
    // A client receives the event when:
    //   a) it has no subscription filter (receives everything), OR
    //   b) its subscription set explicitly contains EventType.
    TArray<FString> Targets;
    {
        FScopeLock L(&ClientMutex);
        for (const FString& Id : ConnectedClientIds)
        {
            const TSet<FString>* Subs = ClientSubscriptions.Find(Id);
            if (!Subs || Subs->Num() == 0 || Subs->Contains(NormEventType))
                Targets.Add(Id);
        }
    }

    for (const FString& Id : Targets)
        ServerRunnable->SendTextToClient(Id, Message);
}

void USMLWebSocketServer::Internal_OnClientConnected(const FString& ClientId, const FString& RemoteAddress)
{
    {
        FScopeLock L(&ClientMutex);
        ConnectedClientIds.Add(ClientId);
    }
    OnClientConnected.Broadcast(ClientId, RemoteAddress);
}

void USMLWebSocketServer::Internal_OnClientDisconnected(const FString& ClientId, const FString& Reason)
{
    {
        FScopeLock L(&ClientMutex);
        ConnectedClientIds.Remove(ClientId);
        ClientSubscriptions.Remove(ClientId); // clean up subscription map
    }
    OnClientDisconnected.Broadcast(ClientId, Reason);
}

void USMLWebSocketServer::Internal_OnClientMessage(const FString& ClientId, const FString& Message)
{
    // Check for a subscription-control message before forwarding to the delegate.
    // Format: {"op":"subscribe","events":["ban","chat"]}
    //         {"op":"unsubscribe","events":["chat"]}
    //         {"op":"subscribe_all"}
    bool bIsControlMessage = false;

    TSharedPtr<FJsonObject> Json;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Message);
    if (FJsonSerializer::Deserialize(Reader, Json) && Json.IsValid())
    {
        FString Op;
        if (Json->TryGetStringField(TEXT("op"), Op))
        {
            if (Op == TEXT("subscribe") || Op == TEXT("unsubscribe"))
            {
                const TArray<TSharedPtr<FJsonValue>>* EventsArr = nullptr;
                Json->TryGetArrayField(TEXT("events"), EventsArr);

                FScopeLock L(&ClientMutex);
                TSet<FString>& Subs = ClientSubscriptions.FindOrAdd(ClientId);

                if (EventsArr)
                {
                    for (const TSharedPtr<FJsonValue>& V : *EventsArr)
                    {
                        FString Evt;
                        if (V.IsValid() && V->TryGetString(Evt) && !Evt.IsEmpty())
                        {
                            if (Op == TEXT("subscribe"))
                                Subs.Add(Evt.ToLower());
                            else
                                Subs.Remove(Evt.ToLower());
                        }
                    }
                }
                bIsControlMessage = true;
            }
            else if (Op == TEXT("subscribe_all"))
            {
                FScopeLock L(&ClientMutex);
                ClientSubscriptions.Remove(ClientId); // empty = receive all
                bIsControlMessage = true;
            }
        }
    }

    if (!bIsControlMessage)
        OnClientMessage.Broadcast(ClientId, Message);
}

void USMLWebSocketServer::Internal_OnClientBinaryMessage(const FString& ClientId, const TArray<uint8>& Data)
{
    OnClientBinaryMessage.Broadcast(ClientId, Data);
}

void USMLWebSocketServer::Internal_OnError(const FString& ErrorMessage)
{
    UE_LOG(LogSMLWebSocketServer, Error, TEXT("WSServer error: %s"), *ErrorMessage);
    OnError.Broadcast(ErrorMessage);
}
