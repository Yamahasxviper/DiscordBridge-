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
 * /ban <player|PUID> [reason...]
 *
 * Permanently bans a player.  The first argument is resolved as follows:
 *   • 32-char hex string       → EOS Product User ID (stored as "EOS:xxx" UID)
 *   • Anything else            → player display-name lookup (case-insensitive,
 *     substring match); bans using the player's FUniqueNetIdRepl identity.
 *
 * Requires the caller's EOS PUID to be in AdminEosPUIDs (BanChatCommands.ini),
 * or the command to be run from the server console.
 *
 * Examples:
 *   /ban BadPlayer Toxic behaviour
 *   /ban 00020aed06f0a6958c3c067fb4b73d51 Cheating
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
 * /tempban <player|PUID> <minutes> [reason...]
 *
 * Temporarily bans a player for the specified number of minutes.
 * Player resolution follows the same rules as /ban.  Use /ban for a permanent ban.
 *
 * Requires admin.
 *
 * Examples:
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
 * /unban <PUID>
 *
 * Removes an existing ban.  Accepts an EOS PUID.
 * The ban is looked up by compound UID ("EOS:xxx").
 *
 * Requires admin.
 *
 * Examples:
 *   /unban 00020aed06f0a6958c3c067fb4b73d51
 *   /unban EOS:00020aed06f0a6958c3c067fb4b73d51
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
 * /bancheck <player|PUID>
 *
 * Reports ban status for a player.  Checks the ban database by compound UID.
 * Accepts a player display name (for online players) or an EOS PUID.
 *
 * Requires admin.
 *
 * Examples:
 *   /bancheck BadPlayer
 *   /bancheck 00020aed06f0a6958c3c067fb4b73d51
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
 * connected under two different EOS PUIDs.
 *
 * Both UIDs must have existing ban records.  The link is bidirectional: each
 * ban's LinkedUids list is updated to include the other UID.
 *
 * Requires admin.
 *
 * Examples:
 *   /linkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344
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
 *   /unlinkbans EOS:00020aed06f0a6958c3c067fb4b73d51 EOS:aabbccdd11223344aabbccdd11223344
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
 * Shows the calling player's own EOS Product User ID.
 * On CSS Dedicated Server all players — regardless of launch platform — are
 * identified by their EOS PUID.  Useful for players who need their exact ID
 * to give to a server admin.
 *
 * Available to all players (no admin requirement).
 *
 * Example output:
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
