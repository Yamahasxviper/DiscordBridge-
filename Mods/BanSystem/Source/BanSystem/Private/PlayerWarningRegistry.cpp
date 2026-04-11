// Copyright Yamahasxviper. All Rights Reserved.

#include "PlayerWarningRegistry.h"
#include "BanSystemConfig.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogPlayerWarningRegistry);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerWarningRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FilePath = GetRegistryPath();

    const FString Dir = FPaths::GetPath(FilePath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    UE_LOG(LogPlayerWarningRegistry, Log,
        TEXT("PlayerWarningRegistry: loaded %s (%d warning(s))"),
        *FilePath, Warnings.Num());
}

void UPlayerWarningRegistry::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerWarningRegistry::AddWarning(const FString& Uid, const FString& PlayerName,
                                        const FString& Reason, const FString& WarnedBy)
{
    if (Uid.IsEmpty()) return;

    FScopeLock Lock(&Mutex);

    // Determine the next Id (max existing + 1).
    int64 NextId = 1;
    for (const FWarningEntry& W : Warnings)
        if (W.Id >= NextId) NextId = W.Id + 1;

    FWarningEntry Entry;
    Entry.Id         = NextId;
    Entry.Uid        = Uid;
    Entry.PlayerName = PlayerName;
    Entry.Reason     = Reason;
    Entry.WarnedBy   = WarnedBy;
    Entry.WarnDate   = FDateTime::UtcNow();

    Warnings.Add(Entry);
    SaveToFile();
}

TArray<FWarningEntry> UPlayerWarningRegistry::GetWarningsForUid(const FString& Uid) const
{
    FScopeLock Lock(&Mutex);

    TArray<FWarningEntry> Result;
    for (const FWarningEntry& W : Warnings)
    {
        if (W.Uid.Equals(Uid, ESearchCase::IgnoreCase))
            Result.Add(W);
    }
    return Result;
}

int32 UPlayerWarningRegistry::ClearWarningsForUid(const FString& Uid)
{
    FScopeLock Lock(&Mutex);

    const int32 Before = Warnings.Num();
    Warnings.RemoveAll([&Uid](const FWarningEntry& W)
    {
        return W.Uid.Equals(Uid, ESearchCase::IgnoreCase);
    });
    const int32 Removed = Before - Warnings.Num();

    if (Removed > 0)
        SaveToFile();

    return Removed;
}

TArray<FWarningEntry> UPlayerWarningRegistry::GetAllWarnings() const
{
    FScopeLock Lock(&Mutex);
    return Warnings;
}

int32 UPlayerWarningRegistry::GetWarningCount(const FString& Uid) const
{
    FScopeLock Lock(&Mutex);

    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    const int32 DecayDays = Cfg ? Cfg->WarnDecayDays : 0;
    const FDateTime DecayCutoff = (DecayDays > 0)
        ? FDateTime::UtcNow() - FTimespan::FromDays(static_cast<double>(DecayDays))
        : FDateTime::MinValue();

    int32 Count = 0;
    for (const FWarningEntry& W : Warnings)
    {
        if (!W.Uid.Equals(Uid, ESearchCase::IgnoreCase)) continue;
        if (W.IsExpired()) continue;
        if (DecayDays > 0 && W.WarnDate < DecayCutoff) continue;
        ++Count;
    }
    return Count;
}

void UPlayerWarningRegistry::AddWarning(const FString& Uid, const FString& PlayerName,
                                         const FString& Reason, const FString& WarnedBy,
                                         int32 ExpiryMinutes)
{
    AddWarning(Uid, PlayerName, Reason, WarnedBy, ExpiryMinutes, 1);
}

void UPlayerWarningRegistry::AddWarning(const FString& Uid, const FString& PlayerName,
                                         const FString& Reason, const FString& WarnedBy,
                                         int32 ExpiryMinutes, int32 Points)
{
    if (Uid.IsEmpty()) return;

    FScopeLock Lock(&Mutex);

    int64 NextId = 1;
    for (const FWarningEntry& W : Warnings)
        if (W.Id >= NextId) NextId = W.Id + 1;

    FWarningEntry Entry;
    Entry.Id         = NextId;
    Entry.Uid        = Uid;
    Entry.PlayerName = PlayerName;
    Entry.Reason     = Reason;
    Entry.WarnedBy   = WarnedBy;
    Entry.WarnDate   = FDateTime::UtcNow();
    Entry.Points     = FMath::Max(1, Points);
    if (ExpiryMinutes > 0)
    {
        Entry.bHasExpiry  = true;
        Entry.ExpireDate  = FDateTime::UtcNow() + FTimespan::FromMinutes(ExpiryMinutes);
    }

    Warnings.Add(Entry);
    SaveToFile();
}

int32 UPlayerWarningRegistry::GetWarningPoints(const FString& Uid) const
{
    FScopeLock Lock(&Mutex);

    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    const int32 DecayDays = Cfg ? Cfg->WarnDecayDays : 0;
    const FDateTime DecayCutoff = (DecayDays > 0)
        ? FDateTime::UtcNow() - FTimespan::FromDays(static_cast<double>(DecayDays))
        : FDateTime::MinValue();

    int32 TotalPoints = 0;
    for (const FWarningEntry& W : Warnings)
    {
        if (!W.Uid.Equals(Uid, ESearchCase::IgnoreCase)) continue;
        if (W.IsExpired()) continue;
        if (DecayDays > 0 && W.WarnDate < DecayCutoff) continue;
        TotalPoints += FMath::Max(1, W.Points);
    }
    return TotalPoints;
}

int32 UPlayerWarningRegistry::PruneExpiredWarnings()
{
    FScopeLock Lock(&Mutex);

    const int32 Before = Warnings.Num();
    Warnings.RemoveAll([](const FWarningEntry& W) -> bool
    {
        return W.bHasExpiry && W.IsExpired();
    });
    const int32 Removed = Before - Warnings.Num();

    if (Removed > 0)
    {
        SaveToFile();
        UE_LOG(LogPlayerWarningRegistry, Log,
            TEXT("PlayerWarningRegistry: pruned %d expired warning(s)."), Removed);
    }

    return Removed;
}

bool UPlayerWarningRegistry::DeleteWarningById(int64 Id){
    FScopeLock Lock(&Mutex);

    const int32 Before = Warnings.Num();
    Warnings.RemoveAll([Id](const FWarningEntry& W) { return W.Id == Id; });
    const bool bRemoved = Warnings.Num() < Before;

    if (bRemoved)
        SaveToFile();

    return bRemoved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerWarningRegistry::LoadFromFile()
{
    FScopeLock Lock(&Mutex);
    Warnings.Empty();

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
        return; // File doesn't exist yet — first run.

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogPlayerWarningRegistry, Warning,
            TEXT("PlayerWarningRegistry: failed to parse %s — starting empty"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("warnings"), Arr) && Arr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FWarningEntry Entry;
            (*ObjPtr)->TryGetNumberField(TEXT("id"),         Entry.Id);
            (*ObjPtr)->TryGetStringField(TEXT("uid"),        Entry.Uid);
            (*ObjPtr)->TryGetStringField(TEXT("playerName"), Entry.PlayerName);
            (*ObjPtr)->TryGetStringField(TEXT("reason"),     Entry.Reason);
            (*ObjPtr)->TryGetStringField(TEXT("warnedBy"),   Entry.WarnedBy);

            FString WarnDateStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("warnDate"), WarnDateStr))
                FDateTime::ParseIso8601(*WarnDateStr, Entry.WarnDate);

            (*ObjPtr)->TryGetBoolField(TEXT("hasExpiry"), Entry.bHasExpiry);
            FString ExpireDateStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("expireDate"), ExpireDateStr) && !ExpireDateStr.IsEmpty())
                FDateTime::ParseIso8601(*ExpireDateStr, Entry.ExpireDate);

            // Points (default 1 for records written before this feature).
            double PointsDbl = 1.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("points"), PointsDbl) && PointsDbl >= 1.0)
                Entry.Points = static_cast<int32>(PointsDbl);

            if (!Entry.Uid.IsEmpty())
                Warnings.Add(Entry);
        }
    }
}

bool UPlayerWarningRegistry::SaveToFile() const
{
    // Caller must already hold Mutex.
    TArray<TSharedPtr<FJsonValue>> WarningArr;
    WarningArr.Reserve(Warnings.Num());
    for (const FWarningEntry& W : Warnings)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),         static_cast<double>(W.Id));
        Obj->SetStringField(TEXT("uid"),        W.Uid);
        Obj->SetStringField(TEXT("playerName"), W.PlayerName);
        Obj->SetStringField(TEXT("reason"),     W.Reason);
        Obj->SetStringField(TEXT("warnedBy"),   W.WarnedBy);
        Obj->SetStringField(TEXT("warnDate"),   W.WarnDate.ToIso8601());
        Obj->SetBoolField  (TEXT("hasExpiry"),  W.bHasExpiry);
        Obj->SetStringField(TEXT("expireDate"), W.bHasExpiry ? W.ExpireDate.ToIso8601() : TEXT(""));
        Obj->SetNumberField(TEXT("points"),     static_cast<double>(W.Points > 0 ? W.Points : 1));
        WarningArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("warnings"), WarningArr);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogPlayerWarningRegistry, Error,
            TEXT("PlayerWarningRegistry: failed to serialize warnings"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
    {
        UE_LOG(LogPlayerWarningRegistry, Error,
            TEXT("PlayerWarningRegistry: failed to write %s"), *FilePath);
        return false;
    }
    return true;
}

FString UPlayerWarningRegistry::GetRegistryPath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    FString BaseDir;
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        BaseDir = FPaths::GetPath(Cfg->DatabasePath);
    else
        BaseDir = FPaths::ProjectSavedDir() / TEXT("BanSystem");

    return BaseDir / TEXT("warnings.json");
}
