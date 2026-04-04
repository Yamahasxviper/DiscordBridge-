// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/apiServer.ts

#include "BanRestApi.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanSystemConfig.h"

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
} // namespace BanJson

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanRestApi::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
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

    RegisterRoutes();
    HttpModule->StartAllListeners();

    UE_LOG(LogBanRestApi, Log,
        TEXT("BanRestApi: REST API listening on port %d"), ApiPort);
}

void UBanRestApi::StopServer()
{
    if (!Router.IsValid()) return;

    for (FHttpRouteHandle& H : RouteHandles)
        Router->UnbindRoute(H);
    RouteHandles.Empty();

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
    RouteHandles.Add(Router->BindRoute(
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
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }
            Done(BanJson::Json(BanJson::ArrayToString(DB->GetActiveBans())));
            return true;
        }
    ));

    // ── GET /bans/all ────────────────────────────────────────────────────────
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/all")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }
            Done(BanJson::Json(BanJson::ArrayToString(DB->GetAllBans())));
            return true;
        }
    ));

    // ── GET /bans/check/:uid ─────────────────────────────────────────────────
    // Must be registered BEFORE /bans/:uid to avoid route collision.
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/check/:uid")),
        EHttpServerRequestVerbs::VERB_GET,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            const FString RawUid   = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid      = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

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
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }

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
                Done(BanJson::Error(TEXT("platform is required (STEAM | EOS | UNKNOWN)")));
                return true;
            }
            Platform = Platform.ToUpper();
            if (Platform != TEXT("STEAM") && Platform != TEXT("EOS") && Platform != TEXT("UNKNOWN"))
            {
                Done(BanJson::Error(TEXT("platform must be STEAM, EOS, or UNKNOWN")));
                return true;
            }

            FString PlayerName, Reason, BannedBy;
            Body->TryGetStringField(TEXT("playerName"), PlayerName);
            Body->TryGetStringField(TEXT("reason"),     Reason);
            Body->TryGetStringField(TEXT("bannedBy"),   BannedBy);

            double DurationMinutesDbl = 0.0;
            Body->TryGetNumberField(TEXT("durationMinutes"), DurationMinutesDbl);
            const int32 DurationMinutes = static_cast<int32>(DurationMinutesDbl);

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
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

            if (!DB->AddBan(Entry))
            {
                Done(BanJson::Error(TEXT("Failed to add ban"), EHttpServerResponseCodes::InternalServerError));
                return true;
            }

            // Kick the player immediately if they are currently connected.
            if (UWorld* World = GI->GetWorld())
            {
                UBanEnforcer::KickConnectedPlayer(World, Entry.Uid, Entry.GetKickMessage());
            }

            // Fetch the row back so we can return the assigned id.
            FBanEntry Saved;
            DB->GetBanByUid(Entry.Uid, Saved);

            auto Resp = BanJson::Json(BanJson::ObjectToString(BanJson::EntryToJson(Saved)));
            Resp->Code = EHttpServerResponseCodes::Created;
            Done(MoveTemp(Resp));
            return true;
        }
    ));

    // ── DELETE /bans/id/:id ──────────────────────────────────────────────────
    // Register BEFORE /bans/:uid so the "id" literal segment wins.
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/id/:id")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            const FString IdStr = Req.PathParams.FindRef(TEXT("id"));
            if (IdStr.IsEmpty() || !IdStr.IsNumeric())
            {
                Done(BanJson::Error(TEXT("id must be an integer")));
                return true;
            }

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

            const int64 Id = FCString::Atoi64(*IdStr);
            if (!DB->RemoveBanById(Id))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No ban found with id %lld"), Id),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            Done(BanJson::Ok(FString::Printf(TEXT("Ban id=%lld removed"), Id)));
            return true;
        }
    ));

    // ── DELETE /bans/:uid ────────────────────────────────────────────────────
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/:uid")),
        EHttpServerRequestVerbs::VERB_DELETE,
        [WeakGI](const FHttpServerRequest& Req, const FHttpResultCallback& Done) -> bool
        {
            const FString RawUid = Req.PathParams.FindRef(TEXT("uid"));
            const FString Uid    = FGenericPlatformHttp::UrlDecode(RawUid);

            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

            if (!DB->RemoveBanByUid(Uid))
            {
                Done(BanJson::Error(
                    FString::Printf(TEXT("No ban found for uid '%s'"), *Uid),
                    EHttpServerResponseCodes::NotFound));
                return true;
            }

            Done(BanJson::Ok(FString::Printf(TEXT("Ban '%s' removed"), *Uid)));
            return true;
        }
    ));

    // ── POST /bans/prune ─────────────────────────────────────────────────────
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/prune")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

            const int32 Pruned = DB->PruneExpiredBans();

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetNumberField(TEXT("pruned"), Pruned);
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));

    // ── POST /bans/backup ────────────────────────────────────────────────────
    RouteHandles.Add(Router->BindRoute(
        FHttpPath(TEXT("/bans/backup")),
        EHttpServerRequestVerbs::VERB_POST,
        [WeakGI](const FHttpServerRequest&, const FHttpResultCallback& Done) -> bool
        {
            UGameInstance* GI = WeakGI.Get();
            if (!GI) { Done(BanJson::Error(TEXT("Server shutting down"), EHttpServerResponseCodes::ServiceUnavailable)); return true; }
            UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
            if (!DB) { Done(BanJson::Error(TEXT("Database unavailable"), EHttpServerResponseCodes::InternalServerError)); return true; }

            const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
            const int32 MaxBackups = Cfg ? Cfg->MaxBackups : 5;

            const FString BackupDir =
                FPaths::GetPath(DB->GetDatabasePath()) / TEXT("backups");
            const FString Dest = DB->Backup(BackupDir, MaxBackups);

            if (Dest.IsEmpty())
            {
                Done(BanJson::Error(TEXT("Backup failed"), EHttpServerResponseCodes::InternalServerError));
                return true;
            }

            TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
            Obj->SetStringField(TEXT("path"), Dest);
            Done(BanJson::Json(BanJson::ObjectToString(Obj)));
            return true;
        }
    ));
}
