// Copyright Coffee Stain Studios. All Rights Reserved.

#include "SMLWebSocketServer.h"
#include "SMLWebSocketServerRunnable.h"
#include "HAL/RunnableThread.h"

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
    return NewObject<USMLWebSocketServer>(WorldContextObject);
}

bool USMLWebSocketServer::Listen(int32 Port)
{
    if (bListening) return true;

    ServerRunnable = MakeShared<FSMLWebSocketServerRunnable>(this, Port);
    if (!ServerRunnable->Init())
    {
        ServerRunnable.Reset();
        return false;
    }

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
    }
    OnClientDisconnected.Broadcast(ClientId, Reason);
}

void USMLWebSocketServer::Internal_OnClientMessage(const FString& ClientId, const FString& Message)
{
    OnClientMessage.Broadcast(ClientId, Message);
}

void USMLWebSocketServer::Internal_OnError(const FString& ErrorMessage)
{
    UE_LOG(LogSMLWebSocketServer, Error, TEXT("WSServer error: %s"), *ErrorMessage);
    OnError.Broadcast(ErrorMessage);
}
