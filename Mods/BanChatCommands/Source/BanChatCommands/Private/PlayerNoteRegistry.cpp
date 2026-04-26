// Copyright Yamahasxviper. All Rights Reserved.

#include "PlayerNoteRegistry.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "HAL/FileManager.h"

DEFINE_LOG_CATEGORY(LogPlayerNoteRegistry);

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

    UE_LOG(LogPlayerNoteRegistry, Log,
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

    FPlayerNoteEntry Entry;
    if (NextId == INT64_MAX)
    {
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: NextId has reached INT64_MAX — cannot add more notes"));
        return;
    }
    Entry.Id         = NextId++;
    Entry.Uid        = Uid;
    Entry.PlayerName = PlayerName;
    Entry.Note       = Note;
    Entry.AddedBy    = AddedBy;
    Entry.NoteDate   = FDateTime::UtcNow();

    Notes.Add(Entry);
    if (!SaveToFile())
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: failed to save notes.json after adding note for %s"), *Uid);
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
    if (bRemoved && !SaveToFile())
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: failed to save notes.json after deleting note id=%lld"), Id);
    return bRemoved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

FString UPlayerNoteRegistry::GetRegistryPath() const
{
    return FPaths::ProjectSavedDir() / TEXT("BanChatCommands") / TEXT("notes.json");
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
        UE_LOG(LogPlayerNoteRegistry, Warning,
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
            {
                if (!FDateTime::ParseIso8601(*DateStr, Entry.NoteDate))
                {
                    UE_LOG(LogPlayerNoteRegistry, Warning,
                        TEXT("PlayerNoteRegistry: uid='%s' has malformed noteDate '%s' — skipping entry"),
                        *Entry.Uid, *DateStr);
                    continue;
                }
            }

            if (!Entry.Uid.IsEmpty())
                Notes.Add(Entry);
        }
    }

    // Restore the O(1) counter from loaded data so AddNote never reuses an Id.
    NextId = 1;
    for (const FPlayerNoteEntry& N : Notes)
        if (N.Id >= NextId) NextId = N.Id + 1;
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
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: failed to serialize notes"));
        return false;
    }

    const FString TmpPath = FilePath + TEXT(".tmp");
    if (!FFileHelper::SaveStringToFile(JsonStr, *TmpPath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: failed to write temp file %s"), *TmpPath);
        return false;
    }
    if (!IFileManager::Get().Move(*FilePath, *TmpPath, /*bReplace=*/true))
    {
        UE_LOG(LogPlayerNoteRegistry, Error,
            TEXT("PlayerNoteRegistry: failed to replace %s with temp file"), *FilePath);
        return false;
    }

    return true;
}
