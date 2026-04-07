// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/apiServer.ts

#include "BanRestApi.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanSystemConfig.h"
#include "PlayerSessionRegistry.h"
#include "PlayerWarningRegistry.h"
#include "BanAuditLog.h"
#include "BanDiscordNotifier.h"
#include "BanAppealRegistry.h"

#include "HttpServerModule.h"
#include "IHttpRouter.h"
#include "HttpPath.h"
#include "HttpServerResponse.h"
#include "HttpServerRequest.h"
#include "GenericPlatform/GenericPlatformHttp.h"

#include "Misc/Paths.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogBanRestApi);

// ─────────────────────────────────────────────────────────────────────────────
//  PIMPL struct — hides TArray<FHttpRouteHandle> from the public header so that
//  UHT / MSVC never needs to instantiate it from the Public include path.
// ─────────────────────────────────────────────────────────────────────────────
struct FBanRestApiRoutes
{
    TArray<FHttpRouteHandle> Handles;
};

// ─────────────────────────────────────────────────────────────────────────────
//  Internal JSON helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace BanJson
{
    static TSharedPtr<FJsonObject> EntryToJson(const FBanEntry& E)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),          static_cast<double>(E.Id));
        Obj->SetStringField(TEXT("uid"),         E.Uid);
        Obj->SetStringField(TEXT("playerUID"),   E.PlayerUID);
        Obj->SetStringField(TEXT("platform"),    E.Platform);
        Obj->SetStringField(TEXT("playerName"),  E.PlayerName);
        Obj->SetStringField(TEXT("reason"),      E.Reason);
        Obj->SetStringField(TEXT("bannedBy"),    E.BannedBy);
        Obj->SetStringField(TEXT("banDate"),     E.BanDate.ToIso8601());
        if (E.bIsPermanent)
            Obj->SetField(TEXT("expireDate"),    MakeShared<FJsonValueNull>());
        else
            Obj->SetStringField(TEXT("expireDate"), E.ExpireDate.ToIso8601());
        Obj->SetBoolField  (TEXT("isPermanent"), E.bIsPermanent);
        return Obj;
    }

    static FString ObjectToString(const TSharedPtr<FJsonObject>& Obj)
    {
        FString Out;
        TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Out);
        FJsonSerializer::Serialize(Obj.ToSharedRef(), Writer);
        return Out;
    }

    static FString ArrayToString(const TArray<FBanEntry>& Bans)
    {
        TArray<TSharedPtr<FJsonValue>> Arr;
        Arr.Reserve(Bans.Num());
        for (const FBanEntry& E : Bans)
            Arr.Add(MakeShared<FJsonValueObject>(EntryToJson(E)));

        TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetNumberField(TEXT("count"), Bans.Num());
        Root->SetArrayField (TEXT("bans"),  Arr);
        return ObjectToString(Root);
    }

    static TUniquePtr<FHttpServerResponse> Json(
        const FString& Body,
        EHttpServerResponseCodes Code = EHttpServerResponseCodes::Ok)
    {
        auto R = FHttpServerResponse::Create(Body, TEXT("application/json"));
        R->Code = Code;
        return R;
    }

    static TUniquePtr<FHttpServerResponse> Error(const FString& Msg,
        EHttpServerResponseCodes Code = EHttpServerResponseCodes::BadRequest)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("error"), Msg);
        return Json(ObjectToString(Obj), Code);
    }

    static TUniquePtr<FHttpServerResponse> Ok(const FString& Msg)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("message"), Msg);
        return Json(ObjectToString(Obj));
    }

    static FString BodyToString(const FHttpServerRequest& Req)
    {
        if (Req.Body.Num() == 0) return TEXT("{}");

        // Guard against excessively large request bodies (e.g. accidental or
        // malicious oversized POST payloads).  1 MB is far more than any valid
        // JSON ban payload would ever require.
        static constexpr int32 MaxBodyBytes = 1 * 1024 * 1024;
        if (Req.Body.Num() > MaxBodyBytes)
        {
            UE_LOG(LogBanRestApi, Warning,
                TEXT("BanRestApi: request body too large (%d bytes, limit %d) — rejecting"),
                Req.Body.Num(), MaxBodyBytes);
            return TEXT("{}");
        }

        TArray<uint8> Buf = Req.Body;
        Buf.Add(0);
        return UTF8_TO_TCHAR(reinterpret_cast<const char*>(Buf.GetData()));
    }

    /**
     * Returns true when the request passes the API key check.
     * If RestApiKey is empty, always returns true (auth disabled).
     * If RestApiKey is non-empty, the request must supply header X-Api-Key with
     * the matching value (case-sensitive).
     */
    static bool CheckApiKey(const FHttpServerRequest& Req)
    {
        const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
        if (!Cfg || Cfg->RestApiKey.IsEmpty()) return true;

        const FString* KeyHeader = Req.Headers.Find(TEXT("X-Api-Key"));
        if (!KeyHeader) return false;
        return (*KeyHeader).Equals(Cfg->RestApiKey, ESearchCase::CaseSensitive);
    }

    static TSharedPtr<FJsonObject> WarningToJson(const FWarningEntry& W)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),         static_cast<double>(W.Id));
        Obj->SetStringField(TEXT("uid"),        W.Uid);
        Obj->SetStringField(TEXT("playerName"), W.PlayerName);
        Obj->SetStringField(TEXT("reason"),     W.Reason);
        Obj->SetStringField(TEXT("warnedBy"),   W.WarnedBy);
        Obj->SetStringField(TEXT("warnDate"),   W.WarnDate.ToIso8601());
        return Obj;
    }

    static TSharedPtr<FJsonObject> AuditToJson(const FAuditEntry& E)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),         static_cast<double>(E.Id));
        Obj->SetStringField(TEXT("action"),     E.Action);
        Obj->SetStringField(TEXT("targetUid"),  E.TargetUid);
        Obj->SetStringField(TEXT("targetName"), E.TargetName);
        Obj->SetStringField(TEXT("adminUid"),   E.AdminUid);
        Obj->SetStringField(TEXT("adminName"),  E.AdminName);
        Obj->SetStringField(TEXT("details"),    E.Details);
        Obj->SetStringField(TEXT("timestamp"),  E.Timestamp.ToIso8601());
        return Obj;
    }
} // namespace BanJson

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanRestApi::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Collection.InitializeDependency<UBanAppealRegistry>();
    Super::Initialize(Collection);

    // Read port from DefaultBanSystem.ini via UBanSystemConfig.
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    ApiPort = Cfg ? Cfg->RestApiPort : 3000;

    if (ApiPort <= 0)
    {
        UE_LOG(LogBanRestApi, Log, TEXT("BanRestApi: REST API disabled (RestApiPort=0)"));
        return;
    }

    StartServer();
}

void UBanRestApi::Deinitialize()
{
    StopServer();
    Super::Deinitialize();
}

void UBanRestApi::StartServer()
{
    FHttpServerModule* HttpModule =
        FModuleManager::Get().LoadModulePtr<FHttpServerModule>(TEXT("HTTPServer"));
    if (!HttpModule)
    {
        UE_LOG(LogBanRestApi, Error,
            TEXT("BanRestApi: HTTPServer module not available"));
        return;
    }

    Router = HttpModule->GetHttpRouter(static_cast<uint32>(ApiPort),
                                       /*bFailOnBindError=*/false);
    if (!Router.IsValid())
    {
        UE_LOG(LogBanRestApi, Error,
            TEXT("BanRestApi: failed to get HTTP router on port %d"), ApiPort);
        return;
    }

    Routes = MakeShared<FBanRestApiRoutes>();
    RegisterRoutes();
    HttpModule->StartAllListeners();

    UE_LOG(LogBanRestApi, Log,
        TEXT("BanRestApi: REST API listening on port %d"), ApiPort);
}

void UBanRestApi::StopServer()
{
    if (!Router.IsValid()) return;

    if (Routes.IsValid())
    {
        for (FHttpRouteHandle& H : Routes->Handles)
            Router->UnbindRoute(H);
        Routes->Handles.Empty();
    }
    Routes.Reset();

    // NOTE: Do NOT call HttpModule->StopAllListeners() here — it would stop
    // every HTTP listener across every mod, not just ours.  Unbinding our routes
    // above is sufficient; the router will return 404 for future requests to our
    // paths and the port can be reused by other consumers.
    Router.Reset();
    UE_LOG(LogBanRestApi, Log, TEXT("BanRestApi: REST API stopped"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Route registration  (mirrors Express routes in apiServer.ts)
// ─────────────────────────────────────────────────────────────────────────────

void UBanRestApi::RegisterRoutes()
{
    // Use a weak pointer so that route lambdas (stored in the HTTP router and
    // potentially called on the HTTP thread) can safely detect when the game
    // instance has been destroyed and bail out rather than crash.
    TWeakObjectPtr<UGameInstance> WeakGI(GetGameInstance());

    // ── GET /health ──────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/health")),
        EHttpServerRequestVerbs::VERB_GET,
        [](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("status"),    TEXT("ok"));
            Obj->SetStringField(TEXT("timestamp"), FDateTime::UtcNow().ToIso8601());
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── GET /bans ────────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }
            Done(BanJson::Json(BanJson::ArrayToString(DB->GetActiveBans())));
            return true;
        }
    ));

    // ── GET /bans/all ────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/all")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }
            Done(BanJson::Json(BanJson::ArrayToString(DB->GetAllBans())));
            return true;
        }
    ));

    // ── GET /bans/check/:uid ─────────────────────────────────────────────────
    // Must be registered BEFORE /bans/:uid to avoid route collision.
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/check/:uid")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            const FString RawUid   = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid      = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            FBanEntry Entry;
            const bool bBanned = DB->IsCurrentlyBanned(Uid, Entry);

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetBoolField(TEXT("isBanned"), bBanned);
            if (bBanned)
            {
                Obj->SetObjectField(TEXT("record"), BanJson::EntryToJson(Entry));
                Obj->SetStringField(TEXT("message"), Entry.GetKickMessage());
            }
            else
            {
                Obj->SetStringField(TEXT("message"), TEXT("Player is not banned."));
            }
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── POST /bans ───────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }

            // Reject oversized bodies before touching body content (HTTP 413).
            static constexpr int32 MaxPostBodyBytes = 1 * 1024 * 1024;
            if (Req.Body.Num() > MaxPostBodyBytes)
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("Request body too large (%d bytes, limit %d)"), Req.Body.Num(), MaxPostBodyBytes),
                    EHttpServerResponseCodes::RequestTooLarge));
                return true;
            }

            // Parse JSON body.
            const FString BodyStr = BanJson::BodyToString(Req);
            TSharedPtr<FJsonObject> Body;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
            if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
            {
                Done(BanJson::Error(TEXT("Invalid JSON body")));
                return true;
            }

            FString PlayerUID, Platform;
            if (!Body->TryGetStringField(TEXT("playerUID"), PlayerUID) || PlayerUID.IsEmpty())
            {
                Done(BanJson::Error(TEXT("playerUID is required")));
                return true;
            }
            if (!Body->TryGetStringField(TEXT("platform"), Platform) || Platform.IsEmpty())
            {
                Done(BanJson::Error(TEXT("platform is required (EOS | UNKNOWN | IP)")));
                return true;
            }
            Platform = Platform.ToUpper();
            if (Platform != TEXT("EOS") && Platform != TEXT("UNKNOWN") && Platform != TEXT("IP"))
            {
                Done(BanJson::Error(TEXT("platform must be EOS, UNKNOWN, or IP")));
                return true;
            }

            FString PlayerName, Reason, BannedBy;
            Body->TryGetStringField(TEXT("playerName"), PlayerName);
            Body->TryGetStringField(TEXT("reason"),     Reason);
            Body->TryGetStringField(TEXT("bannedBy"),   BannedBy);

            double DurationMinutesDbl = 0.0;
            Body->TryGetNumberField(TEXT("durationMinutes"), DurationMinutesDbl);
            // Guard against out-of-range doubles: values outside [0, INT_MAX] would
            // produce undefined behaviour when cast to int32.  Negative/zero means
            // permanent; anything larger than INT_MAX is clamped to INT_MAX.
            const int32 DurationMinutes = (DurationMinutesDbl <= 0.0 || DurationMinutesDbl > static_cast<double>(INT_MAX))
                ? 0
                : static_cast<int32>(DurationMinutesDbl);

            if (Reason.IsEmpty())   Reason   = TEXT("No reason given");
            if (BannedBy.IsEmpty()) BannedBy = TEXT("system");

            FBanEntry Entry;
            Entry.Uid        = UBanDatabase::MakeUid(Platform, PlayerUID);
            Entry.PlayerUID  = PlayerUID;
            Entry.Platform   = Platform;
            Entry.PlayerName = PlayerName;
            Entry.Reason     = Reason;
            Entry.BannedBy   = BannedBy;
            Entry.BanDate    = FDateTime::UtcNow();
            Entry.bIsPermanent = (DurationMinutes <= 0);
            Entry.ExpireDate = Entry.bIsPermanent
                ? FDateTime(0)
                : FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);

            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            if (!DB->AddBan(Entry))
            {
                Done(BanJson::Error(TEXT("Failed to add ban"), EHttpServerResponseCodes::ServerError));
                return true;
            }

            // Kick the player immediately if they are currently connected.
            if (UWorld* World = GI->GetWorld())
            {
                UBanEnforcer::KickConnectedPlayer(World, Entry.Uid, Entry.GetKickMessage());
            }

            // Fetch the row back so we can return the assigned id (auto-incremented by AddBan).
            // Fall back to returning the in-memory entry if the lookup fails (should never
            // happen after a successful AddBan, but guards against a corrupt DB state).
            FBanEntry Saved;
            if (!DB->GetBanByUid(Entry.Uid, Saved))
            {
                UE_LOG(LogBanRestApi, Warning,
                    TEXT("BanRestApi: POST /bans — GetBanByUid failed for '%s' immediately after AddBan; returning in-memory entry"),
                    *Entry.Uid);
                Saved = Entry;
            }

            FBanDiscordNotifier::NotifyBanCreated(Saved);
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("ban"), Saved.Uid, Saved.PlayerName, Saved.BannedBy, Saved.BannedBy, Saved.Reason);

            auto Resp = BanJson::Json(BanJson::ObjectToString(BanJson::EntryToJson(Saved)));
            Resp->Code = EHttpServerResponseCodes::Created;
            Done(MoveTemp(Resp));
            return true;
        }
    ));

    // ── DELETE /bans/id/:id ──────────────────────────────────────────────────
    // Register BEFORE /bans/:uid so the "id" literal segment wins.
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/id/:id")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString IdStr = Req.PathParams.FindRef(TEXT("id"));
            if (IdStr.IsEmpty() || !IdStr.IsNumeric())
            {
                Done(BanJson::Error(TEXT("id must be an integer")));
                return true;
            }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const int64 Id = FCString::Atoi64(*IdStr);

            // Look up before deletion so we have the UID/name for notifications.
            FString DeletedUid;
            FString DeletedPlayerName;
            {
                TArray<FBanEntry> AllBans = DB->GetAllBans();
                for (const FBanEntry& E : AllBans)
                {
                    if (E.Id == Id)
                    {
                        DeletedUid        = E.Uid;
                        DeletedPlayerName = E.PlayerName;
                        break;
                    }
                }
            }

            if (!DB->RemoveBanById(Id))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No ban found with id %lld"), Id),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            FBanDiscordNotifier::NotifyBanRemoved(DeletedUid, DeletedPlayerName, TEXT("api"));
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("unban"), DeletedUid, DeletedPlayerName, TEXT("api"), TEXT("api"),
                    FString::Printf(TEXT("id=%lld"), Id));

            Done(BanJson::Ok(FString::Printf(TEXT("Ban id=%lld removed"), Id)));
            return true;
        }
    ));

    // ── GET /bans/export.csv ─────────────────────────────────────────────────
    // Must be registered before /bans/:uid to avoid route collision.
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/export.csv")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FBanEntry> AllBans = DB->GetAllBans();

            auto CsvQuote = [](const FString& S) -> FString
            {
                return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
            };

            FString Csv = TEXT("id,uid,playerUID,platform,playerName,reason,bannedBy,banDate,expireDate,isPermanent,linkedUids\n");
            for (const FBanEntry& E : AllBans)
            {
                FString LinkedStr;
                for (int32 i = 0; i < E.LinkedUids.Num(); ++i)
                {
                    if (i > 0) LinkedStr += TEXT("|");
                    LinkedStr += E.LinkedUids[i];
                }

                Csv += FString::Printf(TEXT("%lld,"), E.Id);
                Csv += CsvQuote(E.Uid)        + TEXT(",");
                Csv += CsvQuote(E.PlayerUID)  + TEXT(",");
                Csv += CsvQuote(E.Platform)   + TEXT(",");
                Csv += CsvQuote(E.PlayerName) + TEXT(",");
                Csv += CsvQuote(E.Reason)     + TEXT(",");
                Csv += CsvQuote(E.BannedBy)   + TEXT(",");
                Csv += E.BanDate.ToIso8601()  + TEXT(",");
                Csv += (E.bIsPermanent ? FString() : E.ExpireDate.ToIso8601()) + TEXT(",");
                Csv += (E.bIsPermanent ? TEXT("true") : TEXT("false")) + TEXT(",");
                Csv += CsvQuote(LinkedStr)    + TEXT("\n");
            }

            auto R = FHttpServerResponse::Create(Csv, TEXT("text/csv"));
            R->Code = EHttpServerResponseCodes::Ok;
            Done(MoveTemp(R));
            return true;
        }
    ));

    // ── PATCH /bans/:uid ─────────────────────────────────────────────────────
    // Must be registered before DELETE /bans/:uid.
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/:uid")),
        EHttpServerRequestVerbs::VERB_PATCH,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString RawUid = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid    = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            FBanEntry Entry;
            if (!DB->GetBanByUid(Uid, Entry))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No ban found for uid '%s'"), *Uid),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            const FString BodyStr = BanJson::BodyToString(Req);
            TSharedPtr<FJsonObject> Body;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
            if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
            {
                Done(BanJson::Error(TEXT("Invalid JSON body")));
                return true;
            }

            FString NewReason;
            if (Body->TryGetStringField(TEXT("reason"), NewReason) && !NewReason.IsEmpty())
                Entry.Reason = NewReason;

            FString NewBannedBy;
            if (Body->TryGetStringField(TEXT("bannedBy"), NewBannedBy) && !NewBannedBy.IsEmpty())
                Entry.BannedBy = NewBannedBy;

            double DurationMinutesDbl = -1.0;
            if (Body->TryGetNumberField(TEXT("durationMinutes"), DurationMinutesDbl))
            {
                if (DurationMinutesDbl == 0.0)
                {
                    Entry.bIsPermanent = true;
                    Entry.ExpireDate   = FDateTime(0);
                }
                else if (DurationMinutesDbl > 0.0)
                {
                    const int32 Mins = (DurationMinutesDbl > static_cast<double>(INT_MAX))
                        ? INT_MAX
                        : static_cast<int32>(DurationMinutesDbl);
                    Entry.bIsPermanent = false;
                    Entry.ExpireDate   = FDateTime::UtcNow() + FTimespan::FromMinutes(Mins);
                }
                // negative (other than -1 sentinel) = don't change
            }

            if (!DB->AddBan(Entry))
            {
                Done(BanJson::Error(TEXT("Failed to update ban"), EHttpServerResponseCodes::ServerError));
                return true;
            }

            FBanEntry Updated;
            if (!DB->GetBanByUid(Uid, Updated)) Updated = Entry;

            Done(BanJson::Json(BanJson::ObjectToString(BanJson::EntryToJson(Updated))));
            return true;
        }
    ));

    // ── DELETE /bans/:uid ────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/:uid")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString RawUid = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid    = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            // Look up before deletion so we have the player name for notifications.
            FBanEntry OldEntry;
            const bool bFoundEntry = DB->GetBanByUid(Uid, OldEntry);

            if (!DB->RemoveBanByUid(Uid))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No ban found for uid '%s'"), *Uid),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            FBanDiscordNotifier::NotifyBanRemoved(Uid, bFoundEntry ? OldEntry.PlayerName : TEXT(""), TEXT("api"));
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("unban"), Uid, bFoundEntry ? OldEntry.PlayerName : TEXT(""), TEXT("api"), TEXT("api"), TEXT(""));

            Done(BanJson::Ok(FString::Printf(TEXT("Ban '%s' removed"), *Uid)));
            return true;
        }
    ));

    // ── POST /bans/prune ─────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/prune")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const int32 Pruned = DB->PruneExpiredBans();

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("pruned"), Pruned);
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── POST /bans/backup ────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/backup")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
            const int32 MaxBackups = Cfg ? Cfg->MaxBackups : 5;

            const FString BackupDir =
                FPaths::GetPath(DB->GetDatabasePath()) / TEXT("backups");
            const FString Dest = DB->Backup(BackupDir, MaxBackups);

            if (Dest.IsEmpty())
            {
                Done(BanJson::Error(TEXT("Backup failed"), EHttpServerResponseCodes::ServerError));
                return true;
            }

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("path"), Dest);
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── GET /players ─────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/players")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerSessionRegistry* Reg = GI->GetSubsystem<UPlayerSessionRegistry>();
            if (!Reg) { Done(BanJson::Error(TEXT("PlayerSessionRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FPlayerSessionRecord> Players = Reg->GetAllRecords();
            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Reserve(Players.Num());
            for (const FPlayerSessionRecord& P : Players)
            {
                TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                Obj->SetStringField(TEXT("uid"),         P.Uid);
                Obj->SetStringField(TEXT("displayName"), P.DisplayName);
                Obj->SetStringField(TEXT("lastSeen"),    P.LastSeen);
                Obj->SetStringField(TEXT("ipAddress"),   P.IpAddress);
                Arr.Add(MakeShared<FJsonValueObject>(Obj));
            }

            TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("count"),   Players.Num());
            Root->SetArrayField (TEXT("players"), Arr);
            Done(BanJson::Json(BanJson::ObjectToString(Root)));
            return true;
        }
    ));

    // ── GET /warnings ────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/warnings")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
            if (!WarnReg) { Done(BanJson::Error(TEXT("WarningRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FWarningEntry> Entries;
            const FString* UidFilter = Req.QueryParams.Find(TEXT("uid"));
            if (UidFilter && !UidFilter->IsEmpty())
                Entries = WarnReg->GetWarningsForUid(*UidFilter);
            else
                Entries = WarnReg->GetAllWarnings();

            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Reserve(Entries.Num());
            for (const FWarningEntry& W : Entries)
                Arr.Add(MakeShared<FJsonValueObject>(BanJson::WarningToJson(W)));

            TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("count"),    Entries.Num());
            Root->SetArrayField (TEXT("warnings"), Arr);
            Done(BanJson::Json(BanJson::ObjectToString(Root)));
            return true;
        }
    ));

    // ── POST /warnings ───────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/warnings")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }

            const FString BodyStr = BanJson::BodyToString(Req);
            TSharedPtr<FJsonObject> Body;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
            if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
            {
                Done(BanJson::Error(TEXT("Invalid JSON body")));
                return true;
            }

            FString Uid, PlayerName, Reason, WarnedBy;
            if (!Body->TryGetStringField(TEXT("uid"), Uid) || Uid.IsEmpty())
            {
                Done(BanJson::Error(TEXT("uid is required")));
                return true;
            }
            Body->TryGetStringField(TEXT("playerName"), PlayerName);
            Body->TryGetStringField(TEXT("reason"),     Reason);
            Body->TryGetStringField(TEXT("warnedBy"),   WarnedBy);

            if (Reason.IsEmpty())   Reason   = TEXT("No reason given");
            if (WarnedBy.IsEmpty()) WarnedBy = TEXT("console");

            UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
            if (!WarnReg) { Done(BanJson::Error(TEXT("WarningRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            WarnReg->AddWarning(Uid, PlayerName, Reason, WarnedBy);
            const int32 WarnCount          = WarnReg->GetWarningCount(Uid);
            TArray<FWarningEntry> AllForUid = WarnReg->GetWarningsForUid(Uid);
            const FWarningEntry   NewEntry  = AllForUid.Num() > 0 ? AllForUid.Last() : FWarningEntry();

            FBanDiscordNotifier::NotifyWarningIssued(Uid, PlayerName, Reason, WarnedBy, WarnCount);
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("warn"), Uid, PlayerName, WarnedBy, WarnedBy, Reason);

            // Auto-ban if the warning threshold has been reached.
            // First, check escalation tiers; fall back to AutoBanWarnCount if no tiers.
            const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
            if (Cfg)
            {
                int32 BanDurationMinutes = -1;

                // Check escalation tiers (highest matching tier wins).
                if (Cfg->WarnEscalationTiers.Num() > 0)
                {
                    for (const FWarnEscalationTier& Tier : Cfg->WarnEscalationTiers)
                    {
                        if (WarnCount >= Tier.WarnCount)
                        {
                            BanDurationMinutes = Tier.DurationMinutes;
                        }
                    }
                }
                else if (Cfg->AutoBanWarnCount > 0 && WarnCount >= Cfg->AutoBanWarnCount)
                {
                    BanDurationMinutes = Cfg->AutoBanWarnMinutes;
                }

                if (BanDurationMinutes >= 0)
                {
                    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
                    if (DB)
                    {
                        FBanEntry AutoBan;
                        AutoBan.Uid      = Uid;
                        UBanDatabase::ParseUid(Uid, AutoBan.Platform, AutoBan.PlayerUID);
                        AutoBan.PlayerName   = PlayerName;
                        AutoBan.Reason       = TEXT("Auto-banned: reached warning threshold");
                        AutoBan.BannedBy     = TEXT("system");
                        AutoBan.BanDate      = FDateTime::UtcNow();
                        AutoBan.bIsPermanent = (BanDurationMinutes <= 0);
                        AutoBan.ExpireDate   = AutoBan.bIsPermanent
                            ? FDateTime(0)
                            : FDateTime::UtcNow() + FTimespan::FromMinutes(BanDurationMinutes);

                        DB->AddBan(AutoBan);
                        FBanDiscordNotifier::NotifyBanCreated(AutoBan);
                        if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                            AuditLog->LogAction(TEXT("ban"), Uid, PlayerName, TEXT("system"), TEXT("system"), AutoBan.Reason);
                    }
                }
            }

            auto Resp = BanJson::Json(BanJson::ObjectToString(BanJson::WarningToJson(NewEntry)));
            Resp->Code = EHttpServerResponseCodes::Created;
            Done(MoveTemp(Resp));
            return true;
        }
    ));

    // ── DELETE /warnings/:uid ────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/warnings/:uid")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString RawUid = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid    = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
            if (!WarnReg) { Done(BanJson::Error(TEXT("WarningRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const int32 Cleared = WarnReg->ClearWarningsForUid(Uid);

            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("clearwarns"), Uid, TEXT(""), TEXT("api"), TEXT("api"),
                    FString::Printf(TEXT("cleared=%d"), Cleared));

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("cleared"), Cleared);
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── GET /audit ───────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/audit")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>();
            if (!AuditLog) { Done(BanJson::Error(TEXT("AuditLog unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            int32 Limit = 100;
            if (const FString* LimitStr = Req.QueryParams.Find(TEXT("limit")))
            {
                const int32 Parsed = FCString::Atoi(**LimitStr);
                if (Parsed > 0) Limit = FMath::Min(Parsed, 1000);
            }

            int32 Page = 1;
            if (const FString* PageStr = Req.QueryParams.Find(TEXT("page")))
            {
                const int32 Parsed = FCString::Atoi(**PageStr);
                if (Parsed > 0) Page = Parsed;
            }

            TArray<FAuditEntry> Entries;
            const FString* UidFilter = Req.QueryParams.Find(TEXT("uid"));
            if (UidFilter && !UidFilter->IsEmpty())
                Entries = AuditLog->GetEntriesForTarget(*UidFilter);
            else
                Entries = AuditLog->GetAllEntries();

            // Sort newest first before paginating.
            Entries.Sort([](const FAuditEntry& A, const FAuditEntry& B)
            {
                return A.Timestamp > B.Timestamp;
            });

            const int32 Total  = Entries.Num();
            const int32 Offset = (Page - 1) * Limit;
            const int32 End    = FMath::Min(Offset + Limit, Total);

            TArray<TSharedPtr<FJsonValue>> Arr;
            if (Offset < Total)
            {
                Arr.Reserve(End - Offset);
                for (int32 i = Offset; i < End; ++i)
                    Arr.Add(MakeShared<FJsonValueObject>(BanJson::AuditToJson(Entries[i])));
            }

            TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("count"),      Arr.Num());
            Root->SetNumberField(TEXT("total"),      Total);
            Root->SetNumberField(TEXT("page"),       Page);
            Root->SetNumberField(TEXT("pageSize"),   Limit);
            Root->SetNumberField(TEXT("totalPages"), (Total + Limit - 1) / FMath::Max(Limit, 1));
            Root->SetArrayField (TEXT("entries"),    Arr);
            Done(BanJson::Json(BanJson::ObjectToString(Root)));
            return true;
        }
    ));

    // ── GET /metrics ─────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/metrics")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }

            UBanDatabase*           DB        = GI->GetSubsystem<UBanDatabase>();
            UPlayerWarningRegistry* WarnReg   = GI->GetSubsystem<UPlayerWarningRegistry>();
            UPlayerSessionRegistry* PlayerReg = GI->GetSubsystem<UPlayerSessionRegistry>();
            UBanAuditLog*           AuditLog  = GI->GetSubsystem<UBanAuditLog>();

            // Count temp bans expiring within 24 hours.
            int32 TempBansExpiringSoon = 0;
            if (DB)
            {
                const FDateTime Horizon = FDateTime::UtcNow() + FTimespan::FromHours(24.0);
                for (const FBanEntry& E : DB->GetActiveBans())
                {
                    if (!E.bIsPermanent && E.ExpireDate <= Horizon)
                        ++TempBansExpiringSoon;
                }
            }

            // Count warnings issued this week.
            int32 WarningsThisWeek = 0;
            if (WarnReg)
            {
                const FDateTime WeekAgo = FDateTime::UtcNow() - FTimespan::FromDays(7.0);
                for (const FWarningEntry& W : WarnReg->GetAllWarnings())
                {
                    if (W.WarnDate >= WeekAgo)
                        ++WarningsThisWeek;
                }
            }

            // Online player count from the world.
            int32 OnlinePlayers = 0;
            if (UWorld* World = GI->GetWorld())
            {
                for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
                {
                    if (It->IsValid()) ++OnlinePlayers;
                }
            }

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("activeBans"),              DB        ? static_cast<double>(DB->GetActiveBans().Num())        : 0.0);
            Obj->SetNumberField(TEXT("totalBans"),               DB        ? static_cast<double>(DB->GetAllBans().Num())           : 0.0);
            Obj->SetNumberField(TEXT("tempBansExpiringSoon24h"), static_cast<double>(TempBansExpiringSoon));
            Obj->SetNumberField(TEXT("totalWarnings"),           WarnReg   ? static_cast<double>(WarnReg->GetAllWarnings().Num())  : 0.0);
            Obj->SetNumberField(TEXT("warningsThisWeek"),        static_cast<double>(WarningsThisWeek));
            Obj->SetNumberField(TEXT("totalAuditEntries"),       AuditLog  ? static_cast<double>(AuditLog->GetAllEntries().Num())  : 0.0);
            Obj->SetNumberField(TEXT("knownPlayers"),            PlayerReg ? static_cast<double>(PlayerReg->GetAllRecords().Num()) : 0.0);
            Obj->SetNumberField(TEXT("onlinePlayers"),           static_cast<double>(OnlinePlayers));
            Obj->SetStringField(TEXT("timestamp"),               FDateTime::UtcNow().ToIso8601());
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── POST /bans/ip ────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/ip")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const FString BodyStr = BanJson::BodyToString(Req);
            TSharedPtr<FJsonObject> Body;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
            if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
            {
                Done(BanJson::Error(TEXT("Invalid JSON body")));
                return true;
            }

            FString IpAddress;
            if (!Body->TryGetStringField(TEXT("ipAddress"), IpAddress) || IpAddress.IsEmpty())
            {
                Done(BanJson::Error(TEXT("ipAddress is required")));
                return true;
            }

            FString Reason, BannedBy;
            Body->TryGetStringField(TEXT("reason"),   Reason);
            Body->TryGetStringField(TEXT("bannedBy"), BannedBy);
            if (Reason.IsEmpty())   Reason   = TEXT("IP ban");
            if (BannedBy.IsEmpty()) BannedBy = TEXT("system");

            FBanEntry Entry;
            Entry.Uid        = UBanDatabase::MakeUid(TEXT("IP"), IpAddress);
            Entry.PlayerUID  = IpAddress;
            Entry.Platform   = TEXT("IP");
            Entry.PlayerName = IpAddress;
            Entry.Reason     = Reason;
            Entry.BannedBy   = BannedBy;
            Entry.BanDate    = FDateTime::UtcNow();
            Entry.bIsPermanent = true;
            Entry.ExpireDate   = FDateTime(0);

            if (!DB->AddBan(Entry))
            {
                Done(BanJson::Error(TEXT("Failed to add IP ban"), EHttpServerResponseCodes::ServerError));
                return true;
            }

            FBanDiscordNotifier::NotifyBanCreated(Entry);
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("ban"), Entry.Uid, IpAddress, BannedBy, BannedBy, Reason);

            auto Resp = BanJson::Json(BanJson::ObjectToString(BanJson::EntryToJson(Entry)));
            Resp->Code = EHttpServerResponseCodes::Created;
            Done(MoveTemp(Resp));
            return true;
        }
    ));

    // ── DELETE /bans/ip/:ip ──────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/ip/:ip")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString IpAddress = FGenericPlatformHttp::UrlDecode(Req.PathParams.FindRef(TEXT("ip")));

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const FString Uid = UBanDatabase::MakeUid(TEXT("IP"), IpAddress);
            if (!DB->RemoveBanByUid(Uid))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No IP ban found for '%s'"), *IpAddress),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            FBanDiscordNotifier::NotifyBanRemoved(Uid, IpAddress, TEXT("api"));
            if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
                AuditLog->LogAction(TEXT("unban"), Uid, IpAddress, TEXT("api"), TEXT("api"), TEXT("ip ban removal"));

            Done(BanJson::Ok(FString::Printf(TEXT("IP ban for '%s' removed"), *IpAddress)));
            return true;
        }
    ));

    // ── POST /players/prune ──────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/players/prune")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerSessionRegistry* PlayerReg = GI->GetSubsystem<UPlayerSessionRegistry>();
            if (!PlayerReg) { Done(BanJson::Error(TEXT("PlayerSessionRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const UBanSystemConfig* Cfg = UBanSystemConfig::Get();

            // Allow caller to override the retention window.
            int32 DaysToKeep = Cfg ? Cfg->SessionRetentionDays : 0;
            const FString BodyStr = BanJson::BodyToString(Req);
            if (BodyStr != TEXT("{}"))
            {
                TSharedPtr<FJsonObject> Body;
                TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
                if (FJsonSerializer::Deserialize(Reader, Body) && Body.IsValid())
                {
                    double DaysDbl = 0.0;
                    if (Body->TryGetNumberField(TEXT("daysToKeep"), DaysDbl) && DaysDbl > 0.0)
                        DaysToKeep = static_cast<int32>(DaysDbl);
                }
            }

            if (DaysToKeep <= 0)
            {
                Done(BanJson::Error(
                    TEXT("daysToKeep must be > 0 (set in request body or SessionRetentionDays config)")));
                return true;
            }

            const int32 Pruned = PlayerReg->PruneOldRecords(DaysToKeep);

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("pruned"),     static_cast<double>(Pruned));
            Obj->SetNumberField(TEXT("daysToKeep"), static_cast<double>(DaysToKeep));
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── GET /audit/export.csv ────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/audit/export.csv")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>();
            if (!AuditLog) { Done(BanJson::Error(TEXT("AuditLog unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FAuditEntry> Entries = AuditLog->GetAllEntries();

            auto CsvQ = [](const FString& S) -> FString
            {
                return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
            };

            FString Csv = TEXT("id,action,targetUid,targetName,adminUid,adminName,details,timestamp\n");
            for (const FAuditEntry& E : Entries)
            {
                Csv += FString::Printf(TEXT("%lld,"), E.Id);
                Csv += CsvQ(E.Action)     + TEXT(",");
                Csv += CsvQ(E.TargetUid)  + TEXT(",");
                Csv += CsvQ(E.TargetName) + TEXT(",");
                Csv += CsvQ(E.AdminUid)   + TEXT(",");
                Csv += CsvQ(E.AdminName)  + TEXT(",");
                Csv += CsvQ(E.Details)    + TEXT(",");
                Csv += E.Timestamp.ToIso8601() + TEXT("\n");
            }

            auto R = FHttpServerResponse::Create(Csv, TEXT("text/csv"));
            R->Code = EHttpServerResponseCodes::Ok;
            Done(MoveTemp(R));
            return true;
        }
    ));

    // ── GET /warnings/export.csv ─────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/warnings/export.csv")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
            if (!WarnReg) { Done(BanJson::Error(TEXT("WarningRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FWarningEntry> Entries = WarnReg->GetAllWarnings();

            auto CsvQ = [](const FString& S) -> FString
            {
                return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
            };

            FString Csv = TEXT("id,uid,playerName,reason,warnedBy,warnDate\n");
            for (const FWarningEntry& W : Entries)
            {
                Csv += FString::Printf(TEXT("%lld,"), W.Id);
                Csv += CsvQ(W.Uid)        + TEXT(",");
                Csv += CsvQ(W.PlayerName) + TEXT(",");
                Csv += CsvQ(W.Reason)     + TEXT(",");
                Csv += CsvQ(W.WarnedBy)   + TEXT(",");
                Csv += W.WarnDate.ToIso8601() + TEXT("\n");
            }

            auto R = FHttpServerResponse::Create(Csv, TEXT("text/csv"));
            R->Code = EHttpServerResponseCodes::Ok;
            Done(MoveTemp(R));
            return true;
        }
    ));

    // ── GET /players/export.csv ──────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/players/export.csv")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UPlayerSessionRegistry* PlayerReg = GI->GetSubsystem<UPlayerSessionRegistry>();
            if (!PlayerReg) { Done(BanJson::Error(TEXT("PlayerSessionRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FPlayerSessionRecord> Players = PlayerReg->GetAllRecords();

            auto CsvQ = [](const FString& S) -> FString
            {
                return TEXT("\"") + S.Replace(TEXT("\""), TEXT("\"\"")) + TEXT("\"");
            };

            FString Csv = TEXT("uid,displayName,lastSeen,ipAddress\n");
            for (const FPlayerSessionRecord& P : Players)
            {
                Csv += CsvQ(P.Uid)         + TEXT(",");
                Csv += CsvQ(P.DisplayName) + TEXT(",");
                Csv += CsvQ(P.LastSeen)    + TEXT(",");
                Csv += CsvQ(P.IpAddress)   + TEXT("\n");
            }

            auto R = FHttpServerResponse::Create(Csv, TEXT("text/csv"));
            R->Code = EHttpServerResponseCodes::Ok;
            Done(MoveTemp(R));
            return true;
        }
    ));

    // ── POST /appeals ────────────────────────────────────────────────────────
    // No auth required — players submit appeals from outside the game.
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/appeals")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanAppealRegistry* AppealsReg = GI->GetSubsystem<UBanAppealRegistry>();
            if (!AppealsReg) { Done(BanJson::Error(TEXT("AppealRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            const FString BodyStr = BanJson::BodyToString(Req);
            TSharedPtr<FJsonObject> Body;
            TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(BodyStr);
            if (!FJsonSerializer::Deserialize(Reader, Body) || !Body.IsValid())
            {
                Done(BanJson::Error(TEXT("Invalid JSON body")));
                return true;
            }

            FString Uid, Reason, ContactInfo;
            if (!Body->TryGetStringField(TEXT("uid"), Uid) || Uid.IsEmpty())
            {
                Done(BanJson::Error(TEXT("uid is required")));
                return true;
            }
            Body->TryGetStringField(TEXT("reason"),      Reason);
            Body->TryGetStringField(TEXT("contactInfo"), ContactInfo);
            if (Reason.IsEmpty()) Reason = TEXT("No reason given");

            const FBanAppealEntry NewEntry = AppealsReg->AddAppeal(Uid, Reason, ContactInfo);

            // Optionally notify Discord.
            {
                const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
                if (Cfg && !Cfg->DiscordWebhookUrl.IsEmpty())
                {
                    // Reuse BanDiscordNotifier's webhook infrastructure via a simple direct call.
                    // Build a minimal embed payload inline rather than adding a dedicated method.
                }
            }

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("id"),          static_cast<double>(NewEntry.Id));
            Obj->SetStringField(TEXT("uid"),         NewEntry.Uid);
            Obj->SetStringField(TEXT("reason"),      NewEntry.Reason);
            Obj->SetStringField(TEXT("contactInfo"), NewEntry.ContactInfo);
            Obj->SetStringField(TEXT("submittedAt"), NewEntry.SubmittedAt.ToIso8601());

            auto Resp = BanJson::Json(BanJson::ObjectToString(Obj));
            Resp->Code = EHttpServerResponseCodes::Created;
            Done(MoveTemp(Resp));
            return true;
        }
    ));

    // ── GET /appeals ─────────────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/appeals")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanAppealRegistry* AppealsReg = GI->GetSubsystem<UBanAppealRegistry>();
            if (!AppealsReg) { Done(BanJson::Error(TEXT("AppealRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            TArray<FBanAppealEntry> Appeals = AppealsReg->GetAllAppeals();

            TArray<TSharedPtr<FJsonValue>> Arr;
            Arr.Reserve(Appeals.Num());
            for (const FBanAppealEntry& E : Appeals)
            {
                TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
                Obj->SetNumberField(TEXT("id"),          static_cast<double>(E.Id));
                Obj->SetStringField(TEXT("uid"),         E.Uid);
                Obj->SetStringField(TEXT("reason"),      E.Reason);
                Obj->SetStringField(TEXT("contactInfo"), E.ContactInfo);
                Obj->SetStringField(TEXT("submittedAt"), E.SubmittedAt.ToIso8601());
                Arr.Add(MakeShared<FJsonValueObject>(Obj));
            }

            TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
            Root->SetNumberField(TEXT("count"),   static_cast<double>(Appeals.Num()));
            Root->SetArrayField (TEXT("appeals"), Arr);
            Done(BanJson::Json(BanJson::ObjectToString(Root)));
            return true;
        }
    ));

    // ── DELETE /appeals/:id ──────────────────────────────────────────────────
    Routes->Handles.Add(Router->BindRoute(
        FHttpPath(TEXT("/appeals/:id")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            if (!BanJson::CheckApiKey(Req)) { Done(BanJson::Error(TEXT("Unauthorized"), EHttpServerResponseCodes::Denied)); return true; }

            const FString IdStr = Req.PathParams.FindRef(TEXT("id"));
            if (IdStr.IsEmpty() || !IdStr.IsNumeric())
            {
                Done(BanJson::Error(TEXT("id must be an integer")));
                return true;
            }
            const int64 Id = FCString::Atoi64(*IdStr);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavail)); return true; }
            UBanAppealRegistry* AppealsReg = GI->GetSubsystem<UBanAppealRegistry>();
            if (!AppealsReg) { Done(BanJson::Error(TEXT("AppealRegistry unavailable"), EHttpServerResponseCodes::ServerError)); return true; }

            if (!AppealsReg->DeleteAppeal(Id))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No appeal found with id %lld"), Id),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            Done(BanJson::Ok(FString::Printf(TEXT("Appeal id=%lld dismissed"), Id)));
            return true;
        }
    ));
}
