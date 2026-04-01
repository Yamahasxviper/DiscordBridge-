// Copyright Yamahasxviper. All Rights Reserved.

#include "Steam/SteamBanSubsystem.h"
// No CSS/FGOnlineHelpers.h include — the inline validation below is
// self-contained pure-string logic that requires no platform subsystem.
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
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
} // namespace SteamBanJson

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem interface
// ─────────────────────────────────────────────────────────────────────────────
void USteamBanSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);
    LoadBans();
    BackupBans();
    PruneExpiredBans();
    UE_LOG(LogSteamBanSystem, Log,
        TEXT("Steam ban subsystem initialised — %d active ban(s) loaded from disk."),
        BanMap.Num());
}

void USteamBanSubsystem::Deinitialize()
{
    // Clear the notification provider BEFORE saving bans.  If PruneExpiredBans
    // fires inside SaveBans it must not call back into a potentially torn-down
    // UDiscordBridgeSubsystem via the dangling raw pointer.
    NotificationProvider = nullptr;
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

    if (!IsValidSteam64Id(Steam64Id))
    {
        UE_LOG(LogSteamBanSystem, Warning,
            TEXT("BanPlayer: '%s' is not a valid Steam64 ID (must be 17 digits starting with 7656119)."),
            *Steam64Id);
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

    // Warn if we are overwriting an existing active ban record.
    if (const FBanEntry* Existing = BanMap.Find(Steam64Id))
    {
        UE_LOG(LogSteamBanSystem, Warning,
            TEXT("BanPlayer: overwriting existing ban for %s "
                 "(was: '%s', expires: %s) with new ban (reason: '%s')."),
            *Steam64Id, *Existing->Reason, *Existing->GetExpiryString(), *Entry.Reason);
    }

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

        // Notify subscribers so that external mods (e.g. Discord notification
        // bridges) react to the expiry, consistent with PruneExpiredBans().
        OnPlayerUnbanned.Broadcast(Steam64Id);
        if (NotificationProvider)
        {
            NotificationProvider->OnSteamPlayerUnbanned(Steam64Id);
        }

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
    for (const auto& Pair : BanMap)
    {
        if (!Pair.Value.IsExpired())
            Out.Add(Pair.Value);
    }
    return Out;
}

int32 USteamBanSubsystem::GetBanCount() const
{
    int32 Count = 0;
    for (const auto& Pair : BanMap)
    {
        if (!Pair.Value.IsExpired())
            ++Count;
    }
    return Count;
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

        // Notify subscribers that this player is no longer banned so that
        // external mods (e.g. Discord notification bridges) can react.
        OnPlayerUnbanned.Broadcast(Id);
        if (NotificationProvider)
        {
            NotificationProvider->OnSteamPlayerUnbanned(Id);
        }
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
    // Pure string validation — no platform or engine subsystem required.
    // Matches the inline logic in EOSId::IsValidSteam64Id (FGOnlineHelpers)
    // but inlined here to avoid pulling in the CSS FGOnlineHelpers.h include chain
    // (which cascades through FGNetConstructionFunctionLibrary → FGBuildableSubsystem
    // → many other CSS headers that are unnecessary for this one check).
    if (Id.Len() != 17) return false;
    for (TCHAR C : Id)
        if (C < TEXT('0') || C > TEXT('9')) return false;
    // All real Steam64 IDs are in the universe-1 range and share this prefix.
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

    for (int32 Idx = 0; Idx < BansArray->Num(); ++Idx)
    {
        const TSharedPtr<FJsonValue>& Val = (*BansArray)[Idx];
        if (!Val.IsValid() || Val->Type != EJson::Object)
        {
            UE_LOG(LogSteamBanSystem, Warning,
                TEXT("LoadBans: entry[%d] is not a JSON object — skipped."), Idx);
            continue;
        }
        FBanEntry Entry;
        if (SteamBanJson::JsonToEntry(Val->AsObject(), Entry))
            BanMap.Add(Entry.PlayerId, Entry);
        else
            UE_LOG(LogSteamBanSystem, Warning,
                TEXT("LoadBans: failed to parse entry[%d] — skipped."), Idx);
    }
}

void USteamBanSubsystem::BackupBans() const
{
    const FString FilePath = GetBanFilePath();
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

    // Nothing to back up if the live ban file does not exist yet.
    if (!PF.FileExists(*FilePath))
        return;

    // Build the backup directory: <BanSystem>/Backups/
    const FString BackupDir = FPaths::GetPath(FilePath) / TEXT("Backups");
    if (!PF.DirectoryExists(*BackupDir))
        PF.CreateDirectoryTree(*BackupDir);

    // Timestamped filename — lexicographically sortable, filesystem-safe.
    const FDateTime Now = FDateTime::UtcNow();
    const FString Stamp = FString::Printf(TEXT("%04d-%02d-%02d_%02d-%02d-%02d"),
        Now.GetYear(), Now.GetMonth(), Now.GetDay(),
        Now.GetHour(), Now.GetMinute(), Now.GetSecond());
    const FString BackupPath = BackupDir / FString::Printf(TEXT("SteamBans_%s.json"), *Stamp);

    if (!PF.CopyFile(*BackupPath, *FilePath))
    {
        UE_LOG(LogSteamBanSystem, Warning,
            TEXT("BackupBans: failed to create backup at %s"), *BackupPath);
        return;
    }

    UE_LOG(LogSteamBanSystem, Log,
        TEXT("BackupBans: ban list backed up to %s"), *BackupPath);

    // Prune old backups — keep only the most recent MaxBackups files.
    static constexpr int32 MaxBackups = 5;
    TArray<FString> BackupFiles;
    IFileManager::Get().FindFiles(BackupFiles, *(BackupDir / TEXT("*.json")), true, false);
    BackupFiles.Sort();

    const int32 NumToDelete = BackupFiles.Num() - MaxBackups;
    for (int32 i = 0; i < NumToDelete; ++i)
    {
        PF.DeleteFile(*(BackupDir / BackupFiles[i]));
        UE_LOG(LogSteamBanSystem, Verbose,
            TEXT("BackupBans: pruned old backup %s"), *BackupFiles[i]);
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

    // Atomic rename: write to a temp file first, then move it over the target.
    // This ensures the ban file is never left in a partially-written state if
    // the server is killed mid-write.
    const FString TempPath = FilePath + TEXT(".tmp");
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!FFileHelper::SaveStringToFile(JsonStr, *TempPath))
    {
        UE_LOG(LogSteamBanSystem, Error,
            TEXT("SaveBans: could not write temp file %s"), *TempPath);
        return;
    }

    // Try the atomic rename first.  On POSIX the rename(2) syscall replaces the
    // destination in a single atomic step; we never need to pre-delete.
    // On Windows, MoveFile fails when the destination already exists, so we
    // delete the old file and retry — but only after the temp write succeeded.
    if (!PF.MoveFile(*FilePath, *TempPath))
    {
        // Pre-delete the existing file (Windows path) and retry.
        if (PF.FileExists(*FilePath))
            PF.DeleteFile(*FilePath);

        if (!PF.MoveFile(*FilePath, *TempPath))
        {
            // Leave the temp file in place — it contains the latest ban data and
            // can be recovered manually.  Deleting it here would cause data loss.
            UE_LOG(LogSteamBanSystem, Error,
                TEXT("SaveBans: could not rename temp file to %s — "
                     "ban data preserved in %s"),
                *FilePath, *TempPath);
        }
    }
}
