// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/database.ts

#include "BanDatabase.h"
#include "BanSystemConfig.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogBanDatabase);

// ─────────────────────────────────────────────────────────────────────────────
//  Internal JSON helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace BanDbJson
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
            Obj->SetField(TEXT("expireDate"), MakeShared<FJsonValueNull>());
        else
            Obj->SetStringField(TEXT("expireDate"), E.ExpireDate.ToIso8601());
        Obj->SetBoolField(TEXT("isPermanent"), E.bIsPermanent);

        // Persist linked UIDs (cross-platform identity).
        if (E.LinkedUids.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> LinkedArr;
            for (const FString& L : E.LinkedUids)
                LinkedArr.Add(MakeShared<FJsonValueString>(L));
            Obj->SetArrayField(TEXT("linkedUids"), LinkedArr);
        }

        // Category and evidence (may be empty — omit for cleaner JSON).
        if (!E.Category.IsEmpty())
            Obj->SetStringField(TEXT("category"), E.Category);

        if (E.Evidence.Num() > 0)
        {
            TArray<TSharedPtr<FJsonValue>> EvidArr;
            for (const FString& Ev : E.Evidence)
                EvidArr.Add(MakeShared<FJsonValueString>(Ev));
            Obj->SetArrayField(TEXT("evidence"), EvidArr);
        }

        return Obj;
    }

    static bool JsonToEntry(const TSharedPtr<FJsonObject>& Obj, FBanEntry& OutEntry)
    {
        if (!Obj.IsValid()) return false;

        double IdDbl = 0.0;
        Obj->TryGetNumberField(TEXT("id"), IdDbl);
        OutEntry.Id = static_cast<int64>(IdDbl);

        Obj->TryGetStringField(TEXT("uid"),        OutEntry.Uid);
        Obj->TryGetStringField(TEXT("playerUID"),  OutEntry.PlayerUID);
        Obj->TryGetStringField(TEXT("platform"),   OutEntry.Platform);
        Obj->TryGetStringField(TEXT("playerName"), OutEntry.PlayerName);
        Obj->TryGetStringField(TEXT("reason"),     OutEntry.Reason);
        Obj->TryGetStringField(TEXT("bannedBy"),   OutEntry.BannedBy);

        FString BanDateStr;
        if (Obj->TryGetStringField(TEXT("banDate"), BanDateStr))
            FDateTime::ParseIso8601(*BanDateStr, OutEntry.BanDate);

        bool bPerm = true;
        Obj->TryGetBoolField(TEXT("isPermanent"), bPerm);
        OutEntry.bIsPermanent = bPerm;

        if (!bPerm)
        {
            FString ExpireDateStr;
            if (Obj->TryGetStringField(TEXT("expireDate"), ExpireDateStr) && !ExpireDateStr.IsEmpty())
            {
                FDateTime::ParseIso8601(*ExpireDateStr, OutEntry.ExpireDate);
            }
            else
            {
                // Malformed record — isPermanent=false but expireDate is missing or empty.
                // Treat as permanent to avoid an unbounded active-ban window.
                UE_LOG(LogBanDatabase, Warning,
                    TEXT("BanDatabase: record uid='%s' has isPermanent=false but "
                         "expireDate is missing or empty — treating as permanent"),
                    *OutEntry.Uid);
                OutEntry.bIsPermanent = true;
                OutEntry.ExpireDate   = FDateTime(0);
            }
        }
        else
        {
            OutEntry.ExpireDate = FDateTime(0);
        }

        // Restore linked UIDs (may be absent in older records).
        const TArray<TSharedPtr<FJsonValue>>* LinkedArr = nullptr;
        if (Obj->TryGetArrayField(TEXT("linkedUids"), LinkedArr) && LinkedArr)
        {
            for (const TSharedPtr<FJsonValue>& Val : *LinkedArr)
            {
                FString L;
                if (Val.IsValid() && Val->TryGetString(L) && !L.IsEmpty())
                    OutEntry.LinkedUids.Add(L);
            }
        }

        // Category (optional, absent in records written before this feature).
        Obj->TryGetStringField(TEXT("category"), OutEntry.Category);

        // Evidence (optional array).
        const TArray<TSharedPtr<FJsonValue>>* EvidArr = nullptr;
        if (Obj->TryGetArrayField(TEXT("evidence"), EvidArr) && EvidArr)
        {
            for (const TSharedPtr<FJsonValue>& Val : *EvidArr)
            {
                FString Ev;
                if (Val.IsValid() && Val->TryGetString(Ev) && !Ev.IsEmpty())
                    OutEntry.Evidence.Add(Ev);
            }
        }

        return !OutEntry.Uid.IsEmpty();
    }
} // namespace BanDbJson

// Static delegate definitions.
UBanDatabase::FOnBanAdded   UBanDatabase::OnBanAdded;
UBanDatabase::FOnBanRemoved UBanDatabase::OnBanRemoved;

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanDatabase::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    DbPath = GetDatabasePath();

    // Ensure the directory exists.
    const FString Dir = FPaths::GetPath(DbPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    // Bootstrap the file the very first time (or if it was deleted).
    // Calling SaveToFile() here ensures bans.json always exists after
    // initialization so that a subsequent !ban + server-restart cycle can
    // never leave the file absent.
    if (!PF.FileExists(*DbPath))
    {
        SaveToFile();
        UE_LOG(LogBanDatabase, Log,
            TEXT("BanDatabase: created empty ban database at %s"), *DbPath);
    }

    // Capture the mtime so we can detect external edits in ReloadIfChanged().
    LastKnownFileModTime = IFileManager::Get().GetTimeStamp(*DbPath);

    const int32 Pruned = PruneExpiredBans();
    UE_LOG(LogBanDatabase, Log,
        TEXT("BanDatabase: loaded %s (%d ban(s), pruned %d expired)"),
        *DbPath, Bans.Num(), Pruned);
}

void UBanDatabase::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Hot-reload on external file change
// ─────────────────────────────────────────────────────────────────────────────

void UBanDatabase::ReloadIfChanged()
{
    const FDateTime NewModTime = IFileManager::Get().GetTimeStamp(*DbPath);

    // GetTimeStamp returns FDateTime(0) when the file does not exist.
    // In that case there is nothing to reload, so bail out early.
    if (NewModTime == FDateTime(0) || NewModTime == LastKnownFileModTime)
        return;

    UE_LOG(LogBanDatabase, Log,
        TEXT("BanDatabase: bans.json changed on disk, reloading (%s)"), *DbPath);

    LoadFromFile();

    // Prune any bans that expired while the file was externally edited and
    // immediately write the cleaned list back so the file stays consistent.
    const int32 Pruned = PruneExpiredBans();

    // Record the definitive mtime after all writes are done.
    {
        FScopeLock Lock(&DbMutex);
        LastKnownFileModTime = IFileManager::Get().GetTimeStamp(*DbPath);
        UE_LOG(LogBanDatabase, Log,
            TEXT("BanDatabase: reload complete (%d ban(s), pruned %d expired)"),
            Bans.Num(), Pruned);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UBanDatabase::LoadFromFile()
{
    // Caller must hold DbMutex — but Initialize() calls this before subsystems
    // are shared, so we take the lock here for safety.
    FScopeLock Lock(&DbMutex);

    Bans.Empty();
    NextId = 1;

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *DbPath))
        return; // File doesn't exist yet — that's fine on first run.

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogBanDatabase, Warning,
            TEXT("BanDatabase: failed to parse %s — starting with empty ban list"), *DbPath);
        return;
    }

    double NextIdDbl = 1.0;
    Root->TryGetNumberField(TEXT("nextId"), NextIdDbl);
    NextId = FMath::Max((int64)1, static_cast<int64>(NextIdDbl));

    const TArray<TSharedPtr<FJsonValue>>* BanArray = nullptr;
    if (Root->TryGetArrayField(TEXT("bans"), BanArray) && BanArray)
    {
        for (const TSharedPtr<FJsonValue>& Val : *BanArray)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FBanEntry E;
            if (BanDbJson::JsonToEntry(*ObjPtr, E))
                Bans.Add(E);
        }
    }
}

bool UBanDatabase::SaveToFile() const
{
    // Caller must hold DbMutex.

    // Defensively recreate the directory — it may have been absent on first
    // run or removed externally between Initialize() and the first write.
    const FString Dir = FPaths::GetPath(DbPath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    TArray<TSharedPtr<FJsonValue>> BanArray;
    BanArray.Reserve(Bans.Num());
    for (const FBanEntry& E : Bans)
        BanArray.Add(MakeShared<FJsonValueObject>(BanDbJson::EntryToJson(E)));

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetNumberField(TEXT("nextId"), static_cast<double>(NextId));
    Root->SetArrayField(TEXT("bans"), BanArray);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanDatabase, Error, TEXT("BanDatabase: failed to serialize ban list"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *DbPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBanDatabase, Error,
            TEXT("BanDatabase: failed to write %s"), *DbPath);
        return false;
    }

    // Update the cached mtime so ReloadIfChanged() does not treat our own
    // writes as external edits and reload the file unnecessarily.
    LastKnownFileModTime = IFileManager::Get().GetTimeStamp(*DbPath);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Write
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDatabase::AddBan(const FBanEntry& Entry)
{
    FBanEntry NewEntry;
    bool bSaved;

    {
        FScopeLock Lock(&DbMutex);

        // Upsert: remove any existing record with the same UID first.
        Bans.RemoveAll([&Entry](const FBanEntry& E){ return E.Uid == Entry.Uid; });

        NewEntry = Entry;
        if (NewEntry.Id <= 0)
            NewEntry.Id = NextId++;
        else
            NextId = FMath::Max(NextId, NewEntry.Id + 1);

        Bans.Add(NewEntry);
        bSaved = SaveToFile();
    }

    if (bSaved)
        OnBanAdded.Broadcast(NewEntry);

    return bSaved;
}

bool UBanDatabase::RemoveBanByUid(const FString& Uid)
{
    FString RemovedPlayerName;
    bool bRemoved;

    {
        FScopeLock Lock(&DbMutex);

        // Capture player name before removal for the delegate.
        for (const FBanEntry& E : Bans)
        {
            if (E.Uid == Uid) { RemovedPlayerName = E.PlayerName; break; }
        }

        const int32 Removed = Bans.RemoveAll([&Uid](const FBanEntry& E){ return E.Uid == Uid; });
        bRemoved = (Removed > 0);
        if (bRemoved) SaveToFile();
    }

    if (bRemoved)
        OnBanRemoved.Broadcast(Uid, RemovedPlayerName);

    return bRemoved;
}

bool UBanDatabase::RemoveBanById(int64 Id)
{
    FScopeLock Lock(&DbMutex);

    const int32 Removed = Bans.RemoveAll([Id](const FBanEntry& E){ return E.Id == Id; });
    if (Removed == 0)
        return false;

    return SaveToFile();
}

int32 UBanDatabase::PruneExpiredBans()
{
    FScopeLock Lock(&DbMutex);

    const FDateTime Now = FDateTime::UtcNow();
    const int32 Before  = Bans.Num();

    Bans.RemoveAll([&Now](const FBanEntry& E)
    {
        return !E.bIsPermanent && E.ExpireDate < Now;
    });

    const int32 Pruned = Before - Bans.Num();
    if (Pruned > 0)
        SaveToFile();

    return Pruned;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Read
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDatabase::IsCurrentlyBanned(const FString& Uid, FBanEntry& OutEntry) const
{
    FScopeLock Lock(&DbMutex);

    for (const FBanEntry& E : Bans)
    {
        if (E.Uid != Uid) continue;

        // Permanent bans are always active.
        if (E.bIsPermanent)
        {
            OutEntry = E;
            return true;
        }
        // Temporary bans are active until expiry.
        if (FDateTime::UtcNow() < E.ExpireDate)
        {
            OutEntry = E;
            return true;
        }
        // Expired — not currently banned.
        return false;
    }
    return false;
}

bool UBanDatabase::IsCurrentlyBannedByAnyId(const FString& Uid, FBanEntry& OutEntry) const
{
    FScopeLock Lock(&DbMutex);

    const FDateTime Now = FDateTime::UtcNow();
    for (const FBanEntry& E : Bans)
    {
        if (!E.MatchesUid(Uid)) continue;

        if (E.bIsPermanent || Now < E.ExpireDate)
        {
            OutEntry = E;
            return true;
        }
        // Expired — keep scanning in case a different linked record is still active.
    }
    return false;
}

bool UBanDatabase::GetBanByUid(const FString& Uid, FBanEntry& OutEntry) const
{
    FScopeLock Lock(&DbMutex);
    return GetBanByUid_Locked(Uid, OutEntry);
}

bool UBanDatabase::GetBanByUid_Locked(const FString& Uid, FBanEntry& OutEntry) const
{
    for (const FBanEntry& E : Bans)
    {
        if (E.Uid == Uid)
        {
            OutEntry = E;
            return true;
        }
    }
    return false;
}

TArray<FBanEntry> UBanDatabase::GetActiveBans() const
{
    FScopeLock Lock(&DbMutex);

    const FDateTime Now = FDateTime::UtcNow();
    TArray<FBanEntry> Active;
    for (const FBanEntry& E : Bans)
    {
        if (E.bIsPermanent || Now < E.ExpireDate)
            Active.Add(E);
    }
    return Active;
}

TArray<FBanEntry> UBanDatabase::GetAllBans() const
{
    FScopeLock Lock(&DbMutex);
    return Bans;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Cross-platform linking
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDatabase::LinkBans(const FString& UidA, const FString& UidB)
{
    if (UidA.IsEmpty() || UidB.IsEmpty() || UidA.Equals(UidB, ESearchCase::IgnoreCase))
        return false;

    FScopeLock Lock(&DbMutex);

    bool bDirty = false;

    auto AddLinkIfMissing = [&](FBanEntry& Entry, const FString& LinkUid) -> bool
    {
        for (const FString& L : Entry.LinkedUids)
            if (L.Equals(LinkUid, ESearchCase::IgnoreCase)) return false;
        Entry.LinkedUids.Add(LinkUid);
        return true;
    };

    for (FBanEntry& E : Bans)
    {
        if (E.Uid.Equals(UidA, ESearchCase::IgnoreCase))
            bDirty |= AddLinkIfMissing(E, UidB);
        if (E.Uid.Equals(UidB, ESearchCase::IgnoreCase))
            bDirty |= AddLinkIfMissing(E, UidA);
    }

    if (bDirty)
        return SaveToFile();

    UE_LOG(LogBanDatabase, Warning,
        TEXT("BanDatabase: LinkBans — no ban found for either '%s' or '%s'"), *UidA, *UidB);
    return false;
}

bool UBanDatabase::UnlinkBans(const FString& UidA, const FString& UidB)
{
    if (UidA.IsEmpty() || UidB.IsEmpty()) return false;

    FScopeLock Lock(&DbMutex);

    bool bDirty = false;

    for (FBanEntry& E : Bans)
    {
        if (E.Uid.Equals(UidA, ESearchCase::IgnoreCase) || E.Uid.Equals(UidB, ESearchCase::IgnoreCase))
        {
            const FString& ToRemove = E.Uid.Equals(UidA, ESearchCase::IgnoreCase) ? UidB : UidA;
            const int32 Removed = E.LinkedUids.RemoveAll([&ToRemove](const FString& L)
            {
                return L.Equals(ToRemove, ESearchCase::IgnoreCase);
            });
            if (Removed > 0) bDirty = true;
        }
    }

    return bDirty ? SaveToFile() : false;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Backup  (mirrors BanDatabase.backup() in database.ts)
// ─────────────────────────────────────────────────────────────────────────────

FString UBanDatabase::Backup(const FString& BackupDir, int32 MaxKeep) const
{
    FScopeLock Lock(&DbMutex);

    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();

    if (!PF.FileExists(*DbPath))
        return FString();

    if (!PF.DirectoryExists(*BackupDir))
        PF.CreateDirectoryTree(*BackupDir);

    const FDateTime Now   = FDateTime::UtcNow();
    const FString   Stamp = FString::Printf(
        TEXT("%04d-%02d-%02d_%02d-%02d-%02d"),
        Now.GetYear(), Now.GetMonth(),  Now.GetDay(),
        Now.GetHour(), Now.GetMinute(), Now.GetSecond());
    const FString Dest = BackupDir / FString::Printf(TEXT("bans_%s.json"), *Stamp);

    if (!PF.CopyFile(*Dest, *DbPath))
    {
        UE_LOG(LogBanDatabase, Warning,
            TEXT("Backup: failed to copy %s → %s"), *DbPath, *Dest);
        return FString();
    }

    UE_LOG(LogBanDatabase, Log, TEXT("Backup: %s"), *Dest);

    // Prune old backups beyond MaxKeep.
    TArray<FString> Files;
    IFileManager::Get().FindFiles(Files, *(BackupDir / TEXT("bans_*.json")), true, false);
    Files.Sort();
    while (Files.Num() > MaxKeep)
    {
        PF.DeleteFile(*(BackupDir / Files[0]));
        Files.RemoveAt(0);
    }

    return Dest;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────

FString UBanDatabase::MakeUid(const FString& Platform, const FString& PlayerUID)
{
    return Platform.ToUpper() + TEXT(":") + PlayerUID;
}

void UBanDatabase::ParseUid(const FString& Uid, FString& OutPlatform, FString& OutPlayerUID)
{
    int32 ColonIdx = INDEX_NONE;
    if (Uid.FindChar(TEXT(':'), ColonIdx) && ColonIdx > 0)
    {
        OutPlatform  = Uid.Left(ColonIdx).ToUpper();
        OutPlayerUID = Uid.Mid(ColonIdx + 1);
    }
    else
    {
        OutPlatform  = TEXT("UNKNOWN");
        OutPlayerUID = Uid;
    }
}

FString UBanDatabase::GetDatabasePath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        return Cfg->DatabasePath;

    return FPaths::ProjectSavedDir() / TEXT("BanSystem") / TEXT("bans.json");
}
