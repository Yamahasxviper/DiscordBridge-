// Copyright Yamahasxviper. All Rights Reserved.

#include "BanTypes.h"
#include "BanSystemConfig.h"

// ─────────────────────────────────────────────────────────────────────────────
//  FBanTemplate helpers
// ─────────────────────────────────────────────────────────────────────────────

bool FBanTemplate::FromConfigString(const FString& ConfigStr, FBanTemplate& OutTemplate)
{
    TArray<FString> Parts;
    ConfigStr.ParseIntoArray(Parts, TEXT("|"));
    if (Parts.Num() < 3) return false;

    OutTemplate.Slug            = Parts[0];
    OutTemplate.DurationMinutes = FCString::Atoi(*Parts[1]);
    OutTemplate.Reason          = Parts[2];
    OutTemplate.Category        = Parts.Num() > 3 ? Parts[3] : FString();
    return true;
}

TArray<FBanTemplate> FBanTemplate::ParseTemplates(const TArray<FString>& ConfigStrings)
{
    TArray<FBanTemplate> Result;
    Result.Reserve(ConfigStrings.Num());
    for (const FString& Str : ConfigStrings)
    {
        FBanTemplate T;
        if (FromConfigString(Str, T))
        {
            Result.Add(MoveTemp(T));
        }
    }
    return Result;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FBanEntry::GetKickMessage
// ─────────────────────────────────────────────────────────────────────────────

FString FBanEntry::GetKickMessage() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();

    // Determine which template to use (permanent vs. temporary).
    FString Tmpl;
    if (Cfg)
    {
        Tmpl = bIsPermanent
            ? Cfg->BanKickMessageTemplate
            : Cfg->TempBanKickMessageTemplate;
    }

    // Apply the configured template, substituting supported variables.
    if (!Tmpl.IsEmpty())
    {
        const FString ExpiryStr = bIsPermanent
            ? TEXT("never")
            : ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")) + TEXT(" UTC");

        const FString AppealUrlStr = (Cfg && !Cfg->AppealUrl.IsEmpty())
            ? Cfg->AppealUrl
            : TEXT("(contact server admin)");

        Tmpl = Tmpl.Replace(TEXT("{reason}"),      *Reason,        ESearchCase::IgnoreCase);
        Tmpl = Tmpl.Replace(TEXT("{expiry}"),       *ExpiryStr,     ESearchCase::IgnoreCase);
        Tmpl = Tmpl.Replace(TEXT("{appeal_url}"),   *AppealUrlStr,  ESearchCase::IgnoreCase);
        return Tmpl;
    }

    // Built-in fallback messages.
    const FString BaseAppeal = (Cfg && !Cfg->AppealUrl.IsEmpty())
        ? FString::Printf(TEXT(" Appeal at: %s"), *Cfg->AppealUrl)
        : TEXT("");

    if (bIsPermanent)
    {
        return FString::Printf(
            TEXT("You have been permanently banned. Reason: %s%s"),
            *Reason, *BaseAppeal);
    }
    return FString::Printf(
        TEXT("You are banned until %s UTC. Reason: %s%s"),
        *ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
        *Reason, *BaseAppeal);
}
