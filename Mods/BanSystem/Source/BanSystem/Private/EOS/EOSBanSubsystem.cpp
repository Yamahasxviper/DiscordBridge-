// Copyright Yamahasxviper. All Rights Reserved.

#include "EOS/EOSBanSubsystem.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
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
        // Normalize to lowercase to match BanPlayer() and CheckPlayerBan() which
        // always use ToLower() on EOS PUIDs.  This prevents a ban-bypass if the
        // JSON file was written or edited externally with mixed-case IDs.
        OutEntry.PlayerId = IdStr.ToLower();

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
    BackupBans();
    PruneExpiredBans();
    UE_LOG(LogEOSBanSystem, Log,
        TEXT("EOS ban subsystem initialised — %d active ban(s) loaded from disk."),
        BanMap.Num());
}

void UEOSBanSubsystem::Deinitialize()
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

    // Warn if we are overwriting an existing active ban record.
    if (const FBanEntry* Existing = BanMap.Find(Entry.PlayerId))
    {
        UE_LOG(LogEOSBanSystem, Warning,
            TEXT("BanPlayer: overwriting existing ban for %s "
                 "(was: '%s', expires: %s) with new ban (reason: '%s')."),
            *Entry.PlayerId, *Existing->Reason, *Existing->GetExpiryString(), *Entry.Reason);
    }

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

        // Notify subscribers so that external mods (e.g. Discord notification
        // bridges) react to the expiry, consistent with PruneExpiredBans().
        OnPlayerUnbanned.Broadcast(Key);
        if (NotificationProvider)
        {
            NotificationProvider->OnEOSPlayerUnbanned(Key);
        }

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

    for (int32 Idx = 0; Idx < BansArray->Num(); ++Idx)
    {
        const TSharedPtr<FJsonValue>& Val = (*BansArray)[Idx];
        if (!Val.IsValid() || Val->Type != EJson::Object)
        {
            UE_LOG(LogEOSBanSystem, Warning,
                TEXT("LoadBans: entry[%d] is not a JSON object — skipped."), Idx);
            continue;
        }
        FBanEntry Entry;
        if (EOSBanJson::JsonToEntry(Val->AsObject(), Entry))
            BanMap.Add(Entry.PlayerId, Entry);
        else
            UE_LOG(LogEOSBanSystem, Warning,
                TEXT("LoadBans: failed to parse entry[%d] — skipped."), Idx);
    }
}

void UEOSBanSubsystem::BackupBans() const
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
    const FString BackupPath = BackupDir / FString::Printf(TEXT("EOSBans_%s.json"), *Stamp);

    if (!PF.CopyFile(*BackupPath, *FilePath))
    {
        UE_LOG(LogEOSBanSystem, Warning,
            TEXT("BackupBans: failed to create backup at %s"), *BackupPath);
        return;
    }

    UE_LOG(LogEOSBanSystem, Log,
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
        UE_LOG(LogEOSBanSystem, Verbose,
            TEXT("BackupBans: pruned old backup %s"), *BackupFiles[i]);
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

    // Atomic rename: write to a temp file first, then move it over the target.
    // This ensures the ban file is never left in a partially-written state if
    // the server is killed mid-write.
    const FString TempPath = FilePath + TEXT(".tmp");
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!FFileHelper::SaveStringToFile(JsonStr, *TempPath))
    {
        UE_LOG(LogEOSBanSystem, Error,
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
            UE_LOG(LogEOSBanSystem, Error,
                TEXT("SaveBans: could not rename temp file to %s — "
                     "ban data preserved in %s"),
                *FilePath, *TempPath);
        }
    }
}
