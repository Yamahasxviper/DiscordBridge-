// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordNotifier.h"
#include "BanSystemConfig.h"
#include "BanWebSocketPusher.h"

#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace
{
    /** Escapes a string for inclusion as a JSON string value. */
    FString JsonEscape(const FString& In)
    {
        FString Out;
        Out.Reserve(In.Len() + 8);
        for (TCHAR C : In)
        {
            switch (C)
            {
            case TEXT('"'):  Out += TEXT("\\\""); break;
            case TEXT('\\'): Out += TEXT("\\\\"); break;
            case TEXT('\n'): Out += TEXT("\\n");  break;
            case TEXT('\r'): Out += TEXT("\\r");  break;
            case TEXT('\t'): Out += TEXT("\\t");  break;
            default:         Out += C;            break;
            }
        }
        return Out;
    }

    /** Builds a Discord embed JSON string. */
    FString BuildEmbed(int32 Color, const FString& Title, const FString& FieldsJson)
    {
        return FString::Printf(
            TEXT("{\"embeds\":[{\"title\":\"%s\",\"color\":%d,\"fields\":[%s],\"timestamp\":\"%s\"}]}"),
            *JsonEscape(Title),
            Color,
            *FieldsJson,
            *FDateTime::UtcNow().ToIso8601());
    }

    /** Builds a single inline Discord embed field JSON fragment. */
    FString Field(const FString& Name, const FString& Value, bool bInline = true)
    {
        return FString::Printf(
            TEXT("{\"name\":\"%s\",\"value\":\"%s\",\"inline\":%s}"),
            *JsonEscape(Name),
            *JsonEscape(Value),
            bInline ? TEXT("true") : TEXT("false"));
    }
} // anonymous namespace

// ─────────────────────────────────────────────────────────────────────────────
//  PostWebhook
// ─────────────────────────────────────────────────────────────────────────────

void FBanDiscordNotifier::PostWebhook(const FString& JsonPayload)
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || Cfg->DiscordWebhookUrl.IsEmpty())
        return;

    if (!FModuleManager::Get().IsModuleLoaded(TEXT("HTTP")))
        return;

    TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
        FHttpModule::Get().CreateRequest();

    Request->SetURL(Cfg->DiscordWebhookUrl);
    Request->SetVerb(TEXT("POST"));
    Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
    Request->SetContentAsString(JsonPayload);
    Request->ProcessRequest();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public notification methods
// ─────────────────────────────────────────────────────────────────────────────

void FBanDiscordNotifier::NotifyBanCreated(const FBanEntry& Entry)
{
    const FString PlayerValue = Entry.PlayerName + TEXT(" (") + Entry.Uid + TEXT(")");
    const FString DurationValue = Entry.bIsPermanent
        ? TEXT("Permanent")
        : FString::Printf(TEXT("%lld minutes"),
            (Entry.ExpireDate - Entry.BanDate).GetTotalMinutes());

    const FString Fields =
        Field(TEXT("Player"),    PlayerValue)         + TEXT(",") +
        Field(TEXT("Reason"),    Entry.Reason)         + TEXT(",") +
        Field(TEXT("Duration"),  DurationValue)        + TEXT(",") +
        Field(TEXT("Banned By"), Entry.BannedBy);

    // Red: 15158332
    PostWebhook(BuildEmbed(15158332, TEXT("🔨 Player Banned"), Fields));

    // WebSocket push
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("uid"),        Entry.Uid);
        Obj->SetStringField(TEXT("playerName"), Entry.PlayerName);
        Obj->SetStringField(TEXT("reason"),     Entry.Reason);
        Obj->SetStringField(TEXT("bannedBy"),   Entry.BannedBy);
        Obj->SetBoolField  (TEXT("permanent"),  Entry.bIsPermanent);
        if (!Entry.bIsPermanent)
            Obj->SetStringField(TEXT("expireDate"), Entry.ExpireDate.ToIso8601());
        UBanWebSocketPusher::PushEvent(TEXT("ban"), Obj);
    }
}

void FBanDiscordNotifier::NotifyBanRemoved(const FString& Uid, const FString& PlayerName,
                                           const FString& RemovedBy)
{
    const FString PlayerValue = PlayerName.IsEmpty()
        ? Uid
        : PlayerName + TEXT(" (") + Uid + TEXT(")");

    const FString Fields =
        Field(TEXT("Player"),     PlayerValue) + TEXT(",") +
        Field(TEXT("Removed By"), RemovedBy);

    // Green: 3066993
    PostWebhook(BuildEmbed(3066993, TEXT("✅ Ban Removed"), Fields));

    // WebSocket push
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("uid"),        Uid);
        Obj->SetStringField(TEXT("playerName"), PlayerName);
        Obj->SetStringField(TEXT("removedBy"),  RemovedBy);
        UBanWebSocketPusher::PushEvent(TEXT("unban"), Obj);
    }
}

void FBanDiscordNotifier::NotifyWarningIssued(const FString& Uid, const FString& PlayerName,
                                              const FString& Reason, const FString& WarnedBy,
                                              int32 TotalWarnings)
{
    const FString PlayerValue = PlayerName.IsEmpty()
        ? Uid
        : PlayerName + TEXT(" (") + Uid + TEXT(")");

    const FString Fields =
        Field(TEXT("Player"),         PlayerValue)                               + TEXT(",") +
        Field(TEXT("Reason"),         Reason)                                    + TEXT(",") +
        Field(TEXT("Warned By"),      WarnedBy)                                  + TEXT(",") +
        Field(TEXT("Total Warnings"), FString::FromInt(TotalWarnings));

    // Yellow: 16776960
    PostWebhook(BuildEmbed(16776960, TEXT("⚠️ Player Warned"), Fields));

    // WebSocket push
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("uid"),           Uid);
        Obj->SetStringField(TEXT("playerName"),    PlayerName);
        Obj->SetStringField(TEXT("reason"),        Reason);
        Obj->SetStringField(TEXT("warnedBy"),      WarnedBy);
        Obj->SetNumberField(TEXT("totalWarnings"), static_cast<double>(TotalWarnings));
        UBanWebSocketPusher::PushEvent(TEXT("warn"), Obj);
    }
}

void FBanDiscordNotifier::NotifyPlayerKicked(const FString& PlayerName, const FString& Reason,
                                             const FString& KickedBy)
{
    const FString Fields =
        Field(TEXT("Player"),    PlayerName) + TEXT(",") +
        Field(TEXT("Reason"),    Reason)     + TEXT(",") +
        Field(TEXT("Kicked By"), KickedBy);

    // Orange: 15105570
    PostWebhook(BuildEmbed(15105570, TEXT("👢 Player Kicked"), Fields));

    // WebSocket push
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("playerName"), PlayerName);
        Obj->SetStringField(TEXT("reason"),     Reason);
        Obj->SetStringField(TEXT("kickedBy"),   KickedBy);
        UBanWebSocketPusher::PushEvent(TEXT("kick"), Obj);
    }
}

void FBanDiscordNotifier::NotifyBanExpired(const FBanEntry& Entry)
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (!Cfg || !Cfg->bNotifyBanExpired) return;

    const FString PlayerValue = Entry.PlayerName.IsEmpty()
        ? Entry.Uid
        : Entry.PlayerName + TEXT(" (") + Entry.Uid + TEXT(")");

    const FString ExpireStr = Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")) + TEXT(" UTC");

    const FString Fields =
        Field(TEXT("Player"),     PlayerValue)   + TEXT(",") +
        Field(TEXT("Reason"),     Entry.Reason)  + TEXT(",") +
        Field(TEXT("Expired At"), ExpireStr);

    // Grey-blue: 8421504
    PostWebhook(BuildEmbed(8421504, TEXT("⏱️ Temp-Ban Expired"), Fields));

    // WebSocket push
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("uid"),        Entry.Uid);
        Obj->SetStringField(TEXT("playerName"), Entry.PlayerName);
        Obj->SetStringField(TEXT("reason"),     Entry.Reason);
        Obj->SetStringField(TEXT("expireDate"), Entry.ExpireDate.ToIso8601());
        UBanWebSocketPusher::PushEvent(TEXT("ban_expired"), Obj);
    }
}
