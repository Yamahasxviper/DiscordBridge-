// Copyright Coffee Stain Studios. All Rights Reserved.

#include "SMLWebSocketClient.h"
#include "SMLWebSocketRunnable.h"
#include "HAL/RunnableThread.h"
#include "Async/Async.h"

// ─────────────────────────────────────────────────────────────────────────────
// USMLWebSocketClient
// ─────────────────────────────────────────────────────────────────────────────

USMLWebSocketClient::USMLWebSocketClient()
	: bIsConnected(false)
{
}

USMLWebSocketClient::~USMLWebSocketClient()
{
	StopRunnable();
}

void USMLWebSocketClient::BeginDestroy()
{
	StopRunnable();
	Super::BeginDestroy();
}

// ── Factory ───────────────────────────────────────────────────────────────────

USMLWebSocketClient* USMLWebSocketClient::CreateWebSocketClient(UObject* WorldContextObject)
{
	return NewObject<USMLWebSocketClient>(WorldContextObject ? WorldContextObject : GetTransientPackage());
}

// ── Connection ────────────────────────────────────────────────────────────────

void USMLWebSocketClient::Connect(const FString& Url,
                                  const TArray<FString>& Protocols,
                                  const TMap<FString, FString>& ExtraHeaders)
{
	// Stop any existing connection first.
	StopRunnable();

	bIsConnected = false;

	// Build the reconnect configuration from our current property values.
	FSMLWebSocketReconnectConfig ReconnectCfg;
	ReconnectCfg.bAutoReconnect            = bAutoReconnect;
	ReconnectCfg.ReconnectInitialDelay     = ReconnectInitialDelaySeconds;
	ReconnectCfg.MaxReconnectDelay         = MaxReconnectDelaySeconds;
	ReconnectCfg.MaxReconnectAttempts      = MaxReconnectAttempts;

	Runnable = MakeShared<FSMLWebSocketRunnable>(this, Url, Protocols, ExtraHeaders, ReconnectCfg);
	RunnableThread = FRunnableThread::Create(Runnable.Get(),
	                                         TEXT("SMLWebSocketThread"),
	                                         0,
	                                         TPri_Normal);
}

// ── Sending ───────────────────────────────────────────────────────────────────

void USMLWebSocketClient::SendText(const FString& Message)
{
	if (Runnable.IsValid())
	{
		Runnable->EnqueueText(Message);
	}
}

void USMLWebSocketClient::SendBinary(const TArray<uint8>& Data)
{
	if (Runnable.IsValid())
	{
		Runnable->EnqueueBinary(Data);
	}
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void USMLWebSocketClient::Close(int32 Code, const FString& Reason)
{
	if (Runnable.IsValid())
	{
		Runnable->EnqueueClose(Code, Reason);
	}
}

bool USMLWebSocketClient::IsConnected() const
{
	return bIsConnected;
}

// ── Private helpers ───────────────────────────────────────────────────────────

void USMLWebSocketClient::StopRunnable()
{
	if (Runnable.IsValid())
	{
		Runnable->Stop();
	}
	if (RunnableThread)
	{
		RunnableThread->Kill(true /*bShouldWait*/);
		delete RunnableThread;
		RunnableThread = nullptr;
	}
	Runnable.Reset();
	bIsConnected = false;
}

// ── Internal callbacks (called on the game thread) ────────────────────────────

void USMLWebSocketClient::Internal_OnConnected()
{
	bIsConnected = true;
	OnConnected.Broadcast();
}

void USMLWebSocketClient::Internal_OnMessage(const FString& Message)
{
	OnMessage.Broadcast(Message);
}

void USMLWebSocketClient::Internal_OnBinaryMessage(const TArray<uint8>& Data, bool bIsFinal)
{
	OnBinaryMessage.Broadcast(Data, bIsFinal);
}

void USMLWebSocketClient::Internal_OnClosed(int32 StatusCode, const FString& Reason)
{
	bIsConnected = false;
	OnClosed.Broadcast(StatusCode, Reason);
}

void USMLWebSocketClient::Internal_OnError(const FString& ErrorMessage)
{
	bIsConnected = false;
	OnError.Broadcast(ErrorMessage);
}

void USMLWebSocketClient::Internal_OnReconnecting(int32 AttemptNumber, float DelaySeconds)
{
	bIsConnected = false;
	OnReconnecting.Broadcast(AttemptNumber, DelaySeconds);
}
