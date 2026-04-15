// Copyright Yamahasxviper. All Rights Reserved.

#include "WhitelistManager.h"
#include "Logging/LogMacros.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogWhitelistManager, Log, All);

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
bool                           FWhitelistManager::bEnabled  = false;
TArray<FWhitelistEntry>        FWhitelistManager::Entries;
TArray<FWhitelistAuditEntry>   FWhitelistManager::AuditLog;
int32                          FWhitelistManager::MaxSlots  = 0;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

FString FWhitelistManager::GetFilePath()
{
return FPaths::ProjectSavedDir() / TEXT("ServerWhitelist.json");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FWhitelistManager::Load(bool bDefaultEnabled)
{
const FString FilePath = GetFilePath();

if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
{
bEnabled = bDefaultEnabled;
UE_LOG(LogWhitelistManager, Display,
TEXT("Whitelist file not found — creating default at %s (enabled=%s)"),
*FilePath, bEnabled ? TEXT("true") : TEXT("false"));
Save();
return;
}

FString RawJson;
if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
{
UE_LOG(LogWhitelistManager, Error,
TEXT("Failed to read whitelist from %s"), *FilePath);
return;
}

TSharedPtr<FJsonObject> Root;
const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
{
UE_LOG(LogWhitelistManager, Warning,
TEXT("Whitelist JSON is malformed — resetting to defaults"));
bEnabled = bDefaultEnabled;
Entries.Empty();
Save();
return;
}

// Restore the runtime-toggled enabled state from JSON (overrides the ini default).
Root->TryGetBoolField(TEXT("enabled"), bEnabled);

// Load max_slots
double MaxSlotsD = 0.0;
if (Root->TryGetNumberField(TEXT("max_slots"), MaxSlotsD))
{
MaxSlots = static_cast<int32>(MaxSlotsD);
}

// Load players array (backward compat: string or object)
Entries.Empty();
if (Root->HasTypedField<EJson::Array>(TEXT("players")))
{
for (const TSharedPtr<FJsonValue>& Val : Root->GetArrayField(TEXT("players")))
{
if (Val->Type == EJson::String)
{
// Legacy: plain string
FWhitelistEntry E;
E.Name      = Val->AsString().ToLower();
E.EosPUID   = TEXT("");
E.ExpiresAt = FDateTime(0);
Entries.Add(E);
}
else if (Val->Type == EJson::Object)
{
const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
if (Val->TryGetObject(ObjPtr) && ObjPtr)
{
FWhitelistEntry E;
FString NameStr;
(*ObjPtr)->TryGetStringField(TEXT("name"), NameStr);
E.Name    = NameStr.ToLower();
(*ObjPtr)->TryGetStringField(TEXT("eos_puid"), E.EosPUID);
(*ObjPtr)->TryGetStringField(TEXT("group"), E.Group);
FString ExpiresStr;
(*ObjPtr)->TryGetStringField(TEXT("expires_at"), ExpiresStr);
if (ExpiresStr.IsEmpty())
{
E.ExpiresAt = FDateTime(0);
}
else
{
if (!FDateTime::ParseIso8601(*ExpiresStr, E.ExpiresAt))
{
E.ExpiresAt = FDateTime(0);
}
}
Entries.Add(E);
}
}
}
}

// Load audit log
AuditLog.Empty();
if (Root->HasTypedField<EJson::Array>(TEXT("audit_log")))
{
for (const TSharedPtr<FJsonValue>& Val : Root->GetArrayField(TEXT("audit_log")))
{
const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
if (!Val->TryGetObject(ObjPtr) || !ObjPtr) continue;

FWhitelistAuditEntry A;
FString TsStr;
(*ObjPtr)->TryGetStringField(TEXT("timestamp"), TsStr);
if (!TsStr.IsEmpty())
{
FDateTime::ParseIso8601(*TsStr, A.Timestamp);
}
(*ObjPtr)->TryGetStringField(TEXT("admin"),  A.AdminName);
(*ObjPtr)->TryGetStringField(TEXT("action"), A.Action);
(*ObjPtr)->TryGetStringField(TEXT("target"), A.Target);
AuditLog.Add(A);
}
}

UE_LOG(LogWhitelistManager, Display,
TEXT("Whitelist loaded: %s, %d player(s)"),
bEnabled ? TEXT("ENABLED") : TEXT("disabled"),
Entries.Num());
}

void FWhitelistManager::Save()
{
const FString FilePath = GetFilePath();
FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FilePath));

const TSharedRef<FJsonObject> Root = MakeShareable(new FJsonObject());
Root->SetBoolField(TEXT("enabled"), bEnabled);
Root->SetNumberField(TEXT("max_slots"), MaxSlots);

TArray<TSharedPtr<FJsonValue>> PlayerArray;
for (const FWhitelistEntry& E : Entries)
{
TSharedPtr<FJsonObject> EntryObj = MakeShared<FJsonObject>();
EntryObj->SetStringField(TEXT("name"), E.Name);
EntryObj->SetStringField(TEXT("eos_puid"), E.EosPUID);
EntryObj->SetStringField(TEXT("group"), E.Group);
FString ExpiresStr;
if (E.ExpiresAt.GetTicks() > 0)
{
ExpiresStr = E.ExpiresAt.ToIso8601();
}
EntryObj->SetStringField(TEXT("expires_at"), ExpiresStr);
PlayerArray.Add(MakeShared<FJsonValueObject>(EntryObj));
}
Root->SetArrayField(TEXT("players"), PlayerArray);

// Save audit log
TArray<TSharedPtr<FJsonValue>> AuditArray;
for (const FWhitelistAuditEntry& A : AuditLog)
{
TSharedPtr<FJsonObject> AObj = MakeShared<FJsonObject>();
AObj->SetStringField(TEXT("timestamp"), A.Timestamp.ToIso8601());
AObj->SetStringField(TEXT("admin"),     A.AdminName);
AObj->SetStringField(TEXT("action"),    A.Action);
AObj->SetStringField(TEXT("target"),    A.Target);
AuditArray.Add(MakeShared<FJsonValueObject>(AObj));
}
Root->SetArrayField(TEXT("audit_log"), AuditArray);

FString OutJson;
const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
FJsonSerializer::Serialize(Root, Writer);

if (!FFileHelper::SaveStringToFile(OutJson, *FilePath,
	FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
{
UE_LOG(LogWhitelistManager, Error,
TEXT("Failed to save whitelist to %s"), *FilePath);
return;
}
UE_LOG(LogWhitelistManager, Verbose,
TEXT("Whitelist saved to %s"), *FilePath);
}

bool FWhitelistManager::IsEnabled()
{
return bEnabled;
}

void FWhitelistManager::SetEnabled(bool bNewEnabled, const FString& AdminName)
{
bEnabled = bNewEnabled;
LogAudit(AdminName, bNewEnabled ? TEXT("enable") : TEXT("disable"), TEXT("whitelist"));
Save();
}

bool FWhitelistManager::IsWhitelisted(const FString& PlayerName, const FString& EosPUID)
{
const FString LowerName = PlayerName.ToLower();
const FDateTime Now = FDateTime::UtcNow();
for (const FWhitelistEntry& E : Entries)
{
// Skip expired entries
if (E.ExpiresAt.GetTicks() > 0 && E.ExpiresAt <= Now) continue;

if (E.Name == LowerName) return true;
if (!EosPUID.IsEmpty() && !E.EosPUID.IsEmpty() && E.EosPUID == EosPUID) return true;
}
return false;
}

bool FWhitelistManager::IsWhitelistedByPUID(const FString& EosPUID)
{
if (EosPUID.IsEmpty()) return false;
const FDateTime Now = FDateTime::UtcNow();
for (const FWhitelistEntry& E : Entries)
{
if (E.ExpiresAt.GetTicks() > 0 && E.ExpiresAt <= Now) continue;
if (!E.EosPUID.IsEmpty() && E.EosPUID == EosPUID) return true;
}
return false;
}

bool FWhitelistManager::AddPlayer(const FString& PlayerName,
                                   const FString& EosPUID,
                                   const FString& AdminName,
                                   FDateTime      ExpiresAt,
                                   const FString& Group)
{
// Capacity check
if (MaxSlots > 0 && Entries.Num() >= MaxSlots)
{
return false;
}

const FString LowerName = PlayerName.ToLower();

// Duplicate check (by name)
for (const FWhitelistEntry& E : Entries)
{
if (E.Name == LowerName) return false;
if (!EosPUID.IsEmpty() && !E.EosPUID.IsEmpty() && E.EosPUID == EosPUID) return false;
}

FWhitelistEntry NewEntry;
NewEntry.Name      = LowerName;
NewEntry.EosPUID   = EosPUID;
NewEntry.ExpiresAt = ExpiresAt;
NewEntry.Group     = Group;
Entries.Add(NewEntry);

LogAudit(AdminName, TEXT("add"), PlayerName);
Save();
return true;
}

bool FWhitelistManager::RemovePlayer(const FString& PlayerName, const FString& EosPUID, const FString& AdminName)
{
int32 RemovedIdx = INDEX_NONE;

if (!EosPUID.IsEmpty())
{
// Remove by PUID
for (int32 i = 0; i < Entries.Num(); ++i)
{
if (Entries[i].EosPUID == EosPUID)
{
RemovedIdx = i;
break;
}
}
}
else
{
const FString LowerName = PlayerName.ToLower();
for (int32 i = 0; i < Entries.Num(); ++i)
{
if (Entries[i].Name == LowerName)
{
RemovedIdx = i;
break;
}
}
}

if (RemovedIdx == INDEX_NONE) return false;

const FString RemovedName = Entries[RemovedIdx].Name;
Entries.RemoveAt(RemovedIdx);
LogAudit(AdminName, TEXT("remove"), RemovedName);
Save();
return true;
}

TArray<FString> FWhitelistManager::GetAll()
{
TArray<FString> Names;
for (const FWhitelistEntry& E : Entries)
{
Names.Add(E.Name);
}
return Names;
}

TArray<FWhitelistEntry> FWhitelistManager::GetAllEntries()
{
return Entries;
}

void FWhitelistManager::LogAudit(const FString& Admin, const FString& Action, const FString& Target)
{
FWhitelistAuditEntry Entry;
Entry.Timestamp = FDateTime::UtcNow();
Entry.AdminName = Admin;
Entry.Action    = Action;
Entry.Target    = Target;
AuditLog.Add(Entry);

// Keep max 100 entries
while (AuditLog.Num() > 100)
{
AuditLog.RemoveAt(0);
}
}

TArray<FWhitelistAuditEntry> FWhitelistManager::GetAuditLog(int32 MaxEntries)
{
if (MaxEntries <= 0) MaxEntries = 20;
if (MaxEntries > 20) MaxEntries = 20;

const int32 Total = AuditLog.Num();
if (Total <= MaxEntries)
{
return AuditLog;
}
TArray<FWhitelistAuditEntry> Result;
Result.Reserve(MaxEntries);
for (int32 i = Total - MaxEntries; i < Total; ++i)
{
Result.Add(AuditLog[i]);
}
return Result;
}

int32 FWhitelistManager::GetMaxSlots()
{
return MaxSlots;
}

void FWhitelistManager::SetMaxSlots(int32 InMaxSlots)
{
MaxSlots = InMaxSlots;
}

FTimespan FWhitelistManager::ParseDuration(const FString& DurStr)
{
if (DurStr.IsEmpty()) return FTimespan::Zero();
FString Lower = DurStr.ToLower().TrimStartAndEnd();
if (Lower.EndsWith(TEXT("w")))
return FTimespan::FromDays(FCString::Atof(*Lower.LeftChop(1)) * 7.0);
if (Lower.EndsWith(TEXT("d")))
return FTimespan::FromDays(FCString::Atof(*Lower.LeftChop(1)));
if (Lower.EndsWith(TEXT("h")))
return FTimespan::FromHours(FCString::Atof(*Lower.LeftChop(1)));
if (Lower.EndsWith(TEXT("m")))
return FTimespan::FromMinutes(FCString::Atof(*Lower.LeftChop(1)));
return FTimespan::Zero();
}

void FWhitelistManager::RemoveExpiredEntries(TArray<FString>& OutExpiredNames)
{
const FDateTime Now = FDateTime::UtcNow();
TArray<int32> ToRemove;
for (int32 i = 0; i < Entries.Num(); ++i)
{
const FWhitelistEntry& E = Entries[i];
if (E.ExpiresAt.GetTicks() > 0 && E.ExpiresAt <= Now)
{
OutExpiredNames.Add(E.Name);
ToRemove.Add(i);
}
}
if (ToRemove.Num() > 0)
{
// Remove in reverse order to preserve indices
for (int32 i = ToRemove.Num() - 1; i >= 0; --i)
{
Entries.RemoveAt(ToRemove[i]);
}
Save();
}
}

TArray<FWhitelistEntry> FWhitelistManager::Search(const FString& PartialName)
{
TArray<FWhitelistEntry> Result;
if (PartialName.IsEmpty()) return Result;
const FString Lower = PartialName.ToLower();
for (const FWhitelistEntry& E : Entries)
{
if (E.Name.Contains(Lower, ESearchCase::IgnoreCase))
{
Result.Add(E);
}
}
return Result;
}
