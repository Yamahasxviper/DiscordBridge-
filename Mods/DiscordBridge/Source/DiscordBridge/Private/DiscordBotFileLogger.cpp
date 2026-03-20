// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBotFileLogger.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

// ─────────────────────────────────────────────────────────────────────────────
// Log categories captured by this device
// ─────────────────────────────────────────────────────────────────────────────

// These must match the FName values used at the call sites.  They are compared
// by FName equality (case-insensitive, hash-based) so the string literals here
// need only match case-insensitively.
static const FName NAME_LogDiscordBridge   (TEXT("LogDiscordBridge"));
static const FName NAME_LogSMLWebSocket    (TEXT("LogSMLWebSocket"));
static const FName NAME_LogBanManager      (TEXT("LogBanManager"));
static const FName NAME_LogWhitelistManager(TEXT("LogWhitelistManager"));

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

/** Map ELogVerbosity to a short string without pulling in extra UE headers. */
static const TCHAR* VerbosityToString(ELogVerbosity::Type Verbosity)
{
	switch (Verbosity & ELogVerbosity::VerbosityMask)
	{
	case ELogVerbosity::Fatal:       return TEXT("Fatal");
	case ELogVerbosity::Error:       return TEXT("Error");
	case ELogVerbosity::Warning:     return TEXT("Warning");
	case ELogVerbosity::Display:     return TEXT("Display");
	case ELogVerbosity::Log:         return TEXT("Log");
	case ELogVerbosity::Verbose:     return TEXT("Verbose");
	case ELogVerbosity::VeryVerbose: return TEXT("VeryVerbose");
	default:                         return TEXT("Log");
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// FDiscordBotFileLogger
// ─────────────────────────────────────────────────────────────────────────────

FDiscordBotFileLogger::FDiscordBotFileLogger()
{
	// Build the log directory and file path.
	const FString LogDir = FPaths::ProjectSavedDir() / TEXT("Logs") / TEXT("DiscordBot");
	IFileManager::Get().MakeDirectory(*LogDir, /*Tree=*/true);
	LogFilePath = LogDir / TEXT("DiscordBot.log");

	// Open in append mode so multiple server sessions accumulate in one file.
	// FILEWRITE_AllowRead lets tools tail the file while the server is running.
	FileWriter = IFileManager::Get().CreateFileWriter(
		*LogFilePath,
		FILEWRITE_Append | FILEWRITE_AllowRead);

	if (FileWriter)
	{
		const FDateTime Now = FDateTime::UtcNow();
		const FString Header = FString::Printf(
			TEXT("\n[%04d.%02d.%02d-%02d.%02d.%02d UTC] ===== DiscordBot session started =====\n"),
			Now.GetYear(), Now.GetMonth(), Now.GetDay(),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		WriteLineUnlocked(Header);
		FileWriter->Flush();
	}
}

FDiscordBotFileLogger::~FDiscordBotFileLogger()
{
	TearDown();
}

void FDiscordBotFileLogger::Serialize(const TCHAR* Message,
                                      ELogVerbosity::Type Verbosity,
                                      const FName& Category)
{
	// Only capture our own log categories.
	if (Category != NAME_LogDiscordBridge   &&
	    Category != NAME_LogSMLWebSocket    &&
	    Category != NAME_LogBanManager      &&
	    Category != NAME_LogWhitelistManager)
	{
		return;
	}

	if (!FileWriter)
	{
		return;
	}

	const FDateTime Now = FDateTime::UtcNow();
	const FString Line = FString::Printf(
		TEXT("[%04d.%02d.%02d-%02d.%02d.%02d.%03d UTC][%s][%s] %s\n"),
		Now.GetYear(), Now.GetMonth(), Now.GetDay(),
		Now.GetHour(), Now.GetMinute(), Now.GetSecond(), Now.GetMillisecond(),
		VerbosityToString(Verbosity),
		*Category.ToString(),
		Message);

	FScopeLock Lock(&FileMutex);
	WriteLineUnlocked(Line);
}

void FDiscordBotFileLogger::Flush()
{
	FScopeLock Lock(&FileMutex);
	if (FileWriter)
	{
		FileWriter->Flush();
	}
}

void FDiscordBotFileLogger::TearDown()
{
	FScopeLock Lock(&FileMutex);
	if (FileWriter)
	{
		const FDateTime Now = FDateTime::UtcNow();
		const FString Footer = FString::Printf(
			TEXT("[%04d.%02d.%02d-%02d.%02d.%02d UTC] ===== DiscordBot session ended =====\n"),
			Now.GetYear(), Now.GetMonth(), Now.GetDay(),
			Now.GetHour(), Now.GetMinute(), Now.GetSecond());
		WriteLineUnlocked(Footer);
		FileWriter->Flush();
		delete FileWriter;
		FileWriter = nullptr;
	}
}

void FDiscordBotFileLogger::WriteLineUnlocked(const FString& Line)
{
	if (!FileWriter)
	{
		return;
	}
	// Serialize as UTF-8 without BOM so log-viewing tools work correctly.
	const FTCHARToUTF8 Utf8(*Line);
	FileWriter->Serialize(const_cast<ANSICHAR*>(Utf8.Get()), Utf8.Length());
}
