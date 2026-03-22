#include "BanManager.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformFileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanManager, Log, All);

// ---------------------------------------------------------------------------
// Static member definitions
// ---------------------------------------------------------------------------
bool            FBanManager::bEnabled = true;
TArray<FString> FBanManager::Players;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

FString FBanManager::GetFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("ServerBanlist.json");
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

void FBanManager::Load(bool bDefaultEnabled)
{
	const FString FilePath = GetFilePath();

	if (!FPlatformFileManager::Get().GetPlatformFile().FileExists(*FilePath))
	{
		// First run: no file exists yet.  Use the caller-supplied default so
		// the BanSystemEnabled=True/False value from the INI takes effect.
		bEnabled = bDefaultEnabled;
		UE_LOG(LogBanManager, Display,
			TEXT("Ban list file not found — creating default at %s (enabled=%s)"),
			*FilePath, bEnabled ? TEXT("true") : TEXT("false"));
		Save();
		return;
	}

	FString RawJson;
	if (!FFileHelper::LoadFileToString(RawJson, *FilePath))
	{
		UE_LOG(LogBanManager, Error,
			TEXT("Failed to read ban list from %s"), *FilePath);
		return;
	}

	TSharedPtr<FJsonObject> Root;
	const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(RawJson);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogBanManager, Warning,
			TEXT("Ban list JSON is malformed — resetting to defaults"));
		bEnabled = bDefaultEnabled;
		Players.Empty();
		Save();
		return;
	}

	// The enabled/disabled state is always taken from the INI config (bDefaultEnabled),
	// so operators can toggle BanSystemEnabled in DefaultDiscordBridge.ini and it
	// takes effect on the next server restart without touching ServerBanlist.json.
	bEnabled = bDefaultEnabled;

	Players.Empty();
	if (Root->HasTypedField<EJson::Array>(TEXT("players")))
	{
		for (const TSharedPtr<FJsonValue>& Val : Root->GetArrayField(TEXT("players")))
		{
			if (Val->Type == EJson::String)
			{
				Players.AddUnique(Val->AsString().ToLower());
			}
		}
	}

	UE_LOG(LogBanManager, Display,
		TEXT("Ban list loaded: %s, %d player(s)"),
		bEnabled ? TEXT("ENABLED") : TEXT("disabled"),
		Players.Num());
}

void FBanManager::Save()
{
	const FString FilePath = GetFilePath();
	FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(FilePath));

	const TSharedRef<FJsonObject> Root = MakeShareable(new FJsonObject());
	Root->SetBoolField(TEXT("enabled"), bEnabled);

	TArray<TSharedPtr<FJsonValue>> PlayerArray;
	for (const FString& Name : Players)
	{
		PlayerArray.Add(MakeShareable(new FJsonValueString(Name)));
	}
	Root->SetArrayField(TEXT("players"), PlayerArray);

	FString OutJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(Root, Writer);

	if (!FFileHelper::SaveStringToFile(OutJson, *FilePath))
	{
		UE_LOG(LogBanManager, Error,
			TEXT("Failed to save ban list to %s"), *FilePath);
		return;
	}
	UE_LOG(LogBanManager, Display,
		TEXT("Ban list saved to %s"), *FilePath);
}

bool FBanManager::IsEnabled()
{
	return bEnabled;
}

void FBanManager::SetEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;
	Save();
}

bool FBanManager::IsBanned(const FString& PlayerName)
{
	return Players.Contains(PlayerName.ToLower());
}

bool FBanManager::BanPlayer(const FString& PlayerName)
{
	const FString Lower = PlayerName.ToLower();
	if (Players.Contains(Lower))
	{
		return false;
	}
	Players.Add(Lower);
	Save();
	return true;
}

bool FBanManager::UnbanPlayer(const FString& PlayerName)
{
	const int32 Removed = Players.Remove(PlayerName.ToLower());
	if (Removed > 0)
	{
		Save();
		return true;
	}
	return false;
}

TArray<FString> FBanManager::GetAll()
{
	return Players;
}
