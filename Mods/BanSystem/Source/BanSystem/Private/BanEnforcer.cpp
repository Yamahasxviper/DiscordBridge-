// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/enforcer.ts

#include "BanEnforcer.h"
#include "BanDatabase.h"
#include "PlayerSessionRegistry.h"

// Pull in the full FUniqueNetIdRepl definition that was forward-declared in the header.
#include "GameFramework/OnlineReplStructs.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "Engine/NetConnection.h"
#include "TimerManager.h"
// CSS dedicated-server component hooked to capture client login Options before
// UNetConnection::URL is overwritten with the server bind address.
#include "FGGameModeDSComponent.h"
// SML native hook macros used for the PreLogin / NotifyPlayerLogout hooks.
#include "Patching/NativeHookManager.h"

DEFINE_LOG_CATEGORY(LogBanEnforcer);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Collection.InitializeDependency<UPlayerSessionRegistry>();
    Super::Initialize(Collection);

    // Primary enforcement hook — AGameModeBase::PostLogin broadcasts this event
    // and CSS (confirmed by SML) calls it on every player join.
    // Note: CSS routes PreLogin through UFGDedicatedServerGameModeComponentInterface
    // rather than AGameModeBase::PreLogin, so FGameModeEvents::GameModePreLoginEvent
    // does not fire on CSS dedicated servers and is not hooked here.
    // Use AddLambda + TWeakObjectPtr instead of AddUObject to avoid C2665 in
    // CSS 5.3's strict template matching while still handling object lifetime safely.
    TWeakObjectPtr<UBanEnforcer> WeakThis(this);
    PostLoginHandle = FGameModeEvents::GameModePostLoginEvent.AddLambda(
        [WeakThis](AGameModeBase* GameMode, APlayerController* NewPlayer)
        {
            if (UBanEnforcer* Enforcer = WeakThis.Get())
                Enforcer->OnPostLogin(GameMode, NewPlayer);
        });

    // ──────────────────────────────────────────────────────────────────────────
    // PreLogin hook — capture the EOS PUID from the client Options string.
    //
    // Root cause of the "gave up waiting for EOS PUID" / "/whoami UNCOMPLETED"
    // bug:  On CSS DS 1.1.0, UNetConnection::URL is initialised from the
    // server's bind address, NOT from the client's join URL.  So
    // Conn->URL.GetOption("ClientIdentity=") always returns null, and
    // ExtractEosPuidFromConnectionUrl silently returns empty every time.
    //
    // UFGGameModeDSComponent::PreLogin IS called with the full Options string
    // (same string logged as "Login request: ?ClientIdentity=...").  We hook it
    // here with SUBSCRIBE_UOBJECT_METHOD_AFTER to extract and cache the decoded
    // EOS PUID, keyed by UNetConnection, before PostLogin fires.
    // ──────────────────────────────────────────────────────────────────────────
    PreLoginHookHandle = SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGGameModeDSComponent, PreLogin,
        ([WeakThis](const bool& bLoginAllowed,
                    UFGGameModeDSComponent* /*Self*/,
                    UPlayer* NewPlayer,
                    const FString& Options,
                    const FUniqueNetIdRepl& /*UniqueId*/,
                    FString& /*ErrorMessage*/,
                    TSharedPtr<FDedicatedServerGameModeComponentPreLoginDataInterface>& /*OutPreLoginData*/)
        {
            // Only cache for accepted logins.
            if (!bLoginAllowed) return;

            UBanEnforcer* Enforcer = WeakThis.Get();
            if (!Enforcer) return;

            UNetConnection* Conn = Cast<UNetConnection>(NewPlayer);
            if (!Conn) return;

            // Options format: "?ClientIdentity=<hex>?EntryTicket=...?Name=..."
            // Extract the hex blob from between "ClientIdentity=" and the next "?".
            const FString ClientIdentityKey = TEXT("ClientIdentity=");
            int32 KeyIdx = Options.Find(ClientIdentityKey, ESearchCase::IgnoreCase);
            if (KeyIdx == INDEX_NONE) return;

            FString HexStr = Options.Mid(KeyIdx + ClientIdentityKey.Len());
            int32 EndIdx;
            if (HexStr.FindChar(TEXT('?'), EndIdx))
                HexStr = HexStr.Left(EndIdx);
            HexStr.TrimStartAndEndInline();

            // Decode EOS PUID from the binary blob encoded as a lowercase hex string.
            // Layout: offsets 0-7 = 4-byte LE header; offsets 8-71 = 32 ASCII PUID bytes.
            if (HexStr.Len() < 72) return;

            FString Puid;
            Puid.Reserve(32);
            for (int32 i = 8; i < 72; i += 2)
            {
                const TCHAR Hi = FChar::ToLower(HexStr[i]);
                const TCHAR Lo = FChar::ToLower(HexStr[i + 1]);
                if (!FChar::IsHexDigit(Hi) || !FChar::IsHexDigit(Lo)) return;

                const uint8 HiN = (Hi >= TEXT('a')) ? (uint8)(Hi - TEXT('a') + 10) : (uint8)(Hi - TEXT('0'));
                const uint8 LoN = (Lo >= TEXT('a')) ? (uint8)(Lo - TEXT('a') + 10) : (uint8)(Lo - TEXT('0'));
                const TCHAR Ch = static_cast<TCHAR>((HiN << 4) | LoN);
                if (!FChar::IsHexDigit(Ch)) return;
                Puid.AppendChar(Ch);
            }
            if (Puid.Len() != 32) return;
            Puid = Puid.ToLower();

            Enforcer->CachedConnectionPuids.Add(TWeakObjectPtr<UNetConnection>(Conn), Puid);
            UE_LOG(LogBanEnforcer, Log,
                TEXT("BanEnforcer: cached EOS PUID %s for incoming connection (via PreLogin Options)"),
                *Puid);

            // Also cache the remote IP address while the connection object is
            // available in the PreLogin callback.
            const FString RemoteIp = Conn->LowLevelGetRemoteAddress(/*bAppendPort=*/false);
            if (!RemoteIp.IsEmpty())
            {
                Enforcer->CachedConnectionIPs.Add(TWeakObjectPtr<UNetConnection>(Conn), RemoteIp);
                UE_LOG(LogBanEnforcer, Log,
                    TEXT("BanEnforcer: cached remote IP %s for incoming connection"),
                    *RemoteIp);
            }
        }));

    // Evict cache entries when the player's connection is torn down.
    PlayerLogoutHookHandle = SUBSCRIBE_UOBJECT_METHOD_AFTER(UFGGameModeDSComponent, NotifyPlayerLogout,
        ([WeakThis](UFGGameModeDSComponent* /*Self*/, AController* ExitingController)
        {
            UBanEnforcer* Enforcer = WeakThis.Get();
            if (!Enforcer) return;

            if (UNetConnection* Conn = Cast<UNetConnection>(
                    ExitingController ? ExitingController->GetNetConnection() : nullptr))
            {
                const int32 Removed = Enforcer->CachedConnectionPuids.Remove(TWeakObjectPtr<UNetConnection>(Conn));
                if (Removed > 0)
                    UE_LOG(LogBanEnforcer, Log,
                        TEXT("BanEnforcer: evicted cached EOS PUID on player logout"));
                Enforcer->CachedConnectionIPs.Remove(TWeakObjectPtr<UNetConnection>(Conn));
            }

            // Also prune any fully garbage-collected (stale) entries.
            for (auto It = Enforcer->CachedConnectionPuids.CreateIterator(); It; ++It)
            {
                if (!It.Key().IsValid())
                    It.RemoveCurrent();
            }
            for (auto It = Enforcer->CachedConnectionIPs.CreateIterator(); It; ++It)
            {
                if (!It.Key().IsValid())
                    It.RemoveCurrent();
            }
        }));

    UE_LOG(LogBanEnforcer, Log, TEXT("BanEnforcer: login enforcement active (PostLogin)"));
}

void UBanEnforcer::Deinitialize()
{
    FGameModeEvents::GameModePostLoginEvent.Remove(PostLoginHandle);
    PostLoginHandle.Reset();

    // Remove the PreLogin / NotifyPlayerLogout SML hooks.
    UNSUBSCRIBE_UOBJECT_METHOD(UFGGameModeDSComponent, PreLogin, PreLoginHookHandle);
    PreLoginHookHandle.Reset();
    UNSUBSCRIBE_UOBJECT_METHOD(UFGGameModeDSComponent, NotifyPlayerLogout, PlayerLogoutHookHandle);
    PlayerLogoutHookHandle.Reset();

    // Clear the EOS PUID cache.
    CachedConnectionPuids.Empty();
    CachedConnectionIPs.Empty();

    // Cancel any in-flight identity poll and clear the queue.
    PendingBanChecks.Empty();

    // Use the stored world reference because GetGameInstance()->GetWorld() may
    // already return null by the time Deinitialize() is called on shutdown.
    if (UWorld* World = PollTimerWorld.Get())
        World->GetTimerManager().ClearTimer(PollTimerHandle);
    PollTimerWorld.Reset();

    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  PostLogin enforcement (primary path on CSS dedicated servers)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::OnPostLogin(AGameModeBase* GameMode, APlayerController* NewPlayer)
{
    if (!GameMode || !NewPlayer) return;

    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB)
    {
        UE_LOG(LogBanEnforcer, Error,
            TEXT("BanEnforcer: OnPostLogin — UBanDatabase subsystem unavailable, skipping ban check"));
        return;
    }

    UWorld* World = GI->GetWorld();
    if (!World) return;

    // Attempt an immediate ban check when PlayerState and identity are ready.
    // On CSS dedicated servers APlayerState::UniqueId is typically set during
    // AGameModeBase::Login() so this fast path fires for most EOS players.
    if (NewPlayer->PlayerState)
    {
        const FUniqueNetIdRepl& NetIdAtLogin = NewPlayer->PlayerState->GetUniqueId();
        if (NetIdAtLogin.IsValid() && NetIdAtLogin.GetType() != FName(TEXT("NONE")))
        {
            // Defer the kick one tick so CSS's own PostLogin code can finish
            // before the connection is closed.  Closing synchronously inside the
            // PostLogin event can crash if AFGGameMode::PostLogin() runs more
            // setup code after Super::PostLogin() returns.
            TWeakObjectPtr<UBanEnforcer> WeakThis(this);
            TWeakObjectPtr<APlayerController> WeakPC(NewPlayer);
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateLambda([WeakThis, WeakPC]()
                {
                    UBanEnforcer* Enforcer = WeakThis.Get();
                    APlayerController* PC  = WeakPC.Get();
                    if (!Enforcer || !IsValid(PC)) return;

                    UGameInstance* GI2 = Enforcer->GetGameInstance();
                    if (!GI2) return;
                    UWorld* W2 = GI2->GetWorld();
                    UBanDatabase* DB2 = GI2->GetSubsystem<UBanDatabase>();
                    if (W2 && DB2)
                        Enforcer->PerformBanCheckForPlayer(W2, PC, DB2);
                }));
            return;
        }
    }

    // CSS DS 1.1.0 workaround: GetType()==NONE because EOS online subsystem is
    // offline (IsOnline=false).  The EOS PUID is still transmitted in the
    // ClientIdentity URL option — try to extract it directly from the connection.
    {
        const FString EosPuid = ExtractEosPuidFromConnectionUrl(NewPlayer);
        if (!EosPuid.IsEmpty())
        {
            const FString Uid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
            const FString PlayerName = (NewPlayer->PlayerState)
                ? NewPlayer->PlayerState->GetPlayerName()
                : TEXT("(unknown)");

            UE_LOG(LogBanEnforcer, Log,
                TEXT("BanEnforcer: extracted EOS PUID from connection URL for '%s' (%s) — deferred ban check"),
                *PlayerName, *Uid);

            TWeakObjectPtr<UBanEnforcer> WeakThis(this);
            TWeakObjectPtr<APlayerController> WeakPC(NewPlayer);
            World->GetTimerManager().SetTimerForNextTick(
                FTimerDelegate::CreateLambda([WeakThis, WeakPC, Uid]()
                {
                    UBanEnforcer* Enforcer = WeakThis.Get();
                    APlayerController* PC  = WeakPC.Get();
                    if (!Enforcer || !IsValid(PC)) return;

                    UGameInstance* GI2 = Enforcer->GetGameInstance();
                    if (!GI2) return;
                    UWorld* W2 = GI2->GetWorld();
                    UBanDatabase* DB2 = GI2->GetSubsystem<UBanDatabase>();
                    if (W2 && DB2)
                        Enforcer->PerformBanCheckForUid(W2, PC, DB2, Uid);
                }));
            return;
        }
    }

    // PlayerState is null OR identity not yet available AND no URL PUID found.
    // CSS may initialise PlayerState asynchronously after PostLogin fires.
    // Queue the player and poll every 0.5 s until both PlayerState and identity
    // are available.
    PendingBanChecks.Add({NewPlayer, 0});

    if (!World->GetTimerManager().IsTimerActive(PollTimerHandle))
    {
        PollTimerWorld = World;
        World->GetTimerManager().SetTimer(
            PollTimerHandle,
            FTimerDelegate::CreateUObject(this, &UBanEnforcer::ProcessPendingBanChecks),
            0.5f,
            /*bLoop=*/true);

        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: started identity-poll timer for pending ban checks"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Identity poll timer  (fires every 0.5 s while there are pending checks)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::ProcessPendingBanChecks()
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UWorld* World = GI->GetWorld();
    if (!World) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    // Iterate backwards so we can safely RemoveAt while iterating.
    for (int32 i = PendingBanChecks.Num() - 1; i >= 0; --i)
    {
        FPendingBanCheck& Check = PendingBanChecks[i];
        APlayerController* PC = Check.Player.Get();

        // Player already disconnected.
        if (!IsValid(PC))
        {
            PendingBanChecks.RemoveAt(i);
            continue;
        }

        // PlayerState not yet available — CSS may initialise it asynchronously
        // after PostLogin.  Treat this the same as an unresolved identity: burn
        // one attempt and keep the player in the queue.
        if (!PC->PlayerState)
        {
            if (++Check.Attempts >= FPendingBanCheck::MaxAttempts)
            {
                UE_LOG(LogBanEnforcer, Warning,
                    TEXT("BanEnforcer: gave up waiting for PlayerState for player after %d attempts (~%.0f s)"),
                    FPendingBanCheck::MaxAttempts,
                    FPendingBanCheck::MaxAttempts * 0.5f);
                PendingBanChecks.RemoveAt(i);
            }
            // else: PlayerState not ready yet — keep queued, retry next tick.
            continue;
        }

        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        if (!NetId.IsValid() || NetId.GetType() == FName(TEXT("NONE")))
        {
            // CSS DS 1.1.0 workaround: identity stays NONE when EOS subsystem is
            // offline.  Try extracting the EOS PUID from the connection URL before
            // burning another attempt (the PUID is baked into ClientIdentity and
            // doesn't change, so a single successful decode is enough).
            const FString EosPuid = ExtractEosPuidFromConnectionUrl(PC);
            if (!EosPuid.IsEmpty())
            {
                const FString Uid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
                PendingBanChecks.RemoveAt(i);
                PerformBanCheckForUid(World, PC, DB, Uid);
                continue;
            }

            if (++Check.Attempts >= FPendingBanCheck::MaxAttempts)
            {
                UE_LOG(LogBanEnforcer, Warning,
                    TEXT("BanEnforcer: gave up waiting for UniqueId (or NONE-type EOS PUID) for player '%s' after %d attempts (~%.0f s)"),
                    *PC->PlayerState->GetPlayerName(),
                    FPendingBanCheck::MaxAttempts,
                    FPendingBanCheck::MaxAttempts * 0.5f);
                PendingBanChecks.RemoveAt(i);
            }
            // else: identity not ready yet or still NONE type — keep queued, retry next tick.
            continue;
        }

        // Identity is now valid — run the ban check and remove from queue.
        PendingBanChecks.RemoveAt(i);
        PerformBanCheckForPlayer(World, PC, DB);
    }

    // Stop the poll timer when the queue is drained to avoid unnecessary work.
    if (PendingBanChecks.IsEmpty())
    {
        World->GetTimerManager().ClearTimer(PollTimerHandle);
        PollTimerWorld.Reset();
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: identity-poll timer stopped (queue empty)"));
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban lookup + kick for a player whose identity has been confirmed valid
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::PerformBanCheckForPlayer(UWorld* World, APlayerController* PC, UBanDatabase* DB)
{
    if (!World || !IsValid(PC) || !DB || !PC->PlayerState) return;

    const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
    if (!NetId.IsValid()) return;

    // Use the direct FUniqueNetIdRepl accessors (GetType/ToString on the repl
    // struct itself) instead of dereferencing through operator->. On CSS dedicated
    // servers, EOS V2 (PUID) identities do not use the TSharedPtr<FUniqueNetId>
    // storage path — the inner pointer slot holds a raw EOS handle value, not a
    // valid C++ object, so calling NetId->GetType() causes a SIGSEGV.
    // FUniqueNetIdRepl::GetType() and FUniqueNetIdRepl::ToString() are safe for
    // EOS PUID (V2) identities.
    const FString Platform = NetId.GetType().ToString().ToUpper();
    // Guard against an unresolved EOS identity: on CSS DS, FUniqueNetIdRepl::IsValid()
    // can return true before the EOS PUID provider has been assigned, leaving GetType()
    // as "NONE" and ToString() as "".  Recording or enforcing a "NONE:" UID is wrong.
    if (Platform == TEXT("NONE"))
    {
        UE_LOG(LogBanEnforcer, Warning,
            TEXT("BanEnforcer: PerformBanCheckForPlayer — identity type is NONE for player '%s', skipping (EOS PUID not yet resolved)"),
            *PC->PlayerState->GetPlayerName());
        return;
    }
    // Normalize to lowercase so UIDs match bans stored by chat commands
    // (which also lowercase EOS PUIDs via the /ban and /tempban resolution paths).
    const FString RawId    = NetId.ToString().ToLower();
    const FString Uid      = UBanDatabase::MakeUid(Platform, RawId);

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: checking ban status for player '%s' (%s: %s)"),
        *PC->PlayerState->GetPlayerName(), *Platform, *RawId);

    FBanEntry Entry;
    if (!DB->IsCurrentlyBannedByAnyId(Uid, Entry))
    {
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: player '%s' (%s) is not banned — allowing join"),
            *PC->PlayerState->GetPlayerName(), *Uid);

        // Record the session for identity-persistence tracking (Gap 4).
        // This lets admins use /playerhistory to audit which UIDs a player
        // has connected with across server sessions.
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            if (UPlayerSessionRegistry* Registry = GI->GetSubsystem<UPlayerSessionRegistry>())
                Registry->RecordSession(Uid, PC->PlayerState->GetPlayerName(), GetCachedIpForPlayer(PC));
        }

        return;
    }

    const FString KickMsg = Entry.GetKickMessage();
    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: kicking banned player %s (%s) — %s"),
        *RawId, *Platform, *KickMsg);

    // Try the standard session kick first (sends a kick message to the client).
    AGameModeBase* GM = World->GetAuthGameMode();
    if (GM && GM->GameSession)
    {
        GM->GameSession->KickPlayer(PC, FText::FromString(KickMsg));
    }

    // Hard fallback: close the net connection directly.
    // CSS's AFGGameSession::KickPlayer may not fully disconnect the client in all
    // server configurations.  Closing UNetConnection guarantees disconnection.
    // NOTE: Do NOT call PC->Destroy() — the connection close triggers the standard
    // UE cleanup path.  Explicitly destroying the PC before that runs can cause
    // crashes and ghost players.
    if (IsValid(PC))
    {
        if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
        {
            Conn->Close();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Kick helper
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::KickConnectedPlayer(UWorld* World, const FString& Uid, const FString& Reason)
{
    if (!World) return;

    AGameModeBase* GM = World->GetAuthGameMode();
    if (!GM || !GM->GameSession) return;

    // Parse the platform and raw player ID from the compound UID so we can fall back
    // to a second matching strategy when GetUniqueId() is not yet populated.
    FString UidPlatform, UidRawId;
    UBanDatabase::ParseUid(Uid, UidPlatform, UidRawId);

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState) continue;

        // ── Primary match: compound UID via FUniqueNetIdRepl ─────────────────
        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        bool bMatched = false;
        if (NetId.IsValid() && NetId.GetType() != FName(TEXT("NONE")))
        {
            // Use direct FUniqueNetIdRepl accessors — safe for EOS V2 PUIDs.
            // (See PerformBanCheckForPlayer for the full explanation.)
            const FString Platform   = NetId.GetType().ToString().ToUpper();
            // Normalize to lowercase to match stored ban UIDs.
            const FString RawId      = NetId.ToString().ToLower();
            const FString PlayerUid  = UBanDatabase::MakeUid(Platform, RawId);

            if (PlayerUid == Uid)
                bMatched = true;
        }

        if (!bMatched)
        {
            // ── Fallback A: CSS DS 1.1.0 — GetType()==NONE but EOS PUID may be
            //    available from the connection URL's ClientIdentity option.
            const FString UrlPuid = ExtractEosPuidFromConnectionUrl(PC);
            if (!UrlPuid.IsEmpty())
            {
                const FString UrlUid = UBanDatabase::MakeUid(TEXT("EOS"), UrlPuid);
                if (UrlUid == Uid)
                    bMatched = true;
            }
        }

        if (!bMatched)
        {
            // ── Fallback B: match by the raw player ID embedded in the UID
            //    against the PlayerState name (best-effort — only fires when
            //    UniqueId is not yet set).
            const FString PlayerName = PC->PlayerState->GetPlayerName();
            if (PlayerName.Equals(UidRawId, ESearchCase::IgnoreCase))
            {
                UE_LOG(LogBanEnforcer, Warning,
                    TEXT("BanEnforcer: KickConnectedPlayer — matched '%s' by name fallback (UniqueId not yet set)"),
                    *PlayerName);
                bMatched = true;
            }
        }

        if (!bMatched) continue;

        GM->GameSession->KickPlayer(PC, FText::FromString(Reason));

        // Hard fallback: close the net connection directly in case
        // CSS's AFGGameSession::KickPlayer does not fully disconnect the client.
        // NOTE: Do NOT call PC->Destroy() — the connection close triggers the
        // standard UE cleanup path.  Explicitly destroying the PC before that
        // cleanup runs can cause crashes or leave ghost players.
        if (IsValid(PC))
        {
            if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
            {
                Conn->Close();
            }
        }

        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: kicked connected player %s — %s"), *Uid, *Reason);
        return;
    }

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: KickConnectedPlayer — no connected player found for UID %s"), *Uid);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban check using a pre-computed UID (URL-extracted identity path)
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::PerformBanCheckForUid(UWorld* World, APlayerController* PC, UBanDatabase* DB, const FString& Uid)
{
    if (!World || !IsValid(PC) || !DB) return;

    FString Platform, RawId;
    UBanDatabase::ParseUid(Uid, Platform, RawId);

    const FString PlayerName = (PC->PlayerState)
        ? PC->PlayerState->GetPlayerName()
        : TEXT("(unknown)");

    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: checking ban status for player '%s' (%s: %s) [URL-extracted identity]"),
        *PlayerName, *Platform, *RawId);

    FBanEntry Entry;
    if (!DB->IsCurrentlyBannedByAnyId(Uid, Entry))
    {
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: player '%s' (%s) is not banned — allowing join"),
            *PlayerName, *Uid);

        // Record the session for identity-persistence tracking.
        UGameInstance* GI = GetGameInstance();
        if (GI)
        {
            if (UPlayerSessionRegistry* Registry = GI->GetSubsystem<UPlayerSessionRegistry>())
                Registry->RecordSession(Uid, PlayerName, GetCachedIpForPlayer(PC));
        }
        return;
    }

    const FString KickMsg = Entry.GetKickMessage();
    UE_LOG(LogBanEnforcer, Log,
        TEXT("BanEnforcer: kicking banned player %s (%s) — %s"),
        *RawId, *Platform, *KickMsg);

    AGameModeBase* GM = World->GetAuthGameMode();
    if (GM && GM->GameSession)
    {
        GM->GameSession->KickPlayer(PC, FText::FromString(KickMsg));
    }

    // Hard fallback: close the net connection directly.
    // NOTE: Do NOT call PC->Destroy() — the connection close triggers the
    // standard UE cleanup path.
    if (IsValid(PC))
    {
        if (UNetConnection* Conn = Cast<UNetConnection>(PC->Player))
        {
            Conn->Close();
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  CSS DS 1.1.0 EOS PUID extraction from connection URL
// ─────────────────────────────────────────────────────────────────────────────

FString UBanEnforcer::ExtractEosPuidFromConnectionUrl(APlayerController* PC)
{
    if (!IsValid(PC)) return FString();

    // ── Primary path: PreLogin hook cache ────────────────────────────────────
    // On CSS DS 1.1.0 the server-side UNetConnection::URL is the server's bind
    // address, so Conn->URL.GetOption("ClientIdentity=") always returns null.
    // The PreLogin hook (installed in Initialize) caches the decoded EOS PUID
    // from the client's Options string before PostLogin fires.  Check it first.
    UNetConnection* Conn = Cast<UNetConnection>(PC->Player);
    if (Conn)
    {
        if (UGameInstance* GI = PC->GetWorld() ? PC->GetWorld()->GetGameInstance() : nullptr)
        {
            if (UBanEnforcer* Enforcer = GI->GetSubsystem<UBanEnforcer>())
            {
                if (const FString* Cached = Enforcer->CachedConnectionPuids.Find(TWeakObjectPtr<UNetConnection>(Conn)))
                {
                    UE_LOG(LogBanEnforcer, Verbose,
                        TEXT("BanEnforcer: ExtractEosPuidFromConnectionUrl — resolved '%s' from PreLogin cache"),
                        **Cached);
                    return *Cached;
                }
            }
        }
    }

    // ── Fallback path: Conn->URL option (works if CSS ever sets it) ───────────
    // Kept as a belt-and-suspenders fallback; historically always returned null
    // on CSS DS 1.1.0 because Conn->URL holds the server bind address, not the
    // client join URL.  May become relevant for future CSS versions.
    if (!Conn) return FString();

    // The ClientIdentity query option contains the EOS PUID as ASCII bytes
    // embedded in a binary blob encoded as a lowercase hex string.
    //
    // Layout (hex string offsets):
    //   0 - 7  : 4-byte LE header (length or version field)
    //   8 - 71 : 32 ASCII bytes that form the 32-char EOS PUID
    //            e.g. hex pair "34" = 0x34 = 52 = '4', so "3438" → "48"
    //   72+    : additional platform data (flags, etc.)
    const TCHAR* Opt = Conn->URL.GetOption(TEXT("ClientIdentity="), nullptr);
    if (!Opt || !*Opt) return FString();

    const FString Hex(Opt);
    // Minimum: 8 header chars + 64 PUID-as-bytes chars = 72 hex chars
    if (Hex.Len() < 72) return FString();

    FString Puid;
    Puid.Reserve(32);

    for (int32 i = 8; i < 72; i += 2)
    {
        // Decode one hex byte pair into the ASCII character it represents.
        const TCHAR Hi = FChar::ToLower(Hex[i]);
        const TCHAR Lo = FChar::ToLower(Hex[i + 1]);

        if (!FChar::IsHexDigit(Hi) || !FChar::IsHexDigit(Lo)) return FString();

        const uint8 HiNibble = (Hi >= 'a') ? (uint8)(Hi - 'a' + 10) : (uint8)(Hi - '0');
        const uint8 LoNibble = (Lo >= 'a') ? (uint8)(Lo - 'a' + 10) : (uint8)(Lo - '0');
        const TCHAR Ch       = (TCHAR)((HiNibble << 4) | LoNibble);

        // Each decoded byte must itself be a hex character: EOS PUIDs are
        // 32-char lowercase hex strings.
        if (!FChar::IsHexDigit(Ch)) return FString();
        Puid.AppendChar(Ch);
    }

    if (Puid.Len() != 32) return FString();
    return Puid.ToLower();
}

// ─────────────────────────────────────────────────────────────────────────────
//  IP address lookup from the PreLogin cache
// ─────────────────────────────────────────────────────────────────────────────

FString UBanEnforcer::GetCachedIpForPlayer(APlayerController* PC) const
{
    if (!IsValid(PC)) return FString();

    UNetConnection* Conn = Cast<UNetConnection>(PC->Player);
    if (!Conn) return FString();

    if (const FString* Cached = CachedConnectionIPs.Find(TWeakObjectPtr<UNetConnection>(Conn)))
        return *Cached;

    return FString();
}
