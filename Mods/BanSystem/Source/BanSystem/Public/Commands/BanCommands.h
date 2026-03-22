// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "BanCommands.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  STEAM BAN COMMANDS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /steamban <Steam64Id|PlayerName> [duration_minutes] [reason...]
 *
 * Bans a player by Steam64 ID or by their in-game display name.
 * When a player name is supplied the system resolves the Steam64 ID from
 * the currently-connected player whose name matches (case-insensitive).
 * duration_minutes = 0 (or omitted) for a permanent ban.
 *
 * Examples (by raw ID):
 *   /steamban 76561198000000000
 *   /steamban 76561198000000000 0 Cheating
 *   /steamban 76561198000000000 1440 Toxic behaviour
 *
 * Examples (by player name):
 *   /steamban SomePlayer
 *   /steamban SomePlayer 60 Spamming
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
 * /eosban <EOSProductUserId|PlayerName> [duration_minutes] [reason...]
 *
 * Bans a player by EOS Product User ID (32-char hex) or by their in-game
 * display name.  When a player name is supplied the system resolves the
 * EOS PUID from the currently-connected player whose name matches
 * (case-insensitive).
 * duration_minutes = 0 (or omitted) for a permanent ban.
 *
 * Examples (by raw ID):
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51 0 Cheating
 *   /eosban 00020aed06f0a6958c3c067fb4b73d51 60 Spam
 *
 * Examples (by player name):
 *   /eosban SomePlayer
 *   /eosban SomePlayer 60 Spamming
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

// ─────────────────────────────────────────────────────────────────────────────
//  NAME-BASED CROSS-PLATFORM BAN COMMAND
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /banbyname <PlayerName> [duration_minutes] [reason...]
 *
 * Bans a currently-connected player by their in-game display name across ALL
 * available platforms simultaneously.
 *
 * The command:
 *   1. Looks up the connected player whose name matches the query
 *      (case-insensitive, substring match).
 *   2. Resolves their Steam64 ID and/or EOS Product User ID.
 *   3. Applies a ban in each subsystem where a valid ID was found.
 *
 * This is the recommended command for day-to-day moderation — admins do not
 * need to know the player's raw ID.  If multiple players share a similar name,
 * the command lists the ambiguous matches and asks for a more specific name.
 *
 * duration_minutes = 0 (or omitted) for a permanent ban.
 *
 * Examples:
 *   /banbyname SomePlayer
 *   /banbyname SomePlayer 0 Cheating
 *   /banbyname SomePlayer 60 Spam during event
 *
 * A matching /unbanbyname is NOT provided because offline unban must be done
 * by raw ID (the player may not be online).  Use /steamunban or /eosunban.
 */
UCLASS()
class BANSYSTEM_API ABanByNameCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanByNameCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

/**
 * /playerids [PlayerName]
 *
 * Lists the platform IDs (Steam64 and/or EOS PUID) of all connected players,
 * or of a specific player when a name is supplied.
 *
 * Useful for admins who need a raw ID for future offline unban commands.
 *
 * Examples:
 *   /playerids
 *   /playerids SomePlayer
 */
UCLASS()
class BANSYSTEM_API APlayerIdsCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    APlayerIdsCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

