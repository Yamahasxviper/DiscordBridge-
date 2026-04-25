// Copyright Yamahasxviper. All Rights Reserved.

#include "ScheduledBanRegistry.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanSystemConfig.h"
#include "BanAuditLog.h"
#include "BanDiscordNotifier.h"

#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/PlatformFileManager.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY(LogScheduledBanRegistry);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UScheduledBanRegistry::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Super::Initialize(Collection);

    FilePath = GetRegistryPath();

    const FString Dir = FPaths::GetPath(FilePath);
    IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
    if (!PF.DirectoryExists(*Dir))
        PF.CreateDirectoryTree(*Dir);

    LoadFromFile();

    UE_LOG(LogScheduledBanRegistry, Log,
        TEXT("ScheduledBanRegistry: loaded %s (%d pending scheduled ban(s))"),
        *FilePath, Pending.Num());
}

void UScheduledBanRegistry::Deinitialize()
{
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Public API
// ─────────────────────────────────────────────────────────────────────────────

FScheduledBanEntry UScheduledBanRegistry::AddScheduled(
    const FString& Uid, const FString& PlayerName, const FString& Reason,
    const FString& ScheduledBy, FDateTime EffectiveAt, int32 DurationMinutes,
    const FString& Category)
{
    FScopeLock Lock(&Mutex);

    int64 NextId = 1;
    for (const FScheduledBanEntry& E : Pending)
        if (E.Id >= NextId) NextId = E.Id + 1;

    FScheduledBanEntry Entry;
    Entry.Id              = NextId;
    Entry.Uid             = Uid;
    Entry.PlayerName      = PlayerName;
    Entry.Reason          = Reason;
    Entry.ScheduledBy     = ScheduledBy;
    Entry.EffectiveAt     = EffectiveAt;
    Entry.CreatedAt       = FDateTime::UtcNow();
    Entry.DurationMinutes = DurationMinutes;
    Entry.Category        = Category;

    Pending.Add(Entry);
    SaveToFile();

    UE_LOG(LogScheduledBanRegistry, Log,
        TEXT("ScheduledBanRegistry: scheduled ban #%lld for %s at %s"),
        Entry.Id, *Uid, *EffectiveAt.ToIso8601());

    return Entry;
}

TArray<FScheduledBanEntry> UScheduledBanRegistry::GetAllPending() const
{
    FScopeLock Lock(&Mutex);
    return Pending;
}

bool UScheduledBanRegistry::DeleteScheduled(int64 Id)
{
    FScopeLock Lock(&Mutex);

    const int32 Before = Pending.Num();
    Pending.RemoveAll([Id](const FScheduledBanEntry& E) { return E.Id == Id; });
    const bool bRemoved = Pending.Num() < Before;
    if (bRemoved) SaveToFile();
    return bRemoved;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Tick
// ─────────────────────────────────────────────────────────────────────────────

void UScheduledBanRegistry::Tick(float DeltaTime)
{
    AccumulatedSeconds += DeltaTime;
    if (AccumulatedSeconds < TickIntervalSeconds) return;
    AccumulatedSeconds = 0.0f;

    const FDateTime Now = FDateTime::UtcNow();
    TArray<FScheduledBanEntry> Due;

    {
        FScopeLock Lock(&Mutex);
        for (int32 i = Pending.Num() - 1; i >= 0; --i)
        {
            if (Pending[i].EffectiveAt <= Now)
            {
                Due.Add(Pending[i]);
                Pending.RemoveAt(i);
            }
        }
        if (!Due.IsEmpty())
            SaveToFile();
    }

    for (const FScheduledBanEntry& S : Due)
        ApplyScheduledBan(S);
}

void UScheduledBanRegistry::ApplyScheduledBan(const FScheduledBanEntry& Entry)
{
    UGameInstance* GI = GetGameInstance();
    if (!GI) return;

    UBanDatabase* DB = GI->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    FBanEntry Ban;
    Ban.Uid        = Entry.Uid;
    UBanDatabase::ParseUid(Entry.Uid, Ban.Platform, Ban.PlayerUID);
    Ban.PlayerName      = Entry.PlayerName;
    Ban.Reason          = Entry.Reason;
    Ban.BannedBy        = Entry.ScheduledBy;
    const FDateTime Now = FDateTime::UtcNow();
    Ban.BanDate         = Now;
    Ban.Category        = Entry.Category;
    Ban.bIsPermanent    = (Entry.DurationMinutes <= 0);
    Ban.ExpireDate      = Ban.bIsPermanent
        ? FDateTime(0)
        : Now + FTimespan::FromMinutes(Entry.DurationMinutes);

    if (!DB->AddBan(Ban))
    {
        UE_LOG(LogScheduledBanRegistry, Warning,
            TEXT("ScheduledBanRegistry: failed to apply scheduled ban #%lld for %s"),
            Entry.Id, *Entry.Uid);
        return;
    }

    // Kick the player immediately if online.
    if (UWorld* World = GI->GetWorld())
        UBanEnforcer::KickConnectedPlayer(World, Entry.Uid, Ban.GetKickMessage());

    FBanDiscordNotifier::NotifyBanCreated(Ban);
    if (UBanAuditLog* AuditLog = GI->GetSubsystem<UBanAuditLog>())
        AuditLog->LogAction(TEXT("ban"), Entry.Uid, Entry.PlayerName,
            Entry.ScheduledBy, Entry.ScheduledBy,
            FString::Printf(TEXT("Scheduled ban applied. Reason: %s"), *Entry.Reason));

    UE_LOG(LogScheduledBanRegistry, Log,
        TEXT("ScheduledBanRegistry: applied scheduled ban #%lld for %s"),
        Entry.Id, *Entry.Uid);
}

// ─────────────────────────────────────────────────────────────────────────────
//  File I/O
// ─────────────────────────────────────────────────────────────────────────────

void UScheduledBanRegistry::LoadFromFile()
{
    // Always hold the mutex regardless of calling context (Initialize or reload).
    FScopeLock Lock(&Mutex);
    Pending.Empty();

    FString RawJson;
    if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
        return;

    TSharedPtr<FJsonObject> Root;
    TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
    if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
    {
        UE_LOG(LogScheduledBanRegistry, Warning,
            TEXT("ScheduledBanRegistry: failed to parse %s — starting empty"), *FilePath);
        return;
    }

    const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
    if (Root->TryGetArrayField(TEXT("scheduled"), Arr) && Arr)
    {
        for (const TSharedPtr<FJsonValue>& Val : *Arr)
        {
            if (!Val.IsValid()) continue;
            const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
            if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

            FScheduledBanEntry Entry;
            double IdDbl = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("id"), IdDbl))
                Entry.Id = static_cast<int64>(IdDbl);

            (*ObjPtr)->TryGetStringField(TEXT("uid"),          Entry.Uid);
            (*ObjPtr)->TryGetStringField(TEXT("playerName"),   Entry.PlayerName);
            (*ObjPtr)->TryGetStringField(TEXT("reason"),       Entry.Reason);
            (*ObjPtr)->TryGetStringField(TEXT("scheduledBy"),  Entry.ScheduledBy);
            (*ObjPtr)->TryGetStringField(TEXT("category"),     Entry.Category);

            double DurDbl = 0.0;
            if ((*ObjPtr)->TryGetNumberField(TEXT("durationMinutes"), DurDbl))
                Entry.DurationMinutes = static_cast<int32>(DurDbl);

            FString EffStr, CreatedStr;
            if ((*ObjPtr)->TryGetStringField(TEXT("effectiveAt"), EffStr))
                FDateTime::ParseIso8601(*EffStr, Entry.EffectiveAt);
            if ((*ObjPtr)->TryGetStringField(TEXT("createdAt"), CreatedStr))
                FDateTime::ParseIso8601(*CreatedStr, Entry.CreatedAt);

            if (!Entry.Uid.IsEmpty())
                Pending.Add(Entry);
        }
    }
}

bool UScheduledBanRegistry::SaveToFile() const
{
    // Caller must hold Mutex.
    TArray<TSharedPtr<FJsonValue>> Arr;
    Arr.Reserve(Pending.Num());
    for (const FScheduledBanEntry& E : Pending)
    {
        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetNumberField(TEXT("id"),              static_cast<double>(E.Id));
        Obj->SetStringField(TEXT("uid"),             E.Uid);
        Obj->SetStringField(TEXT("playerName"),      E.PlayerName);
        Obj->SetStringField(TEXT("reason"),          E.Reason);
        Obj->SetStringField(TEXT("scheduledBy"),     E.ScheduledBy);
        Obj->SetStringField(TEXT("category"),        E.Category);
        Obj->SetNumberField(TEXT("durationMinutes"), static_cast<double>(E.DurationMinutes));
        Obj->SetStringField(TEXT("effectiveAt"),     E.EffectiveAt.ToIso8601());
        Obj->SetStringField(TEXT("createdAt"),       E.CreatedAt.ToIso8601());
        Arr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetArrayField(TEXT("scheduled"), Arr);

    FString JsonStr;
    TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonStr);
    if (!FJsonSerializer::Serialize(Root.ToSharedRef(), Writer))
    {
        UE_LOG(LogScheduledBanRegistry, Error,
            TEXT("ScheduledBanRegistry: failed to serialize"));
        return false;
    }

    if (!FFileHelper::SaveStringToFile(JsonStr, *FilePath,
        FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
    {
        UE_LOG(LogScheduledBanRegistry, Error,
            TEXT("ScheduledBanRegistry: failed to write %s"), *FilePath);
        return false;
    }
    return true;
}

FString UScheduledBanRegistry::GetRegistryPath() const
{
    const UBanSystemConfig* Cfg = UBanSystemConfig::Get();
    FString BaseDir;
    if (Cfg && !Cfg->DatabasePath.IsEmpty())
        BaseDir = FPaths::GetPath(Cfg->DatabasePath);
    else
        BaseDir = FPaths::ProjectSavedDir() / TEXT("BanSystem");

    return BaseDir / TEXT("scheduled_bans.json");
}
