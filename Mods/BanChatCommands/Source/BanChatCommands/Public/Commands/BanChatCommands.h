// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "Subsystems/GameInstanceSubsystem.h"
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
//  /banname  — ban by display name (EOS PUID + IP in one command)
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /banname <name_substring> [reason...]
 *
 * Looks up the player in the session registry (so offline players can be
 * targeted) and permanently bans both their EOS PUID and, if an IP address
 * was recorded at their last login, their IP address.  The two ban records
 * are linked so enforcement triggers on either identity.
 *
 * If the name matches more than one session record the command lists all
 * matches and asks for a more specific substring.
 *
 * Requires admin.
 *
 * Examples:
 *   /banname BadPlayer Griefing
 *   /banname bad Cheating
 */
UCLASS()
class BANCHATCOMMANDS_API ABanNameChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanNameChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /unbanname  — remove EOS + IP bans by display name
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /unbanname <name_substring>
 *
 * Looks up the player in the session registry by display name and removes
 * both their EOS PUID ban and, if an IP address was recorded, their IP ban.
 * Works for offline players as long as they connected at least once while
 * the session registry was active.
 *
 * If the name matches more than one session record the command lists all
 * matches and asks for a more specific substring.
 *
 * Requires admin.
 *
 * Examples:
 *   /unbanname BadPlayer
 *   /unbanname bad
 */
UCLASS()
class BANCHATCOMMANDS_API AUnbanNameChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AUnbanNameChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
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

// ─────────────────────────────────────────────────────────────────────────────
//  /reloadconfig  — hot-reload BanChatCommands.ini without restarting
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /reloadconfig
 *
 * Reloads the BanChatCommands configuration (AdminEosPUIDs) from disk without
 * restarting the server.  Useful after editing DefaultBanChatCommands.ini or
 * Saved/Config/<Platform>/BanChatCommands.ini while the server is running.
 *
 * Requires admin (or server console).
 */
UCLASS()
class BANCHATCOMMANDS_API AReloadConfigChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AReloadConfigChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /kick  — disconnect a player without banning them
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /kick <player|PUID> [reason...]
 *
 * Kicks a connected player without banning them.
 * Player resolution follows the same rules as /ban.
 *
 * Requires admin OR moderator (AdminEosPUIDs or ModeratorEosPUIDs).
 *
 * Examples:
 *   /kick BadPlayer Stop griefing
 *   /kick 00020aed06f0a6958c3c067fb4b73d51 AFK too long
 */
UCLASS()
class BANCHATCOMMANDS_API AKickChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AKickChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /modban  — 30-minute temporary ban usable by moderators
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /modban <player|PUID> [reason...]
 *
 * Temporarily bans a player for 30 minutes.  Designed for moderators who do
 * not have access to /tempban or /ban.  Player resolution follows the same
 * rules as /ban.
 *
 * Requires admin OR moderator (AdminEosPUIDs or ModeratorEosPUIDs).
 *
 * Examples:
 *   /modban BadPlayer Spamming chat
 *   /modban 00020aed06f0a6958c3c067fb4b73d51 Harassment
 */
UCLASS()
class BANCHATCOMMANDS_API AModBanChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AModBanChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /warn  — issue a formal warning to a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /warn <player|PUID> <reason...>
 *
 * Issues a formal warning to a player.  Warnings are stored persistently in
 * the PlayerWarningRegistry.  If the player's total warning count reaches
 * AutoBanWarnCount (BanSystem config), they are automatically banned for
 * AutoBanWarnMinutes minutes (0 = permanent).
 *
 * Requires admin.
 *
 * Examples:
 *   /warn BadPlayer Please stop griefing other players
 *   /warn 00020aed06f0a6958c3c067fb4b73d51 Toxic chat behaviour
 */
UCLASS()
class BANCHATCOMMANDS_API AWarnChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AWarnChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /warnings  — list all warnings for a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /warnings <player|PUID>
 *
 * Lists all recorded warnings for a player, showing the warning number,
 * reason, issuing admin, and date.
 *
 * Requires admin.
 *
 * Examples:
 *   /warnings BadPlayer
 *   /warnings 00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AWarningsChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AWarningsChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /clearwarns  — remove all warnings for a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /clearwarns <player|PUID>
 *
 * Clears all warnings for a player from the PlayerWarningRegistry and
 * reports how many warnings were removed.
 *
 * Requires admin.
 *
 * Examples:
 *   /clearwarns BadPlayer
 *   /clearwarns 00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AClearWarnsChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AClearWarnsChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /announce  — server-wide broadcast
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /announce <message...>
 *
 * Sends a server-wide system message to all connected players.
 * Also mirrors the announcement to Discord via the existing bridge.
 *
 * Requires admin.
 *
 * Examples:
 *   /announce Server restarting in 5 minutes!
 */
UCLASS()
class BANCHATCOMMANDS_API AAnnounceChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AAnnounceChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /stafflist  — list currently-online admins and moderators
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /stafflist
 *
 * Shows the calling player a formatted list of currently-online admins and
 * moderators.  Useful for players to know who is on duty.
 *
 * Available to all players.
 *
 * Examples:
 *   /stafflist
 */
UCLASS()
class BANCHATCOMMANDS_API AStaffListChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AStaffListChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /reason  — show ban reason for a UID
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /reason <UID>
 *
 * Shows the ban reason for the given compound UID.  Any player or admin may
 * call this to look up the public ban reason without needing REST/API access.
 *
 * Available to all players and console.
 *
 * Examples:
 *   /reason EOS:00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AReasonChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AReasonChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /history  — self session and warning history
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /history
 *
 * Lets any player see their own session info and warning history without
 * requiring admin.  Shows last-seen timestamp and all warning entries
 * (up to 5, summarised if more exist).
 *
 * Requires the sender to be an in-game player (not console).
 *
 * Examples:
 *   /history
 */
UCLASS()
class BANCHATCOMMANDS_API AHistoryChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AHistoryChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /mute  — mute a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /mute <player|PUID> [minutes] [reason...]
 *
 * Mutes a player — their chat messages will not be relayed to Discord and
 * other players will not see them until the mute expires or is lifted with
 * /unmute.  Mutes are in-memory only and do not persist across server restarts.
 *
 * Requires admin.
 *
 * Examples:
 *   /mute SpamBot 30 Spamming chat
 *   /mute 00020aed06f0a6958c3c067fb4b73d51 Harassment
 */
UCLASS()
class BANCHATCOMMANDS_API AMuteChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AMuteChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /unmute  — unmute a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /unmute <player|PUID>
 *
 * Lifts an active mute from a player.
 *
 * Requires admin.
 *
 * Examples:
 *   /unmute SpamBot
 *   /unmute 00020aed06f0a6958c3c067fb4b73d51
 */
UCLASS()
class BANCHATCOMMANDS_API AUnmuteChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AUnmuteChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /note  — add a private admin note to a player record
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /note <player|PUID> <text...>
 *
 * Adds a private admin-only note to a player's record.  Notes are NOT shown
 * to the player and do NOT count toward the auto-ban warning threshold.
 * Use /notes <player> to view all notes for a player.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API ANoteChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ANoteChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /notes  — list notes for a player
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /notes <player|PUID>
 *
 * Lists all private admin notes on record for a player.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API ANotesChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ANotesChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /duration  — show remaining time on active tempban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /duration <player|PUID>
 *
 * Shows the remaining time on a player's active temporary ban,
 * formatted as "X days Y hours Z minutes remaining".
 * For permanent bans reports "permanent".
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API ADurationChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ADurationChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /tempunmute  — timed mute that auto-expires
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /tempunmute <player|PUID> <minutes>
 *
 * Applies (or updates) a timed mute that automatically expires after
 * <minutes> minutes.  If the player is already muted indefinitely, this
 * replaces the indefinite mute with a timed one.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API ATempUnmuteChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ATempUnmuteChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /mutecheck  — check mute status
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /mutecheck <player|PUID>
 *
 * Shows whether a player is currently muted and when the mute expires.
 *
 * Requires moderator or admin.
 */
UCLASS()
class BANCHATCOMMANDS_API AMuteCheckChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AMuteCheckChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /banreason  — edit the reason on an active ban
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /banreason <player|PUID> <new reason...>
 *
 * Edits the reason on an existing active ban without changing its duration.
 * Avoids the need to unban and re-ban just to correct a reason.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API ABanReasonChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    ABanReasonChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /staffchat  — staff-only in-game message
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /staffchat <message...>
 *
 * Sends a chat message visible ONLY to online admins and moderators.
 * Regular players cannot see the message.  Useful for in-game staff
 * coordination without cluttering public chat.
 *
 * Requires moderator or admin.
 */
UCLASS()
class BANCHATCOMMANDS_API AStaffChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AStaffChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  UPlayerNoteRegistry  — per-player private admin notes
// ─────────────────────────────────────────────────────────────────────────────

USTRUCT(BlueprintType)
struct BANCHATCOMMANDS_API FPlayerNoteEntry
{
    GENERATED_BODY()

    /** Auto-incremented integer primary key (0 when not yet persisted). */
    UPROPERTY(BlueprintReadOnly, Category = "BanChatCommands")
    int64 Id = 0;

    /** Compound UID of the noted player: "EOS:xxx". */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString Uid;

    /** Display name at time of note. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString PlayerName;

    /** The note text. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString Note;

    /** Admin who added the note. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FString AddedBy;

    /** UTC timestamp when the note was added. */
    UPROPERTY(BlueprintReadWrite, Category = "BanChatCommands")
    FDateTime NoteDate;

    FPlayerNoteEntry()
        : Id(0)
        , NoteDate(FDateTime::UtcNow())
    {}
};

/**
 * UPlayerNoteRegistry
 *
 * GameInstance subsystem that stores private admin notes about players.
 * Persists notes to notes.json (same directory as bans.json).
 * Notes are NOT shown to players and do NOT count toward warning thresholds.
 *
 * Thread-safe: all public methods acquire the internal Mutex.
 */
UCLASS()
class BANCHATCOMMANDS_API UPlayerNoteRegistry : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    /** Add a note for the given player UID. Thread-safe. */
    void AddNote(const FString& Uid, const FString& PlayerName,
                 const FString& Note, const FString& AddedBy);

    /** Returns all notes for the given UID (case-insensitive). Thread-safe. */
    TArray<FPlayerNoteEntry> GetNotesForUid(const FString& Uid) const;

    /** Returns every note in the registry. Thread-safe. */
    TArray<FPlayerNoteEntry> GetAllNotes() const;

    /** Delete the note with the given Id. Returns true if found. Thread-safe. */
    bool DeleteNote(int64 Id);

private:
    void    LoadFromFile();
    bool    SaveToFile() const;
    FString GetRegistryPath() const;

    TArray<FPlayerNoteEntry> Notes;
    mutable FCriticalSection Mutex;
    FString FilePath;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /mutelist
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class BANCHATCOMMANDS_API AMuteListChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AMuteListChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /clearwarn <id>
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class BANCHATCOMMANDS_API AClearWarnByIdChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AClearWarnByIdChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /extend <player|PUID> <minutes>
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class BANCHATCOMMANDS_API AExtendBanChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AExtendBanChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /appeal <reason...>
// ─────────────────────────────────────────────────────────────────────────────
UCLASS()
class BANCHATCOMMANDS_API AAppealChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AAppealChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /mutereason  — edit the reason on an active mute
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /mutereason <player|PUID> <new reason...>
 *
 * Updates the reason stored on an existing active mute without lifting or
 * replacing the mute.  Useful to correct a typo or add context after the fact.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API AMuteReasonChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AMuteReasonChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /freeze  — temporarily immobilise a player for investigation
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /freeze <player|PUID>
 * /spectator <player|PUID>
 *
 * Locks a player in place (disables movement) without kicking them.
 * Run the same command again to unfreeze the player.
 * Useful for pausing a suspect during an investigation.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API AFreezeChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AFreezeChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;

    /** UIDs of currently-frozen players. */
    static TSet<FString> FrozenPlayerUids;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /clearchat  — flush recent chat history and notify Discord
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /clearchat [reason...]
 *
 * Flushes the in-game chat by broadcasting blank lines (effectively scrolling
 * old content off screen), then posts a notification to the configured
 * DiscordWebhookUrl so staff are aware the chat was cleared.
 *
 * Requires admin.
 */
UCLASS()
class BANCHATCOMMANDS_API AClearChatChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AClearChatChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /report  — player-initiated report that creates a Discord alert
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /report <player> [reason...]
 *
 * Lets any in-game player flag another player.  Sends a Discord alert to the
 * configured ReportWebhookUrl (BanChatCommandsConfig) and logs the report.
 * No admin privileges required — available to all connected players.
 *
 * Examples:
 *   /report BadPlayer Griefing my base
 *   /report SpamBot Repeated spam in chat
 */
UCLASS()
class BANCHATCOMMANDS_API AReportChatCommand : public AChatCommandInstance
{
    GENERATED_BODY()
public:
    AReportChatCommand();
    virtual EExecutionStatus ExecuteCommand_Implementation(
        UCommandSender* Sender,
        const TArray<FString>& Arguments,
        const FString& Label) override;
};
