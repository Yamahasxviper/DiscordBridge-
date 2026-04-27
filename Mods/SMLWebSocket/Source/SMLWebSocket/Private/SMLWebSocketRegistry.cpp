// Copyright Yamahasxviper. All Rights Reserved.

#include "SMLWebSocketRegistry.h"
#include "SMLWebSocketClient.h"

void USMLWebSocketRegistry::RegisterClient(const FString& Name, USMLWebSocketClient* Client)
{
    if (Name.IsEmpty() || !Client) return;
    FScopeLock Lock(&ClientsMutex);
    Clients.Add(Name, Client);
}

USMLWebSocketClient* USMLWebSocketRegistry::GetClient(const FString& Name)
{
    FScopeLock Lock(&ClientsMutex);
    USMLWebSocketClient** Found = Clients.Find(Name);
    return Found ? *Found : nullptr;
}

void USMLWebSocketRegistry::UnregisterClient(const FString& Name)
{
    FScopeLock Lock(&ClientsMutex);
    Clients.Remove(Name);
}

bool USMLWebSocketRegistry::HasClient(const FString& Name) const
{
    FScopeLock Lock(&ClientsMutex);
    return Clients.Contains(Name);
}

TArray<FString> USMLWebSocketRegistry::GetRegisteredNames() const
{
    FScopeLock Lock(&ClientsMutex);
    TArray<FString> Names;
    Clients.GetKeys(Names);
    return Names;
}
