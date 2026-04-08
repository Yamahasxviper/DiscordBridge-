// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanChatCommands.h"
#include "BanSystemConfig.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerNoteRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    FilePath = GetRegistryPath();

    const FString Dir = FPaths::GetPath(FilePath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    UE_LOG(LogBanChatCommands, Log,
        TEXT("PlayerNoteRegistry: loaded %s (%d note(s))"),
        *FilePath, Notes.Num());
}

void UPlayerNoteRegistry::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

void UPlayerNoteRegistry::AddNote(const FString& Uid, const FString& PlayerName,
                                  const FString& Note, const FString& AddedBy)
{
    if (Uid.IsEmpty()) return;

    FScopeLock Lock(&Mutex);

    // Determine next Id (max existing + 1).
    int64 NextId = 1;
    for (const FPlayerNoteEntry& N : Notes)
        if (N.Id >= NextId) NextId = N.Id + 1;

    FPlayerNoteEntry Entry;
    Entry.Id         = NextId;
    Entry.Uid        = Uid;
    Entry.PlayerName = PlayerName;
    Entry.Note       = Note;
    Entry.AddedBy    = AddedBy;
    Entry.NoteDate   = FDateTime::UtcNow();

    Notes.Add(Entry);
    SaveToFile();
}

TArray<FPlayerNoteEntry> UPlayerNoteRegistry::GetNotesForUid(const FString& Uid) const
{
    FScopeLock Lock(&Mutex);
    TArray<FPlayerNoteEntry> Result;
    for (const FPlayerNoteEntry& N : Notes)
    {
        if (N.Uid.Equals(Uid, ESearchCase::IgnoreCase))
            Result.Add(N);
    }
    return Result;
}

TArray<FPlayerNoteEntry> UPlayerNoteRegistry::GetAllNotes() const
{
    FScopeLock Lock(&Mutex);
    return Notes;
}

bool UPlayerNoteRegistry::DeleteNote(int64 Id)
{
    FScopeLock Lock(&Mutex);
    const int32 Before = Notes.Num();
    Notes.RemoveAll([Id](const FPlayerNoteEntry& N)
    {
        return N.Id == Id;
    });
    const bool bRemoved = Notes.Num() < Before;
    if (bRemoved) SaveToFile();
    return bRemoved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

FString UPlayerNoteRegistry::GetRegistryPath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    FString BaseDir;
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        BaseDir = FPaths::GetPath(Cfg->DatabasePath);
    else
        BaseDir = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("BanSystem"));

    return FPaths::Combine(BaseDir, TEXT("notes.json"));
}

void UPlayerNoteRegistry::LoadFromFile()
{
    FScopeLock Lock(&Mutex);
    Notes.Empty();

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
        return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogBanChatCommands, Warning,
            TEXT("PlayerNoteRegistry: failed to parse %s — starting empty"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("notes"), Arr) && Arr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FPlayerNoteEntry Entry;
            double IdDbl = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("id"), IdDbl))
                Entry.Id = static_cast<int64>(IdDbl);
            (*ObjPtr)->TryGetStringField(TEXT("uid"),        Entry.Uid);
            (*ObjPtr)->TryGetStringField(TEXT("playerName"), Entry.PlayerName);
            (*ObjPtr)->TryGetStringField(TEXT("note"),       Entry.Note);
            (*ObjPtr)->TryGetStringField(TEXT("addedBy"),    Entry.AddedBy);

            FString DateStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("noteDate"), DateStr))
                FDateTime::ParseIso8601(*DateStr, Entry.NoteDate);

            if (!Entry.Uid.IsEmpty())
                Notes.Add(Entry);
        }
    }
}

bool UPlayerNoteRegistry::SaveToFile() const
{
    // Caller must already hold Mutex.
    TArray<TSharedPtr<FJsonValue>> NoteArr;
    NoteArr.Reserve(Notes.Num());
    for (const FPlayerNoteEntry& N : Notes)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),         static_cast<double>(N.Id));
        Obj->SetStringField(TEXT("uid"),        N.Uid);
        Obj->SetStringField(TEXT("playerName"), N.PlayerName);
        Obj->SetStringField(TEXT("note"),       N.Note);
        Obj->SetStringField(TEXT("addedBy"),    N.AddedBy);
        Obj->SetStringField(TEXT("noteDate"),   N.NoteDate.ToIso8601());
        NoteArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("notes"), NoteArr);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogBanChatCommands, Error,
            TEXT("PlayerNoteRegistry: failed to serialize notes"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath))
    {
        UE_LOG(LogBanChatCommands, Error,
            TEXT("PlayerNoteRegistry: failed to write %s"), *FilePath);
        return false;
    }

    return true;
}
