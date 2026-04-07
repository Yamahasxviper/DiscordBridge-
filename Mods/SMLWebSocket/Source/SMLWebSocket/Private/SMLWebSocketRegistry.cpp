// Copyright Coffee Stain Studios. All Rights Reserved.

#include "SMLWebSocketRegistry.h"
#include "SMLWebSocketClient.h"

void USMLWebSocketRegistry::RegisterClient(const FString& Name, USMLWebSocketClient* Client)
{
    if (Name.IsEmpty()) return;
    Clients.Add(Name, Client);
}

USMLWebSocketClient* USMLWebSocketRegistry::GetClient(const FString& Name) const
{
    const USMLWebSocketClient* const* Found = Clients.Find(Name);
    return Found ? const_cast<USMLWebSocketClient*>(*Found) : nullptr;
}

void USMLWebSocketRegistry::UnregisterClient(const FString& Name)
{
    Clients.Remove(Name);
}

bool USMLWebSocketRegistry::HasClient(const FString& Name) const
{
    return Clients.Contains(Name);
}

TArray<FString> USMLWebSocketRegistry::GetRegisteredNames() const
{
    TArray<FString> Names;
    Clients.GetKeys(Names);
    return Names;
}
