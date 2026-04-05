// Copyright Yamahasxviper. All Rights Reserved.

#include "PlayerSessionRegistry.h"
#include "BanSystemConfig.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogPlayerSessionRegistry);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerSessionRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FilePath = GetRegistryPath();

    const FString Dir = FPaths::GetPath(FilePath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    UE_LOG(LogPlayerSessionRegistry, Log,
        TEXT("PlayerSessionRegistry: loaded %s (%d record(s))"),
        *FilePath, Records.Num());
}

void UPlayerSessionRegistry::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerSessionRegistry::RecordSession(const FString& Uid, const FString& DisplayName, const FString& IpAddress)
{
    if (Uid.IsEmpty()) return;

    const FString NowStr = FDateTime::UtcNow().ToIso8601();

    FScopeLock Lock(&Mutex);

    // Update existing record if the UID already exists.
    for (FPlayerSessionRecord& R : Records)
    {
        if (R.Uid.Equals(Uid, ESearchCase::IgnoreCase))
        {
            R.DisplayName = DisplayName;
            R.LastSeen    = NowStr;
            if (!IpAddress.IsEmpty())
                R.IpAddress = IpAddress;
            SaveToFile();
            return;
        }
    }

    // New UID — append.
    FPlayerSessionRecord NewRec;
    NewRec.Uid         = Uid;
    NewRec.DisplayName = DisplayName;
    NewRec.LastSeen    = NowStr;
    NewRec.IpAddress   = IpAddress;
    Records.Add(NewRec);
    SaveToFile();
}

TArray<FPlayerSessionRecord> UPlayerSessionRegistry::FindByName(const FString& NameSubstring) const
{
    FScopeLock Lock(&Mutex);

    TArray<FPlayerSessionRecord> Result;
    for (const FPlayerSessionRecord& R : Records)
    {
        if (R.DisplayName.Contains(NameSubstring, ESearchCase::IgnoreCase))
            Result.Add(R);
    }
    return Result;
}

bool UPlayerSessionRegistry::FindByUid(const FString& Uid, FPlayerSessionRecord& OutRecord) const
{
    FScopeLock Lock(&Mutex);

    for (const FPlayerSessionRecord& R : Records)
    {
        if (R.Uid.Equals(Uid, ESearchCase::IgnoreCase))
        {
            OutRecord = R;
            return true;
        }
    }
    return false;
}

TArray<FPlayerSessionRecord> UPlayerSessionRegistry::GetAllRecords() const
{
    FScopeLock Lock(&Mutex);
    return Records;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerSessionRegistry::LoadFromFile()
{
    FScopeLock Lock(&Mutex);
    Records.Empty();

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
        return; // File doesn't exist yet — first run.

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogPlayerSessionRegistry, Warning,
            TEXT("PlayerSessionRegistry: failed to parse %s — starting empty"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("sessions"), Arr) && Arr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FPlayerSessionRecord Rec;
            (*ObjPtr)->TryGetStringField(TEXT("uid"),         Rec.Uid);
            (*ObjPtr)->TryGetStringField(TEXT("displayName"), Rec.DisplayName);
            (*ObjPtr)->TryGetStringField(TEXT("lastSeen"),    Rec.LastSeen);
            (*ObjPtr)->TryGetStringField(TEXT("ipAddress"),   Rec.IpAddress);

            if (!Rec.Uid.IsEmpty())
                Records.Add(Rec);
        }
    }
}

bool UPlayerSessionRegistry::SaveToFile() const
{
    // Caller must already hold Mutex.
    TArray<TSharedPtr<FJsonValue>> SessionArr;
    SessionArr.Reserve(Records.Num());
    for (const FPlayerSessionRecord& R : Records)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("uid"),         R.Uid);
        Obj->SetStringField(TEXT("displayName"), R.DisplayName);
        Obj->SetStringField(TEXT("lastSeen"),    R.LastSeen);
        if (!R.IpAddress.IsEmpty())
            Obj->SetStringField(TEXT("ipAddress"), R.IpAddress);
        SessionArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("sessions"), SessionArr);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogPlayerSessionRegistry, Error,
            TEXT("PlayerSessionRegistry: failed to serialize sessions"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
    {
        UE_LOG(LogPlayerSessionRegistry, Error,
            TEXT("PlayerSessionRegistry: failed to write %s"), *FilePath);
        return false;
    }
    return true;
}

FString UPlayerSessionRegistry::GetRegistryPath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    FString BaseDir;
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        BaseDir = FPaths::GetPath(Cfg->DatabasePath);
    else
        BaseDir = FPaths::ProjectSavedDir() / TEXT("BanSystem");

    return BaseDir / TEXT("player_sessions.json");
}
