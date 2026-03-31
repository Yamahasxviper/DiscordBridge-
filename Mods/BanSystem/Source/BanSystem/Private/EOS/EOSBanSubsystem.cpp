// Copyright Yamahasxviper. All Rights Reserved.

#include "EOS/EOSBanSubsystem.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"

DEFINE_LOG_CATEGORY(LogEOSBanSystem);

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace EOSBanJson
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

        FString IdStr;
        if (!Obj->TryGetStringField(KeyId, IdStr) || IdStr.IsEmpty()) return false;
        OutEntry.PlayerId = IdStr;

        FString ReasonStr;
        Obj->TryGetStringField(KeyReason, ReasonStr);
        OutEntry.Reason = ReasonStr;

        FString BannedByStr;
        Obj->TryGetStringField(KeyBannedBy, BannedByStr);
        OutEntry.BannedBy = BannedByStr;

        bool bIsPermanent = false;
        Obj->TryGetBoolField(KeyIsPermanent, bIsPermanent);
        OutEntry.bIsPermanent = bIsPermanent;

        FString BannedAtStr;
        if (Obj->TryGetStringField(KeyBannedAt, BannedAtStr))
            FDateTime::ParseIso8601(*BannedAtStr, OutEntry.BannedAt);

        FString ExpiresAtStr;
        if (Obj->TryGetStringField(KeyExpiresAt, ExpiresAtStr))
            FDateTime::ParseIso8601(*ExpiresAtStr, OutEntry.ExpiresAt);

        return true;
    }
} // namespace EOSBanJson

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem interface
// ─────────────────────────────────────────────────────────────────────────────
void UEOSBanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadBans();
    PruneExpiredBans();
    UE_LOG(LogEOSBanSystem, Log,
        TEXT("EOS ban subsystem initialised — %d active ban(s) loaded from disk."),
        BanMap.Num());
}

void UEOSBanSubsystem::Deinitialize()
{
    SaveBans();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Ban Management
// ─────────────────────────────────────────────────────────────────────────────
bool UEOSBanSubsystem::BanPlayer(const FString& EOSProductUserId,
                                 const FString& Reason,
                                 int32          DurationMinutes,
                                 const FString& BannedBy)
{
    if (EOSProductUserId.IsEmpty())
    {
        UE_LOG(LogEOSBanSystem, Warning, TEXT("BanPlayer: empty EOSProductUserId provided."));
        return false;
    }

    if (!IsValidEOSProductUserId(EOSProductUserId))
    {
        UE_LOG(LogEOSBanSystem, Warning,
            TEXT("BanPlayer: '%s' is not a valid EOS Product User ID (must be 32 hex chars)."),
            *EOSProductUserId);
        return false;
    }

    FBanEntry Entry;
    Entry.PlayerId     = EOSProductUserId.ToLower();
    Entry.Reason       = Reason.IsEmpty() ? TEXT("Banned by server administrator") : Reason;
    Entry.BannedAt     = FDateTime::UtcNow();
    Entry.BannedBy     = BannedBy;
    Entry.bIsPermanent = (DurationMinutes <= 0);
    Entry.ExpiresAt    = Entry.bIsPermanent
                            ? FDateTime(0)
                            : FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);

    BanMap.Add(Entry.PlayerId, Entry);
    SaveBans();

    UE_LOG(LogEOSBanSystem, Log,
        TEXT("EOS player %s banned by %s — Reason: %s — Duration: %s"),
        *Entry.PlayerId, *BannedBy, *Entry.Reason, *Entry.GetExpiryString());

    OnPlayerBanned.Broadcast(Entry.PlayerId, Entry);

    if (NotificationProvider)
    {
        NotificationProvider->OnEOSPlayerBanned(Entry.PlayerId, Entry);
    }

    return true;
}

bool UEOSBanSubsystem::UnbanPlayer(const FString& EOSProductUserId)
{
    const FString Key = EOSProductUserId.ToLower();
    if (BanMap.Remove(Key) > 0)
    {
        SaveBans();
        UE_LOG(LogEOSBanSystem, Log, TEXT("EOS player %s unbanned."), *Key);
        OnPlayerUnbanned.Broadcast(Key);

        if (NotificationProvider)
        {
            NotificationProvider->OnEOSPlayerUnbanned(Key);
        }

        return true;
    }
    UE_LOG(LogEOSBanSystem, Warning,
        TEXT("UnbanPlayer: no ban found for EOS PUID %s"), *Key);
    return false;
}

EBanCheckResult UEOSBanSubsystem::CheckPlayerBan(const FString& EOSProductUserId, FBanEntry& OutEntry)
{
    const FString Key = EOSProductUserId.ToLower();
    FBanEntry* Found = BanMap.Find(Key);
    if (!Found) return EBanCheckResult::NotBanned;

    if (Found->IsExpired())
    {
        UE_LOG(LogEOSBanSystem, Log,
            TEXT("Timed ban for %s has expired — removing."), *Key);
        BanMap.Remove(Key);
        SaveBans();
        return EBanCheckResult::BanExpired;
    }

    OutEntry = *Found;
    return EBanCheckResult::Banned;
}

bool UEOSBanSubsystem::IsPlayerBanned(const FString& EOSProductUserId, FString& OutReason)
{
    FBanEntry Entry;
    EBanCheckResult Result = CheckPlayerBan(EOSProductUserId, Entry);
    if (Result == EBanCheckResult::Banned)
    {
        OutReason = Entry.Reason;
        return true;
    }
    return false;
}

TArray<FBanEntry> UEOSBanSubsystem::GetAllBans() const
{
    TArray<FBanEntry> Out;
    for (const auto& Pair : BanMap)
    {
        if (!Pair.Value.IsExpired())
            Out.Add(Pair.Value);
    }
    return Out;
}

int32 UEOSBanSubsystem::GetBanCount() const
{
    int32 Count = 0;
    for (const auto& Pair : BanMap)
    {
        if (!Pair.Value.IsExpired())
            ++Count;
    }
    return Count;
}

void UEOSBanSubsystem::PruneExpiredBans()
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
        UE_LOG(LogEOSBanSystem, Log, TEXT("Pruned expired EOS ban for %s."), *Id);

        // Notify subscribers that this player is no longer banned so that
        // external mods (e.g. Discord notification bridges) can react.
        OnPlayerUnbanned.Broadcast(Id);
        if (NotificationProvider)
        {
            NotificationProvider->OnEOSPlayerUnbanned(Id);
        }
    }
    if (ToRemove.Num() > 0)
        SaveBans();
}

void UEOSBanSubsystem::ReloadBans()
{
    BanMap.Empty();
    LoadBans();
    UE_LOG(LogEOSBanSystem, Log,
        TEXT("EOS ban list reloaded — %d ban(s)."), BanMap.Num());
}

// ─────────────────────────────────────────────────────────────────────────────
//  Notification Provider
// ─────────────────────────────────────────────────────────────────────────────
void UEOSBanSubsystem::SetNotificationProvider(IBanNotificationProvider* Provider)
{
    NotificationProvider = Provider;
    UE_LOG(LogEOSBanSystem, Log,
        TEXT("EOS ban notification provider %s."),
        Provider ? TEXT("registered") : TEXT("cleared"));
}

// ─────────────────────────────────────────────────────────────────────────────
//  Validation
// ─────────────────────────────────────────────────────────────────────────────
bool UEOSBanSubsystem::IsValidEOSProductUserId(const FString& Id)
{
    if (Id.Len() != 32) return false;
    const FString Lower = Id.ToLower();
    for (TCHAR C : Lower)
    {
        const bool bDigit = (C >= TEXT('0') && C <= TEXT('9'));
        const bool bHex   = (C >= TEXT('a') && C <= TEXT('f'));
        if (!bDigit && !bHex) return false;
    }
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Persistence (JSON)
// ─────────────────────────────────────────────────────────────────────────────
FString UEOSBanSubsystem::GetBanFilePath() const
{
    return FPaths::ProjectSavedDir() / TEXT("BanSystem") / TEXT("EOSBans.json");
}

void UEOSBanSubsystem::LoadBans()
{
    const FString FilePath = GetBanFilePath();
    if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
        return;

    FString JsonStr;
    if (!FFileHelper::LoadFileToString(JsonStr, *FilePath))
    {
        UE_LOG(LogEOSBanSystem, Warning,
            TEXT("LoadBans: failed to read %s"), *FilePath);
        return;
    }

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonStr);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogEOSBanSystem, Warning,
            TEXT("LoadBans: JSON parse error in %s"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* BansArray = nullptr;
    if (!Root->TryGetArrayField(EOSBanJson::KeyBans, BansArray) || !BansArray)
        return;

    for (const TSharedPtr<FJsonValue>& Val : *BansArray)
    {
        if (!Val.IsValid() || Val->Type != EJson::Object) continue;
        FBanEntry Entry;
        if (EOSBanJson::JsonToEntry(Val->AsObject(), Entry))
            BanMap.Add(Entry.PlayerId, Entry);
    }
}

void UEOSBanSubsystem::SaveBans() const
{
    const FString FilePath = GetBanFilePath();
    const FString Dir = FPaths::GetPath(FilePath);
    if (!FPlatformFileManager::Get().GetPlatformFile().DirectoryExists(*Dir))
        FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*Dir);

    TArray<TSharedPtr<FJsonValue>> BansArray;
    for (const auto& Pair : BanMap)
        BansArray.Add(MakeShared<FJsonValueObject>(EOSBanJson::EntryToJson(Pair.Value)));

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(EOSBanJson::KeyBans, BansArray);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogEOSBanSystem, Error, TEXT("SaveBans: JSON serialisation failed."));
        return;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
    {
        UE_LOG(LogEOSBanSystem, Error,
            TEXT("SaveBans: could not write %s"), *FilePath);
    }
}
