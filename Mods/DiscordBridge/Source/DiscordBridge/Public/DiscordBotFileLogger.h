// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Misc/OutputDevice.h"

/**
 * Lightweight FOutputDevice that captures log messages from the DiscordBot-related
 * categories (LogDiscordBridge, LogSMLWebSocket, LogBanManager, LogWhitelistManager)
 * and appends them to FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log.
 *
 * Each server session is separated by a clear header/footer line so operators
 * can identify restarts within a single log file.  The file is opened in append
 * mode; delete or rotate it manually to start fresh.
 *
 * Register via GLog->AddOutputDevice() in InitializeServer() and unregister via
 * GLog->RemoveOutputDevice() in Deinitialize().  The subsystem owns the lifetime
 * via TUniquePtr<FDiscordBotFileLogger>.
 */
class DISCORDBRIDGE_API FDiscordBotFileLogger : public FOutputDevice
{
public:
	FDiscordBotFileLogger();
	virtual ~FDiscordBotFileLogger();

	// FOutputDevice interface
	virtual void Serialize(const TCHAR* Message, ELogVerbosity::Type Verbosity,
	                       const FName& Category) override;
	virtual void Flush() override;
	virtual void TearDown() override;

	// Safe to call from background threads (e.g. the SMLWebSocket runnable).
	virtual bool CanBeUsedOnAnyThread() const override { return true; }
	virtual bool CanBeUsedOnPanic()     const          { return true; }

	/** Returns the full path to the log file.  Empty if the file could not be created. */
	const FString& GetLogFilePath() const { return LogFilePath; }

private:
	/** Protects FileWriter from concurrent access by the game thread and the
	 *  SMLWebSocket background thread. */
	FCriticalSection FileMutex;

	/** Opened in append mode; null if the file could not be created. */
	FArchive* FileWriter = nullptr;

	/** Full path to FactoryGame/Saved/Logs/DiscordBot/DiscordBot.log */
	FString LogFilePath;

	/** Write a UTF-8 line to the file without acquiring the lock.
	 *  Caller must hold FileMutex before calling. */
	void WriteLineUnlocked(const FString& Line);
};
