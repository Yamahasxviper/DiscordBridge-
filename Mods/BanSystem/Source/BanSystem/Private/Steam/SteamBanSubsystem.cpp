// Copyright Yamahasxviper. All Rights Reserved.

#include "Steam/SteamBanSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogSteamBanSystem);

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace SteamBanJson
{
    static const FString KeyId          = TEXT("Id");
    static const FString KeyReason      = TEXT("Reason");
    static const FString KeyBannedAt    = TEXT("BannedAt");
    static const FString KeyExpiresAt   = TEXT("ExpiresAt");
    static const FString KeyBannedBy    = TEXT("BannedBy");
    static const FString KeyIsPermanent = TEXT("IsPermanent");
    static const FString KeyBans        = TEXT("Bans");

    static TSharedPtr<FJsonObject> EntryToJson(const FBanEntry& Entry)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(KeyId,          Entry.PlayerId);
        Obj->SetStringField(KeyReason,      Entry.Reason);
        Obj->SetStringField(KeyBannedAt,    Entry.BannedAt.ToIso8601());
        Obj->SetStringField(KeyExpiresAt,   Entry.ExpiresAt.ToIso8601());
        Obj->SetStringField(KeyBannedBy,    Entry.BannedBy);
        Obj->SetBoolField  (KeyIsPermanent, Entry.bIsPermanent);
        return Obj;
    }

    static bool JsonToEntry(const TSharedPtr<FJsonObject>& Obj, FBanEntry& OutEntry)
    {
        if (!Obj) return false;
        OutEntry.PlayerId     = Obj->GetStringField(KeyId);
        OutEntry.Reason       = Obj->GetStringField(KeyReason);
        OutEntry.BannedBy     = Obj->GetStringField(KeyBannedBy);
        OutEntry.bIsPermanent = Obj->GetBoolField(KeyIsPermanent);

        FDateTime::ParseIso8601(*Obj->GetStringField(KeyBannedAt),  OutEntry.BannedAt);
        FDateTime::ParseIso8601(*Obj->GetStringField(KeyExpiresAt), OutEntry.ExpiresAt);
        return !OutEntry.PlayerId.IsEmpty();
    }
} // namespace SteamBanJson

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem interface
// ─────────────────────────────────────────────────────────────────────────────
void USteamBanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadBans();
    UE_LOG(LogSteamBanSystem, Log,
        TEXT("Steam ban subsystem initialised — %d ban(s) loaded from disk."),
        BanMap.Num());
}

void USteamBanSubsystem::Deinitialize()
{
    SaveBans();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban Management
// ─────────────────────────────────────────────────────────────────────────────
bool USteamBanSubsystem::BanPlayer(const FString& Steam64Id,
                                   const FString& Reason,
                                   int32          DurationMinutes,
                                   const FString& BannedBy)
{
    if (Steam64Id.IsEmpty())
    {
        UE_LOG(LogSteamBanSystem, Warning, TEXT("BanPlayer: empty Steam64Id provided."));
        return false;
    }

    FBanEntry Entry;
    Entry.PlayerId     = Steam64Id;
    Entry.Reason       = Reason.IsEmpty() ? TEXT("Banned by server administrator") : Reason;
    Entry.BannedAt     = FDateTime::UtcNow();
    Entry.BannedBy     = BannedBy;
    Entry.bIsPermanent = (DurationMinutes <= 0);
    Entry.ExpiresAt    = Entry.bIsPermanent
                            ? FDateTime(0)
                            : FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);

    BanMap.Add(Steam64Id, Entry);
    SaveBans();

    UE_LOG(LogSteamBanSystem, Log,
        TEXT("Steam player %s banned by %s — Reason: %s — Duration: %s"),
        *Steam64Id, *BannedBy, *Entry.Reason, *Entry.GetExpiryString());

    OnPlayerBanned.Broadcast(Steam64Id, Entry);

    if (NotificationProvider)
    {
        NotificationProvider->OnSteamPlayerBanned(Steam64Id, Entry);
    }

    return true;
}

bool USteamBanSubsystem::UnbanPlayer(const FString& Steam64Id)
{
    if (BanMap.Remove(Steam64Id) > 0)
    {
        SaveBans();
        UE_LOG(LogSteamBanSystem, Log, TEXT("Steam player %s unbanned."), *Steam64Id);
        OnPlayerUnbanned.Broadcast(Steam64Id);

        if (NotificationProvider)
        {
            NotificationProvider->OnSteamPlayerUnbanned(Steam64Id);
        }

        return true;
    }
    UE_LOG(LogSteamBanSystem, Warning,
        TEXT("UnbanPlayer: no ban found for Steam64Id %s"), *Steam64Id);
    return false;
}

EBanCheckResult USteamBanSubsystem::CheckPlayerBan(const FString& Steam64Id, FBanEntry& OutEntry)
{
    FBanEntry* Found = BanMap.Find(Steam64Id);
    if (!Found) return EBanCheckResult::NotBanned;

    if (Found->IsExpired())
    {
        UE_LOG(LogSteamBanSystem, Log,
            TEXT("Timed ban for %s has expired — removing."), *Steam64Id);
        BanMap.Remove(Steam64Id);
        SaveBans();
        return EBanCheckResult::BanExpired;
    }

    OutEntry = *Found;
    return EBanCheckResult::Banned;
}

bool USteamBanSubsystem::IsPlayerBanned(const FString& Steam64Id, FString& OutReason)
{
    FBanEntry Entry;
    EBanCheckResult Result = CheckPlayerBan(Steam64Id, Entry);
    if (Result == EBanCheckResult::Banned)
    {
        OutReason = Entry.Reason;
        return true;
    }
    return false;
}

TArray<FBanEntry> USteamBanSubsystem::GetAllBans() const
{
    TArray<FBanEntry> Out;
    BanMap.GenerateValueArray(Out);
    return Out;
}

int32 USteamBanSubsystem::GetBanCount() const
{
    return BanMap.Num();
}

void USteamBanSubsystem::PruneExpiredBans()
{
    TArray<FString> ToRemove;
    for (const auto& Pair : BanMap)
    {
        if (Pair.Value.IsExpired())
            ToRemove.Add(Pair.Key);
    }
    for (const FString& Id : ToRemove)
    {
        BanMap.Remove(Id);
        UE_LOG(LogSteamBanSystem, Log,
            TEXT("Pruned expired Steam ban for %s."), *Id);
    }
    if (ToRemove.Num() > 0)
        SaveBans();
}

void USteamBanSubsystem::ReloadBans()
{
    BanMap.Empty();
    LoadBans();
    UE_LOG(LogSteamBanSystem, Log,
        TEXT("Steam ban list reloaded — %d ban(s)."), BanMap.Num());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Notification Provider
// ─────────────────────────────────────────────────────────────────────────────
void USteamBanSubsystem::SetNotificationProvider(IBanNotificationProvider* Provider)
{
    NotificationProvider = Provider;
    UE_LOG(LogSteamBanSystem, Log,
        TEXT("Steam ban notification provider %s."),
        Provider ? TEXT("registered") : TEXT("cleared"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Validation
// ─────────────────────────────────────────────────────────────────────────────
bool USteamBanSubsystem::IsValidSteam64Id(const FString& Id)
{
    if (Id.Len() != 17) return false;
    for (TCHAR C : Id)
        if (C < TEXT('0') || C > TEXT('9')) return false;
    // Steam64 IDs live in the range [76561193005069312, 76561202255233023]
    // A quick prefix check covers the overwhelming majority of real IDs.
    return Id.StartsWith(TEXT("7656119"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistence (JSON)
// ─────────────────────────────────────────────────────────────────────────────
FString USteamBanSubsystem::GetBanFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("BanSystem") / TEXT("SteamBans.json");
}

void USteamBanSubsystem::LoadBans()
{
    const FString FilePath = GetBanFilePath();
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
        return;

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
    {
        UE_LOG(LogSteamBanSystem, Warning,
            TEXT("LoadBans: failed to read %s"), *FilePath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogSteamBanSystem, Warning,
            TEXT("LoadBans: JSON parse error in %s"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* BansArray = nullptr;
    if (!Root->TryGetArrayField(SteamBanJson::KeyBans, BansArray) || !BansArray)
        return;

    for (const TSharedPtr<FJsonValue>& Val : *BansArray)
    {
        if (!Val.IsValid() || Val->Type != EJson::Object) continue;
        FBanEntry Entry;
        if (SteamBanJson::JsonToEntry(Val->AsObject(), Entry))
            BanMap.Add(Entry.PlayerId, Entry);
    }
}

void USteamBanSubsystem::SaveBans() const
{
    const FString FilePath = GetBanFilePath();
    const FString Dir = FPaths::GetPath(FilePath);
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Dir))
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);

    TArray<TSharedPtr<FJsonValue>> BansArray;
    for (const auto& Pair : BanMap)
        BansArray.Add(MakeShared<FJsonValueObject>(SteamBanJson::EntryToJson(Pair.Value)));

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(SteamBanJson::KeyBans, BansArray);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogSteamBanSystem, Error,
            TEXT("SaveBans: JSON serialisation failed."));
        return;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
    {
        UE_LOG(LogSteamBanSystem, Error,
            TEXT("SaveBans: could not write %s"), *FilePath);
    }
}
