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
TArray<FString> FBanManager::PlatformIds;

// ---------------------------------------------------------------------------
// Platform ID type detection and normalization
// ---------------------------------------------------------------------------

bool FBanManager::IsValidSteam64Id(const FString& Id)
{
	// Steam64 IDs are exactly 17 decimal digits and start with "765".
	// The Steam ID 64-bit base value ensures all valid Steam64 IDs begin with "765".
	if (Id.Len() != 17 || !Id.StartsWith(TEXT("765")))
	{
		return false;
	}
	for (const TCHAR Ch : Id)
	{
		if (!FChar::IsDigit(Ch))
		{
			return false;
		}
	}
	return true;
}

FString FBanManager::NormalizePlatformId(const FString& RawId)
{
	const FString Id = RawId.TrimStartAndEnd();
	if (Id.IsEmpty())
	{
		return Id;
	}

	// Only attempt header-format parsing when the entire string is valid hex.
	// Steam64 IDs contain decimal digits that are also valid hex, so we must
	// check the length and prefix BEFORE calling IsValidSteam64Id to avoid
	// falsely interpreting a decimal Steam64 ID as a header value.
	const FString Lower = Id.ToLower();
	bool bAllHex = (Lower.Len() > 0);
	for (const TCHAR Ch : Lower)
	{
		if (!FChar::IsDigit(Ch) && (Ch < TEXT('a') || Ch > TEXT('f')))
		{
			bAllHex = false;
			break;
		}
	}

	if (!bAllHex)
	{
		// Not a pure hex string (e.g. contains non-hex chars).
		// Pass through unchanged.
		return Id;
	}

	// ── Steam header: "06" + 16 hex chars = 18 chars total ───────────────────
	// The type byte 0x06 maps to EOnlineServices::Steam in Unreal Engine.
	// The 16 hex chars represent a big-endian uint64 SteamID64.
	// We convert those 16 hex chars to a decimal string and validate that it
	// looks like a real Steam64 ID (starts with "765").
	if (Lower.Len() == 18 && Lower.StartsWith(TEXT("06")))
	{
		// Parse the 8-byte big-endian uint64 from the 16 hex nibbles.
		uint64 SteamId64 = 0;
		const FString HexBytes = Lower.Mid(2, 16);
		for (const TCHAR Ch : HexBytes)
		{
			uint64 Nibble = 0;
			if (Ch >= TEXT('0') && Ch <= TEXT('9'))      { Nibble = static_cast<uint64>(Ch - TEXT('0')); }
			else if (Ch >= TEXT('a') && Ch <= TEXT('f')) { Nibble = 10 + static_cast<uint64>(Ch - TEXT('a')); }
			SteamId64 = (SteamId64 << 4) | Nibble;
		}

		// All valid Steam64 IDs start with "765" in their decimal representation.
		const FString Decimal = FString::Printf(TEXT("%llu"), SteamId64);
		if (IsValidSteam64Id(Decimal))
		{
			UE_LOG(LogBanManager, Verbose,
				TEXT("NormalizePlatformId: converted Steam X-FactoryGame-PlayerId header "
				     "'%s' → Steam64 '%s'"),
				*Id, *Decimal);
			return Decimal;
		}
		// Decodes to a non-Steam64 value – fall through and return unchanged.
	}

	// ── EOS/Epic header: "01" + 32 hex chars = 34 chars total ───────────────
	// The type byte 0x01 maps to EOnlineServices::Epic in Unreal Engine.
	// The 32 hex chars are the raw 16-byte EOS ProductUserId encoded as hex.
	// The canonical EOS PUID used by the ban system is exactly those 32 hex chars
	// (without the "01" type prefix).
	if (Lower.Len() == 34 && Lower.StartsWith(TEXT("01")))
	{
		const FString Puid = Lower.Mid(2, 32);
		UE_LOG(LogBanManager, Verbose,
			TEXT("NormalizePlatformId: converted EOS X-FactoryGame-PlayerId header "
			     "'%s' → EOS PUID '%s'"),
			*Id, *Puid);
		return Puid;
	}

	// Not a recognised X-FactoryGame-PlayerId header format – pass through.
	return Id;
}

FBanManager::EPlatformIdType FBanManager::GetPlatformIdType(const FString& PlatformId)
{
	const FString Id = PlatformId.TrimStartAndEnd();
	if (Id.IsEmpty())
	{
		return EPlatformIdType::Unknown;
	}

	// Steam64 ID: exactly 17 decimal digits starting with "765".
	if (IsValidSteam64Id(Id))
	{
		return EPlatformIdType::Steam;
	}

	// EOS PUID: composed entirely of hex digits (0-9, a-f).
	// We treat any non-Steam, all-hex string as an EOS PUID because that is
	// the only other platform ID format used in CSS UnrealEngine-CSS.
	const FString Lower = Id.ToLower();
	bool bAllHex = (Lower.Len() > 0);
	for (const TCHAR Ch : Lower)
	{
		if (!FChar::IsDigit(Ch) && (Ch < TEXT('a') || Ch > TEXT('f')))
		{
			bAllHex = false;
			break;
		}
	}
	if (bAllHex)
	{
		return EPlatformIdType::Epic;
	}

	return EPlatformIdType::Unknown;
}

FString FBanManager::GetPlatformTypeLabel(const FString& PlatformId)
{
	switch (GetPlatformIdType(PlatformId))
	{
		case EPlatformIdType::Steam: return TEXT("Steam");
		case EPlatformIdType::Epic:  return TEXT("EOS PUID");
		default:                     return TEXT("Unknown");
	}
}

TArray<FString> FBanManager::GetPlatformIdsByType(EPlatformIdType Type)
{
	TArray<FString> Result;
	for (const FString& Id : PlatformIds)
	{
		if (GetPlatformIdType(Id) == Type)
		{
			Result.Add(Id);
		}
	}
	return Result;
}

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
		PlatformIds.Empty();
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

	PlatformIds.Empty();
	if (Root->HasTypedField<EJson::Array>(TEXT("platform_ids")))
	{
		for (const TSharedPtr<FJsonValue>& Val : Root->GetArrayField(TEXT("platform_ids")))
		{
			if (Val->Type == EJson::String)
			{
				PlatformIds.AddUnique(Val->AsString().ToLower());
			}
		}
	}

	UE_LOG(LogBanManager, Display,
		TEXT("Ban list loaded: %s, %d player name(s), %d platform ID(s)"),
		bEnabled ? TEXT("ENABLED") : TEXT("disabled"),
		Players.Num(), PlatformIds.Num());
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

	TArray<TSharedPtr<FJsonValue>> PlatformIdArray;
	for (const FString& Id : PlatformIds)
	{
		PlatformIdArray.Add(MakeShareable(new FJsonValueString(Id)));
	}
	Root->SetArrayField(TEXT("platform_ids"), PlatformIdArray);

	FString OutJson;
	const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&OutJson);
	FJsonSerializer::Serialize(Root, Writer);

	if (!FFileHelper::SaveStringToFile(OutJson, *FilePath, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
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

bool FBanManager::IsPlatformIdBanned(const FString& PlatformId)
{
	return PlatformIds.Contains(NormalizePlatformId(PlatformId).ToLower());
}

bool FBanManager::BanPlatformId(const FString& PlatformId)
{
	const FString Lower = NormalizePlatformId(PlatformId).ToLower();
	if (PlatformIds.Contains(Lower))
	{
		return false;
	}
	PlatformIds.Add(Lower);
	Save();
	return true;
}

bool FBanManager::UnbanPlatformId(const FString& PlatformId)
{
	const int32 Removed = PlatformIds.Remove(NormalizePlatformId(PlatformId).ToLower());
	if (Removed > 0)
	{
		Save();
		return true;
	}
	return false;
}

TArray<FString> FBanManager::GetAllPlatformIds()
{
	return PlatformIds;
}
