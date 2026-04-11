// Copyright Yamahasxviper. All Rights Reserved.

#include "SMLWebSocketServerRunnable.h"
#include "SMLWebSocketServer.h"

#include "Async/Async.h"
#include "Sockets.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"
#include "Misc/Base64.h"
#include "HAL/PlatformProcess.h"
#include "Misc/SecureHash.h"

DEFINE_LOG_CATEGORY_STATIC(LogWSServer, Log, All);

std::atomic<uint64> FSMLWebSocketServerRunnable::NextClientId{1};

// ─────────────────────────────────────────────────────────────────────────────
//  Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

FSMLWebSocketServerRunnable::FSMLWebSocketServerRunnable(USMLWebSocketServer* InOwner, int32 InPort,
                                                          const FString& InApiToken)
    : Owner(InOwner)
    , ListenPort(InPort)
    , ApiToken(InApiToken)
{}

FSMLWebSocketServerRunnable::~FSMLWebSocketServerRunnable()
{
    if (ListenSocket)
    {
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  FRunnable
// ─────────────────────────────────────────────────────────────────────────────

bool FSMLWebSocketServerRunnable::Init()
{
    ISocketSubsystem* SocketSub = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM);
    ListenSocket = SocketSub->CreateSocket(NAME_Stream, TEXT("SMLWSServer"), false);
    if (!ListenSocket)
    {
        UE_LOG(LogWSServer, Error, TEXT("WSServer: failed to create listen socket"));
        return false;
    }

    ListenSocket->SetReuseAddr(true);
    ListenSocket->SetNonBlocking(false);

    TSharedRef<FInternetAddr> Addr = SocketSub->CreateInternetAddr();
    Addr->SetAnyAddress();
    Addr->SetPort(ListenPort);

    if (!ListenSocket->Bind(*Addr))
    {
        UE_LOG(LogWSServer, Error, TEXT("WSServer: failed to bind to port %d"), ListenPort);
        SocketSub->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        return false;
    }

    if (!ListenSocket->Listen(8))
    {
        UE_LOG(LogWSServer, Error, TEXT("WSServer: failed to listen on port %d"), ListenPort);
        SocketSub->DestroySocket(ListenSocket);
        ListenSocket = nullptr;
        return false;
    }

    UE_LOG(LogWSServer, Log, TEXT("WSServer: listening on port %d"), ListenPort);
    return true;
}

uint32 FSMLWebSocketServerRunnable::Run()
{
    while (!bStopping.load())
    {
        // Accept any pending connections (non-blocking poll).
        bool bHasPending = false;
        if (ListenSocket && ListenSocket->HasPendingConnection(bHasPending) && bHasPending)
        {
            TSharedRef<FInternetAddr> RemoteAddr = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
            FSocket* NewSocket = ListenSocket->Accept(*RemoteAddr, TEXT("SMLWSClient"));
            if (NewSocket)
            {
                const FString ClientId = FString::Printf(TEXT("%llu"), NextClientId.fetch_add(1));
                const FString Remote   = RemoteAddr->ToString(true);

                FClientState State;
                State.Socket        = NewSocket;
                State.RemoteAddress = Remote;
                State.bHandshakeDone = false;

                if (PerformHandshake(State))
                {
                    State.bHandshakeDone = true;
                    {
                        FScopeLock L(&ClientMutex);
                        Clients.Add(ClientId, MoveTemp(State));
                    }

                    TWeakObjectPtr<USMLWebSocketServer> WeakOwner = Owner;
                    AsyncTask(ENamedThreads::GameThread, [WeakOwner, ClientId, Remote]()
                    {
                        if (USMLWebSocketServer* O = WeakOwner.Get())
                            O->Internal_OnClientConnected(ClientId, Remote);
                    });
                }
                else
                {
                    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(NewSocket);
                }
            }
        }

        // Drain outbound queue.
        FOutboundMsg Msg;
        while (OutboundQueue.Dequeue(Msg))
        {
            FScopeLock L(&ClientMutex);
            if (Msg.ClientId.IsEmpty())
            {
                // Broadcast.
                for (auto& KV : Clients)
                    SendFrame(KV.Value.Socket, Msg.Frame);
            }
            else
            {
                FClientState* C = Clients.Find(Msg.ClientId);
                if (C) SendFrame(C->Socket, Msg.Frame);
            }
        }

        // Read from connected clients.
        TArray<FString> ToRemove;
        {
            FScopeLock L(&ClientMutex);
            for (auto& KV : Clients)
            {
                FClientState& C = KV.Value;
                uint32 Pending = 0;
                if (!C.Socket->HasPendingData(Pending)) { ToRemove.Add(KV.Key); continue; }
                if (Pending == 0) continue;

                const int32 OldLen = C.RecvBuffer.Num();
                C.RecvBuffer.SetNum(OldLen + static_cast<int32>(Pending));
                int32 Read = 0;
                if (!C.Socket->Recv(C.RecvBuffer.GetData() + OldLen, static_cast<int32>(Pending), Read))
                {
                    ToRemove.Add(KV.Key);
                    continue;
                }
                C.RecvBuffer.SetNum(OldLen + Read);

                if (!ProcessFrames(KV.Key, C))
                    ToRemove.Add(KV.Key);
            }
        }

        for (const FString& Id : ToRemove)
        {
            FClientState Removed;
            {
                FScopeLock L(&ClientMutex);
                if (Clients.RemoveAndCopyValue(Id, Removed))
                {
                    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Removed.Socket);
                }
            }
            TWeakObjectPtr<USMLWebSocketServer> WeakOwner = Owner;
            AsyncTask(ENamedThreads::GameThread, [WeakOwner, Id]()
            {
                if (USMLWebSocketServer* O = WeakOwner.Get())
                    O->Internal_OnClientDisconnected(Id, TEXT("connection closed"));
            });
        }

        FPlatformProcess::SleepNoStats(0.005f);
    }

    // Close all client sockets on shutdown.
    FScopeLock L(&ClientMutex);
    for (auto& KV : Clients)
        ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(KV.Value.Socket);
    Clients.Empty();

    return 0;
}

void FSMLWebSocketServerRunnable::Stop()
{
    bStopping.store(true);
}

void FSMLWebSocketServerRunnable::Exit()
{
}

// ─────────────────────────────────────────────────────────────────────────────
//  Handshake
// ─────────────────────────────────────────────────────────────────────────────

bool FSMLWebSocketServerRunnable::PerformHandshake(FClientState& Client)
{
    // Read HTTP request headers.
    TArray<uint8> HeaderBuf;
    HeaderBuf.Reserve(1024);
    uint8 Ch = 0;
    int32 Recv = 0;

    for (int32 i = 0; i < 8192; ++i)
    {
        if (!Client.Socket->Recv(&Ch, 1, Recv) || Recv == 0) return false;
        HeaderBuf.Add(Ch);
        // Look for \r\n\r\n.
        const int32 N = HeaderBuf.Num();
        if (N >= 4 &&
            HeaderBuf[N-4] == '\r' && HeaderBuf[N-3] == '\n' &&
            HeaderBuf[N-2] == '\r' && HeaderBuf[N-1] == '\n')
            break;
    }

    FString Headers = FUTF8ToTCHAR(reinterpret_cast<const ANSICHAR*>(HeaderBuf.GetData()), HeaderBuf.Num()).Get();

    // Extract Sec-WebSocket-Key.
    FString Key;
    const FString Marker = TEXT("Sec-WebSocket-Key: ");
    int32 KeyPos = Headers.Find(Marker, ESearchCase::IgnoreCase);
    if (KeyPos == INDEX_NONE) return false;
    int32 Start = KeyPos + Marker.Len();
    int32 End   = Headers.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, Start);
    if (End == INDEX_NONE) return false;
    Key = Headers.Mid(Start, End - Start).TrimStartAndEnd();

    // Compute accept key: SHA1(Key + GUID) → Base64.
    const FString Combined = Key + TEXT("258EAFA5-E914-47DA-95CA-C5AB0DC85B11");
    FSHA1 Sha;
    Sha.Update(reinterpret_cast<const uint8*>(TCHAR_TO_ANSI(*Combined)),
               static_cast<uint32>(FTCHARToUTF8(*Combined).Length()));
    Sha.Final();
    uint8 Hash[20];
    Sha.GetHash(Hash);
    const FString Accept = FBase64::Encode(Hash, 20);

    // ── API token check ──────────────────────────────────────────────────────
    if (!ApiToken.IsEmpty())
    {
        FString AuthValue;
        const FString AuthPrefix = TEXT("Authorization:");
        int32 AuthIdx = Headers.Find(AuthPrefix, ESearchCase::IgnoreCase);
        if (AuthIdx != INDEX_NONE)
        {
            const int32 ValueStart = AuthIdx + AuthPrefix.Len();
            int32 LineEnd = Headers.Find(TEXT("\r\n"), ESearchCase::CaseSensitive, ESearchDir::FromStart, ValueStart);
            if (LineEnd == INDEX_NONE) LineEnd = Headers.Len();
            AuthValue = Headers.Mid(ValueStart, LineEnd - ValueStart).TrimStartAndEnd();
        }

        const FString Expected = FString(TEXT("Bearer ")) + ApiToken;
        if (AuthValue != Expected)
        {
            const FString Response401 =
                TEXT("HTTP/1.1 401 Unauthorized\r\n")
                TEXT("Content-Length: 0\r\n")
                TEXT("Connection: close\r\n\r\n");
            FTCHARToUTF8 R401Utf8(*Response401);
            int32 Sent401 = 0;
            Client.Socket->Send(
                reinterpret_cast<const uint8*>(R401Utf8.Get()),
                R401Utf8.Length(), Sent401);
            UE_LOG(LogWSServer, Warning, TEXT("WSServer: Rejected client — invalid or missing API token."));
            return false;
        }
    }

    // Send HTTP 101 response.
    FString Response = FString(TEXT("HTTP/1.1 101 Switching Protocols\r\n")
                               TEXT("Upgrade: websocket\r\n")
                               TEXT("Connection: Upgrade\r\n"))
                       + TEXT("Sec-WebSocket-Accept: ") + Accept + TEXT("\r\n")
                       + TEXT("\r\n");
    FTCHARToUTF8 ResponseUtf8(*Response);
    int32 Sent = 0;
    return Client.Socket->Send(
        reinterpret_cast<const uint8*>(ResponseUtf8.Get()),
        ResponseUtf8.Length(), Sent)
        && Sent == ResponseUtf8.Length();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Frame processing
// ─────────────────────────────────────────────────────────────────────────────

bool FSMLWebSocketServerRunnable::ProcessFrames(const FString& ClientId, FClientState& Client)
{
    TArray<uint8>& Buf = Client.RecvBuffer;

    while (Buf.Num() >= 2)
    {
        const uint8 B0 = Buf[0];
        const uint8 B1 = Buf[1];
        const uint8 Opcode   = B0 & 0x0F;
        const bool  bMasked  = (B1 & 0x80) != 0;
        uint64 PayloadLen    = B1 & 0x7F;

        int32 HeaderSize = 2;
        if (PayloadLen == 126)
        {
            if (Buf.Num() < 4) break;
            PayloadLen = (static_cast<uint64>(Buf[2]) << 8) | Buf[3];
            HeaderSize = 4;
        }
        else if (PayloadLen == 127)
        {
            if (Buf.Num() < 10) break;
            PayloadLen = 0;
            for (int32 i = 0; i < 8; ++i)
                PayloadLen = (PayloadLen << 8) | Buf[2 + i];
            HeaderSize = 10;
        }

        const int32 MaskSize = bMasked ? 4 : 0;
        const int32 TotalSize = HeaderSize + MaskSize + static_cast<int32>(PayloadLen);
        if (Buf.Num() < TotalSize) break;

        if (Opcode == 0x8) // Close
        {
            Buf.RemoveAt(0, TotalSize);
            return false;
        }

        if (Opcode == 0x1 || Opcode == 0x0) // Text or continuation
        {
            uint8 Mask[4] = {0,0,0,0};
            if (bMasked)
            {
                for (int32 i = 0; i < 4; ++i)
                    Mask[i] = Buf[HeaderSize + i];
            }

            TArray<uint8> Payload;
            Payload.SetNum(static_cast<int32>(PayloadLen));
            for (int32 i = 0; i < static_cast<int32>(PayloadLen); ++i)
                Payload[i] = Buf[HeaderSize + MaskSize + i] ^ (bMasked ? Mask[i % 4] : 0);

            const FString Text = FString(UTF8_TO_TCHAR(reinterpret_cast<const ANSICHAR*>(Payload.GetData())));

            TWeakObjectPtr<USMLWebSocketServer> WeakOwner = Owner;
            const FString CId = ClientId;
            AsyncTask(ENamedThreads::GameThread, [WeakOwner, CId, Text]()
            {
                if (USMLWebSocketServer* O = WeakOwner.Get())
                    O->Internal_OnClientMessage(CId, Text);
            });
        }

        Buf.RemoveAt(0, TotalSize);
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Outbound helpers
// ─────────────────────────────────────────────────────────────────────────────

TArray<uint8> FSMLWebSocketServerRunnable::BuildTextFrame(const FString& Text)
{
    FTCHARToUTF8 UTF8(*Text);
    const int32 PayloadLen = UTF8.Length();

    TArray<uint8> Frame;
    Frame.Add(0x81); // FIN + text opcode

    if (PayloadLen <= 125)
    {
        Frame.Add(static_cast<uint8>(PayloadLen));
    }
    else if (PayloadLen <= 65535)
    {
        Frame.Add(126);
        Frame.Add(static_cast<uint8>(PayloadLen >> 8));
        Frame.Add(static_cast<uint8>(PayloadLen & 0xFF));
    }
    else
    {
        Frame.Add(127);
        for (int32 i = 7; i >= 0; --i)
            Frame.Add(static_cast<uint8>((static_cast<uint64>(PayloadLen) >> (i * 8)) & 0xFF));
    }

    const uint8* Data = reinterpret_cast<const uint8*>(UTF8.Get());
    Frame.Append(Data, PayloadLen);
    return Frame;
}

bool FSMLWebSocketServerRunnable::SendFrame(FSocket* Socket, const TArray<uint8>& Frame)
{
    int32 Sent = 0;
    return Socket->Send(Frame.GetData(), Frame.Num(), Sent) && Sent == Frame.Num();
}

void FSMLWebSocketServerRunnable::BroadcastText(const FString& Message)
{
    FOutboundMsg Msg;
    Msg.ClientId = TEXT(""); // empty = broadcast
    Msg.Frame    = BuildTextFrame(Message);
    OutboundQueue.Enqueue(MoveTemp(Msg));
}

void FSMLWebSocketServerRunnable::SendTextToClient(const FString& ClientId, const FString& Message)
{
    FOutboundMsg Msg;
    Msg.ClientId = ClientId;
    Msg.Frame    = BuildTextFrame(Message);
    OutboundQueue.Enqueue(MoveTemp(Msg));
}

void FSMLWebSocketServerRunnable::DisconnectClient(const FString& ClientId)
{
    FClientState Removed;
    {
        FScopeLock L(&ClientMutex);
        if (!Clients.RemoveAndCopyValue(ClientId, Removed))
            return;
    }
    ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(Removed.Socket);

    TWeakObjectPtr<USMLWebSocketServer> WeakOwner = Owner;
    AsyncTask(ENamedThreads::GameThread, [WeakOwner, ClientId]()
    {
        if (USMLWebSocketServer* O = WeakOwner.Get())
            O->Internal_OnClientDisconnected(ClientId, TEXT("server disconnected"));
    });
}

int32 FSMLWebSocketServerRunnable::GetClientCount() const
{
    FScopeLock L(const_cast<FCriticalSection*>(&ClientMutex));
    return Clients.Num();
}

TArray<FString> FSMLWebSocketServerRunnable::GetClientIds() const
{
    FScopeLock L(const_cast<FCriticalSection*>(&ClientMutex));
    TArray<FString> Ids;
    Clients.GetKeys(Ids);
    return Ids;
}
