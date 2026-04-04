// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "BanChatCommands.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanChatCommands, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  /ban  — permanent ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /ban <player|Steam64|PUID> [reason...]
 *
 * Permanently bans a player.  The first argument is resolved as follows:
 *   • 17-digit decimal string  → Steam64 ID (stored as "STEAM:xxx" UID)
 *   • 32-char hex string       → EOS Product User ID (stored as "EOS:xxx" UID)
 *   • Anything else            → player display-name lookup (case-insensitive,
 *     substring match); bans using the player's FUniqueNetIdRepl identity.
 *
 * Requires the caller's Steam64 to be in AdminSteam64Ids (BanChatCommands.ini),
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
 * Player resolution follows the same rules as /ban.  Use /ban for a permanent ban.
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
 * The ban is looked up by compound UID ("STEAM:xxx" or "EOS:xxx").
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
 * Reports ban status for a player.  Checks the ban database by compound UID.
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
 * Lists all active bans from the ban database.
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
//  /linkbans  — associate two UIDs with the same ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /linkbans <UID1> <UID2>
 *
 * Links two compound UIDs together so that a ban on one also blocks the player
 * when they connect under the other identity.  Useful when the same person has
 * both a Steam account and an EOS account.
 *
 * Both UIDs must have existing ban records.  The link is bidirectional: each
 * ban's LinkedUids list is updated to include the other UID.
 *
 * Requires admin.
 *
 * Examples:
 *   /linkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API ALinkBansChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ALinkBansChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /unlinkbans  — remove the association between two UIDs
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /unlinkbans <UID1> <UID2>
 *
 * Removes the bidirectional link between two compound UIDs that was previously
 * created by /linkbans.
 *
 * Requires admin.
 *
 * Examples:
 *   /unlinkbans STEAM:76561198000000000 EOS:00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AUnlinkBansChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AUnlinkBansChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /playerhistory  — look up all known identities for a player name or UID
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /playerhistory <name_substring|UID>
 *
 * Queries the player session registry for all compound UIDs that have connected
 * under a given display name (substring match), or the display name recorded for
 * a given compound UID.
 *
 * Useful when an EOS player reconnects under a new PUID: admins can search the
 * registry by the player's old display name, find their previous UID, cross-check
 * it against the ban database, and use /linkbans or /ban to re-apply the ban.
 *
 * Requires admin.
 *
 * Examples:
 *   /playerhistory BadPlayer
 *   /playerhistory EOS:00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API APlayerHistoryChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    APlayerHistoryChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;

private:
    static constexpr int32 MaxResults = 20;
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
