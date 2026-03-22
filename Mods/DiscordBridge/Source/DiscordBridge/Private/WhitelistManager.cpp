#include "WhitelistManager.h"
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
bool          FWhitelistManager::bEnabled = false;
TArray<FString> FWhitelistManager::Players;

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
		// First run: no file exists yet.  Use the caller-supplied default so
		// the WhitelistEnabled=True/False value from the INI takes effect.
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
		Players.Empty();
		Save();
		return;
	}

	// The enabled/disabled state is always taken from the INI config (bDefaultEnabled),
	// so operators can toggle WhitelistEnabled in DefaultDiscordBridge.ini and it
	// takes effect on the next server restart without touching ServerWhitelist.json.
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

	UE_LOG(LogWhitelistManager, Display,
		TEXT("Whitelist loaded: %s, %d player(s)"),
		bEnabled ? TEXT("ENABLED") : TEXT("disabled"),
		Players.Num());
}

void FWhitelistManager::Save()
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
		UE_LOG(LogWhitelistManager, Error,
			TEXT("Failed to save whitelist to %s"), *FilePath);
		return;
	}
	UE_LOG(LogWhitelistManager, Display,
		TEXT("Whitelist saved to %s"), *FilePath);
}

bool FWhitelistManager::IsEnabled()
{
	return bEnabled;
}

void FWhitelistManager::SetEnabled(bool bNewEnabled)
{
	bEnabled = bNewEnabled;
	Save();
}

bool FWhitelistManager::IsWhitelisted(const FString& PlayerName)
{
	return Players.Contains(PlayerName.ToLower());
}

bool FWhitelistManager::AddPlayer(const FString& PlayerName)
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

bool FWhitelistManager::RemovePlayer(const FString& PlayerName)
{
	const int32 Removed = Players.Remove(PlayerName.ToLower());
	if (Removed > 0)
	{
		Save();
		return true;
	}
	return false;
}

TArray<FString> FWhitelistManager::GetAll()
{
	return Players;
}
