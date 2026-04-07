// Copyright Coffee Stain Studios. All Rights Reserved.

#include "SMLWebSocketClient.h"
#include "SMLWebSocket.h"
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
	bHasConnectedOnce = false;
	ConnectionState.store(static_cast<uint8>(EWebSocketState::Connecting));

	// Build the reconnect configuration from our current property values.
	FSMLWebSocketReconnectConfig ReconnectCfg;
	ReconnectCfg.bAutoReconnect            = bAutoReconnect;
	ReconnectCfg.ReconnectInitialDelay     = ReconnectInitialDelaySeconds;
	ReconnectCfg.MaxReconnectDelay         = MaxReconnectDelaySeconds;
	ReconnectCfg.MaxReconnectAttempts      = MaxReconnectAttempts;

	Runnable = MakeShared<FSMLWebSocketRunnable>(this, Url, Protocols, ExtraHeaders, ReconnectCfg,
	                                             ConnectionGeneration);
	RunnableThread = FRunnableThread::Create(Runnable.Get(),
	                                         TEXT("SMLWebSocketThread"),
	                                         0,
	                                         TPri_Normal);
}

// ── Sending ───────────────────────────────────────────────────────────────────

void USMLWebSocketClient::SendText(const FString& Message)
{
	if (bIsConnected && Runnable.IsValid())
	{
		Runnable->EnqueueText(Message);
		return;
	}
	if (bQueueMessagesWhileDisconnected)
	{
		FScopeLock Lock(&QueueMutex);
		if (PendingSendQueue.Num() < 100)
		{
			PendingSendQueue.Add(Message);
		}
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
		ConnectionState.store(static_cast<uint8>(EWebSocketState::Closing));
		Runnable->EnqueueClose(Code, Reason);
	}
}

bool USMLWebSocketClient::IsConnected() const
{
	return bIsConnected;
}

EWebSocketState USMLWebSocketClient::GetConnectionState() const
{
	return static_cast<EWebSocketState>(ConnectionState.load());
}

// ── Private helpers ───────────────────────────────────────────────────────────

void USMLWebSocketClient::StopRunnable()
{
	// Invalidate any game-thread callbacks that were queued by the old
	// connection before we tear it down.  The Notify* lambdas inside
	// FSMLWebSocketRunnable capture this generation value and silently
	// discard their work when it no longer matches.
	++ConnectionGeneration;

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
	ConnectionState.store(static_cast<uint8>(EWebSocketState::Disconnected));
}

// ── Internal callbacks (called on the game thread) ────────────────────────────

void USMLWebSocketClient::Internal_OnConnected()
{
	bIsConnected = true;
	ConnectionState.store(static_cast<uint8>(EWebSocketState::Connected));

	// Flush messages that were queued while the connection was down.
	{
		FScopeLock Lock(&QueueMutex);
		for (const FString& Msg : PendingSendQueue)
		{
			if (Runnable.IsValid())
			{
				Runnable->EnqueueText(Msg);
			}
		}
		PendingSendQueue.Empty();
	}

	if (bHasConnectedOnce)
	{
		OnReconnected.Broadcast();
	}
	bHasConnectedOnce = true;

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
	ConnectionState.store(static_cast<uint8>(
		bAutoReconnect ? EWebSocketState::Connecting : EWebSocketState::Disconnected));
	OnClosed.Broadcast(StatusCode, Reason);
}

void USMLWebSocketClient::Internal_OnError(const FString& ErrorMessage)
{
	bIsConnected = false;
	ConnectionState.store(static_cast<uint8>(
		bAutoReconnect ? EWebSocketState::Connecting : EWebSocketState::Disconnected));
	OnError.Broadcast(ErrorMessage);
}

void USMLWebSocketClient::Internal_OnReconnecting(int32 AttemptNumber, float DelaySeconds)
{
	bIsConnected = false;
	OnReconnecting.Broadcast(AttemptNumber, DelaySeconds);
}
