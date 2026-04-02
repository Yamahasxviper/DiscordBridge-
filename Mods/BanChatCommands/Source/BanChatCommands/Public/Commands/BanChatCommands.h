// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "BanChatCommands.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  /ban  — permanent ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /ban <player|Steam64|PUID> [reason...]
 *
 * Permanently bans a player.  The first argument is resolved as follows:
 *   • 17-digit decimal string  → Steam64 ID (bans via USteamBanSubsystem)
 *   • 32-char hex string       → EOS Product User ID (bans via UEOSBanSubsystem)
 *   • Anything else            → player display-name lookup (case-insensitive,
 *     substring match); bans by Steam64 when available, otherwise by EOS PUID.
 *
 * After the platform-specific ban is issued, cross-platform propagation is
 * triggered via UBanEnforcementSubsystem::PropagateToEOSAsync /
 * PropagateToSteamAsync so the player is also blocked on the other platform
 * if their accounts are linked.
 *
 * Requires the caller's Steam64 to be in AdminSteam64Ids (DefaultGame.ini),
 * or the command to be run from the server console.
 *
 * Examples:
 *   /ban 76561198000000000
 *   /ban 76561198000000000 Cheating
 *   /ban BadPlayer Toxic behaviour
 */
UCLASS()
class BANCHATCOMMANDS_API ABanChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /tempban  — timed ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /tempban <player|Steam64|PUID> <minutes> [reason...]
 *
 * Temporarily bans a player for the specified number of minutes.
 * Player resolution and cross-platform propagation follow the same rules as
 * /ban.  Use /ban for a permanent ban.
 *
 * Requires admin.
 *
 * Examples:
 *   /tempban 76561198000000000 60
 *   /tempban BadPlayer 1440 Spamming
 */
UCLASS()
class BANCHATCOMMANDS_API ATempBanChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ATempBanChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /unban  — remove a ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /unban <Steam64|PUID>
 *
 * Removes an existing ban.  Accepts either a Steam64 ID or an EOS PUID.
 * Both the Steam and EOS ban lists are tried so a single command removes
 * the ban regardless of which platform it was originally issued on.
 *
 * Requires admin.
 *
 * Examples:
 *   /unban 76561198000000000
 *   /unban 00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AUnbanChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AUnbanChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /bancheck  — query ban status
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /bancheck <player|Steam64|PUID>
 *
 * Reports ban status for a player.  Checks both Steam and EOS ban lists.
 * Accepts a player display name (for online players), a Steam64 ID, or an
 * EOS PUID.
 *
 * Requires admin.
 *
 * Examples:
 *   /bancheck 76561198000000000
 *   /bancheck BadPlayer
 */
UCLASS()
class BANCHATCOMMANDS_API ABanCheckChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanCheckChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /banlist  — list all active bans
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /banlist [page]
 *
 * Lists all active bans from both the Steam and EOS ban lists.
 * Results are paginated (10 entries per page).  Pass an optional page number
 * to view further pages.
 *
 * Requires admin.
 *
 * Examples:
 *   /banlist
 *   /banlist 2
 */
UCLASS()
class BANCHATCOMMANDS_API ABanListChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanListChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;

private:
    static constexpr int32 PageSize = 10;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /whoami  — display the caller's own platform IDs
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /whoami
 *
 * Shows the calling player's own platform identity (Steam64 ID or EOS Product
 * User ID, depending on how they connected to the server).
 * Useful for players who need their exact ID for a ban lookup.
 *
 * Available to all players (no admin requirement).
 *
 * Example output:
 *   [BanChatCommands] Your Steam64: 76561198000000000
 *   — or —
 *   [BanChatCommands] Your EOS PUID: 00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AWhoAmIChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AWhoAmIChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};
