// Copyright Yamahasxviper. All Rights Reserved.

#include "BanAppealRegistry.h"
#include "BanSystemConfig.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

DEFINE_LOG_CATEGORY(LogBanAppealRegistry);

// Static delegate definition.
UBanAppealRegistry::FOnBanAppealSubmitted UBanAppealRegistry::OnBanAppealSubmitted;
UBanAppealRegistry::FOnBanAppealReviewed  UBanAppealRegistry::OnBanAppealReviewed;

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanAppealRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FilePath = GetRegistryPath();

    const FString Dir = FPaths::GetPath(FilePath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    UE_LOG(LogBanAppealRegistry, Log,
        TEXT("BanAppealRegistry: loaded %s (%d appeal(s))"),
        *FilePath, Appeals.Num());
}

void UBanAppealRegistry::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

FBanAppealEntry UBanAppealRegistry::AddAppeal(const FString& Uid,
                                               const FString& Reason,
                                               const FString& ContactInfo)
{
    FBanAppealEntry Entry;

    {
        FScopeLock Lock(&Mutex);

        // Determine next Id (max existing + 1).
        int64 NextId = 1;
        for (const FBanAppealEntry& A : Appeals)
            if (A.Id >= NextId) NextId = A.Id + 1;

        Entry.Id          = NextId;
        Entry.Uid         = Uid;
        Entry.Reason      = Reason;
        Entry.ContactInfo = ContactInfo;
        Entry.SubmittedAt = FDateTime::UtcNow();

        Appeals.Add(Entry);
        SaveToFile();
    }

    // Broadcast outside the lock so listeners can safely call back into the registry.
    OnBanAppealSubmitted.Broadcast(Entry);

    return Entry;
}

TArray<FBanAppealEntry> UBanAppealRegistry::GetAllAppeals() const
{
    FScopeLock Lock(&Mutex);
    return Appeals;
}

FBanAppealEntry UBanAppealRegistry::GetAppealById(int64 Id) const
{
    FScopeLock Lock(&Mutex);
    for (const FBanAppealEntry& A : Appeals)
    {
        if (A.Id == Id)
            return A;
    }
    return FBanAppealEntry(); // Id == 0 signals "not found"
}

bool UBanAppealRegistry::DeleteAppeal(int64 Id)
{
    FScopeLock Lock(&Mutex);

    const int32 Before = Appeals.Num();
    Appeals.RemoveAll([Id](const FBanAppealEntry& A)
    {
        return A.Id == Id;
    });
    const bool bRemoved = Appeals.Num() < Before;
    if (bRemoved) SaveToFile();
    return bRemoved;
}

bool UBanAppealRegistry::ReviewAppeal(int64 Id, EAppealStatus NewStatus,
                                       const FString& ReviewedByName,
                                       const FString& ReviewNote)
{
    FBanAppealEntry ReviewedEntry;
    bool bFound = false;

    {
        FScopeLock Lock(&Mutex);
        for (FBanAppealEntry& A : Appeals)
        {
            if (A.Id != Id) continue;
            A.Status     = NewStatus;
            A.ReviewedBy = ReviewedByName;
            A.ReviewNote = ReviewNote;
            A.ReviewedAt = FDateTime::UtcNow();
            ReviewedEntry = A;
            bFound = true;
            break;
        }
        if (bFound)
            SaveToFile();
    }

    if (bFound)
        OnBanAppealReviewed.Broadcast(ReviewedEntry);

    return bFound;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

FString UBanAppealRegistry::GetRegistryPath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    FString BaseDir;
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        BaseDir = FPaths::GetPath(Cfg->DatabasePath);
    else
        BaseDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BanSystem"));

    return FPaths::Combine(BaseDir, TEXT("appeals.json"));
}

void UBanAppealRegistry::LoadFromFile()
{
    FScopeLock Lock(&Mutex);
    Appeals.Empty();

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
        return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogBanAppealRegistry, Warning,
            TEXT("BanAppealRegistry: failed to parse %s — starting empty"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("appeals"), Arr) && Arr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FBanAppealEntry Entry;
            double IdDbl = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("id"), IdDbl))
                Entry.Id = static_cast<int64>(IdDbl);
            (*ObjPtr)->TryGetStringField(TEXT("uid"),         Entry.Uid);
            (*ObjPtr)->TryGetStringField(TEXT("reason"),      Entry.Reason);
            (*ObjPtr)->TryGetStringField(TEXT("contactInfo"), Entry.ContactInfo);

            FString DateStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("submittedAt"), DateStr))
                FDateTime::ParseIso8601(*DateStr, Entry.SubmittedAt);

            // Status (default Pending for records written before this feature).
            FString StatusStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("status"), StatusStr))
            {
                if (StatusStr == TEXT("approved"))      Entry.Status = EAppealStatus::Approved;
                else if (StatusStr == TEXT("denied"))   Entry.Status = EAppealStatus::Denied;
                else                                    Entry.Status = EAppealStatus::Pending;
            }

            (*ObjPtr)->TryGetStringField(TEXT("reviewedBy"),  Entry.ReviewedBy);
            (*ObjPtr)->TryGetStringField(TEXT("reviewNote"),  Entry.ReviewNote);
            FString ReviewedAtStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("reviewedAt"), ReviewedAtStr) && !ReviewedAtStr.IsEmpty())
                FDateTime::ParseIso8601(*ReviewedAtStr, Entry.ReviewedAt);

            if (!Entry.Uid.IsEmpty())
                Appeals.Add(Entry);
        }
    }
}

bool UBanAppealRegistry::SaveToFile() const
{
    // Caller must already hold Mutex.
    TArray<TSharedPtr<FJsonValue>> AppealArr;
    AppealArr.Reserve(Appeals.Num());
    for (const FBanAppealEntry& A : Appeals)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),          static_cast<double>(A.Id));
        Obj->SetStringField(TEXT("uid"),         A.Uid);
        Obj->SetStringField(TEXT("reason"),      A.Reason);
        Obj->SetStringField(TEXT("contactInfo"), A.ContactInfo);
        Obj->SetStringField(TEXT("submittedAt"), A.SubmittedAt.ToIso8601());

        // Status
        FString StatusStr;
        switch (A.Status)
        {
        case EAppealStatus::Approved: StatusStr = TEXT("approved"); break;
        case EAppealStatus::Denied:   StatusStr = TEXT("denied");   break;
        default:                      StatusStr = TEXT("pending");  break;
        }
        Obj->SetStringField(TEXT("status"), StatusStr);

        if (!A.ReviewedBy.IsEmpty())
            Obj->SetStringField(TEXT("reviewedBy"), A.ReviewedBy);
        if (!A.ReviewNote.IsEmpty())
            Obj->SetStringField(TEXT("reviewNote"), A.ReviewNote);
        if (A.ReviewedAt != FDateTime(0))
            Obj->SetStringField(TEXT("reviewedAt"), A.ReviewedAt.ToIso8601());

        AppealArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("appeals"), AppealArr);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanAppealRegistry, Error,
            TEXT("BanAppealRegistry: failed to serialize appeals"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogBanAppealRegistry, Error,
            TEXT("BanAppealRegistry: failed to write %s"), *FilePath);
        return false;
    }

    return true;
}
