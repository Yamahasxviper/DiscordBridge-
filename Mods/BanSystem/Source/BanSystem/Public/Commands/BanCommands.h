// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "BanCommands.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  STEAM BAN COMMANDS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /steamban <Steam64Id> [duration_minutes] [reason...]
 *
 * Bans a player by Steam64 ID.
 * duration_minutes = 0 (or omitted) for a permanent ban.
 *
 * Examples:
 *   /steamban 76561198000000000
 *   /steamban 76561198000000000 0 Cheating
 *   /steamban 76561198000000000 1440 Toxic behaviour
 */
UCLASS()
class BANSYSTEM_API ASteamBanCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ASteamBanCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

/**
 * /steamunban <Steam64Id>
 *
 * Removes an existing Steam ban.
 */
UCLASS()
class BANSYSTEM_API ASteamUnbanCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ASteamUnbanCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

/**
 * /steambanlist
 *
 * Lists all active Steam bans.
 */
UCLASS()
class BANSYSTEM_API ASteamBanListCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ASteamBanListCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  EOS BAN COMMANDS  (completely independent from Steam commands)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /eosban <EOSProductUserId> [duration_minutes] [reason...]
 *
 * Bans a player by EOS Product User ID (32-char hex).
 * duration_minutes = 0 (or omitted) for a permanent ban.
 *
 * Examples:
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51 0 Cheating
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51 60 Spam
 */
UCLASS()
class BANSYSTEM_API AEOSBanCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AEOSBanCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

/**
 * /eosunban <EOSProductUserId>
 *
 * Removes an existing EOS ban.
 */
UCLASS()
class BANSYSTEM_API AEOSUnbanCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AEOSUnbanCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

/**
 * /eosbanlist
 *
 * Lists all active EOS bans.
 */
UCLASS()
class BANSYSTEM_API AEOSBanListCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AEOSBanListCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};
