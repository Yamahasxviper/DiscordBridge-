// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanCommands.h"
#include "Command/CommandSender.h"
#include "BanPlayerLookup.h"
#include "BanEnforcementSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanCommands, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace BanCmdHelper
{
    /** Extract a joined reason string from Arguments starting at index StartIdx. */
    static FString JoinArgs(const TArray<FString>& Arguments, int32 StartIdx)
    {
        FString Result;
        for (int32 i = StartIdx; i < Arguments.Num(); ++i)
        {
            if (i > StartIdx) Result += TEXT(" ");
            Result += Arguments[i];
        }
        return Result;
    }

    // WorldContext must be an actor (e.g. the AChatCommandInstance itself via `this`)
    static USteamBanSubsystem* GetSteamBans(UObject* WorldContext)
    {
        if (!WorldContext) return nullptr;
        if (UWorld* World = WorldContext->GetWorld())
            if (UGameInstance* GI = World->GetGameInstance())
                return GI->GetSubsystem<USteamBanSubsystem>();
        return nullptr;
    }

    static UEOSBanSubsystem* GetEOSBans(UObject* WorldContext)
    {
        if (!WorldContext) return nullptr;
        if (UWorld* World = WorldContext->GetWorld())
            if (UGameInstance* GI = World->GetGameInstance())
                return GI->GetSubsystem<UEOSBanSubsystem>();
        return nullptr;
    }

    /**
     * Shared duration/reason parsing logic.
     *
     * Arguments layout:
     *   [0] = ID or player name (already consumed by caller)
     *   [1] = optional duration_minutes (numeric) or start of reason
     *   [2..] = rest of reason
     *
     * argsStartIdx is the index of the first argument AFTER the ID/name.
     */
    static void ParseDurationAndReason(const TArray<FString>& Arguments, int32 argsStartIdx,
                                       int32& OutDuration, FString& OutReason)
    {
        OutDuration = 0;
        OutReason   = TEXT("Banned by server administrator");

        if (Arguments.Num() > argsStartIdx)
        {
            if (Arguments[argsStartIdx].IsNumeric())
            {
                OutDuration = FCString::Atoi(*Arguments[argsStartIdx]);
                if (Arguments.Num() > argsStartIdx + 1)
                    OutReason = JoinArgs(Arguments, argsStartIdx + 1);
            }
            else
            {
                // No duration provided — treat everything after ID/name as reason
                OutReason = JoinArgs(Arguments, argsStartIdx);
            }
        }
    }

    /** Format duration for confirmation messages. */
    static FString FormatDuration(int32 DurationMinutes)
    {
        return (DurationMinutes <= 0)
            ? TEXT("permanently")
            : FString::Printf(TEXT("for %d minute(s)"), DurationMinutes);
    }

    /**
     * Try to resolve a Steam64 ID from either a raw ID string or a player name.
     *
     * @param WorldContext  Actor providing GetWorld() access.
     * @param Sender        Command sender for error feedback.
     * @param Input         First command argument (raw ID or player name).
     * @param OutSteam64Id  Populated with the resolved Steam64 ID on success.
     * @return true on success.
     */
    static bool ResolveSteamId(UObject* WorldContext, UCommandSender* Sender,
                               const FString& Input, FString& OutSteam64Id)
    {
        // Fast path: raw Steam64 ID
        if (USteamBanSubsystem::IsValidSteam64Id(Input))
        {
            OutSteam64Id = Input;
            return true;
        }

        // Attempt player name lookup for currently-connected players
        UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
        if (!World)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Invalid Steam64 ID: %s  "
                    "(must be 17 digits starting with 7656119)"), *Input),
                FLinearColor::Red);
            return false;
        }

        FResolvedBanId   Ids;
        FString          PlayerName;
        TArray<FString>  Ambiguous;

        if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
        {
            if (Ambiguous.Num() > 1)
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanSystem] Ambiguous name '%s'. Matching players: %s"),
                        *Input, *FString::Join(Ambiguous, TEXT(", "))),
                    FLinearColor::Yellow);
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanSystem] '%s' is not a valid Steam64 ID and no "
                        "online player with that name was found."), *Input),
                    FLinearColor::Red);
            }
            return false;
        }

        if (!Ids.HasSteamId())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Player '%s' was found but has no Steam ID. "
                    "They may be an Epic-only player — use /eosban instead."),
                    *PlayerName),
                FLinearColor::Yellow);
            return false;
        }

        OutSteam64Id = Ids.Steam64Id;
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Resolved '%s' → Steam64 ID: %s"),
                *PlayerName, *OutSteam64Id),
            FLinearColor::White);
        return true;
    }

    /**
     * Try to resolve an EOS PUID from either a raw ID string or a player name.
     */
    static bool ResolveEOSId(UObject* WorldContext, UCommandSender* Sender,
                             const FString& Input, FString& OutPUID)
    {
        // Fast path: raw EOS PUID — accept if it passes the 32-hex-char format check.
        if (UEOSBanSubsystem::IsValidEOSProductUserId(Input))
        {
            OutPUID = Input.ToLower();
            return true;
        }

        // Attempt player name lookup
        UWorld* World = WorldContext ? WorldContext->GetWorld() : nullptr;
        if (!World)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Invalid EOS Product User ID: %s  "
                    "(must be 32 hex chars)"), *Input),
                FLinearColor::Red);
            return false;
        }

        FResolvedBanId   Ids;
        FString          PlayerName;
        TArray<FString>  Ambiguous;

        if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
        {
            if (Ambiguous.Num() > 1)
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanSystem] Ambiguous name '%s'. Matching players: %s"),
                        *Input, *FString::Join(Ambiguous, TEXT(", "))),
                    FLinearColor::Yellow);
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanSystem] '%s' is not a valid EOS PUID and no "
                        "online player with that name was found."), *Input),
                    FLinearColor::Red);
            }
            return false;
        }

        if (!Ids.HasEOSPuid())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Player '%s' was found but has no EOS Product "
                    "User ID.  They may be offline-only — use /steamban instead."),
                    *PlayerName),
                FLinearColor::Yellow);
            return false;
        }

        OutPUID = Ids.EOSProductUserId;
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Resolved '%s' → EOS PUID: %s"),
                *PlayerName, *OutPUID),
            FLinearColor::White);
        return true;
    }
} // namespace BanCmdHelper

// ─────────────────────────────────────────────────────────────────────────────
//  /steamban  — accepts Steam64 ID or player name
// ─────────────────────────────────────────────────────────────────────────────
ASteamBanCommand::ASteamBanCommand()
{
    CommandName          = TEXT("steamban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("steamban <Steam64Id|PlayerName> [duration_minutes] [reason...]"));
}

EExecutionStatus ASteamBanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    FString Steam64Id;
    if (!BanCmdHelper::ResolveSteamId(this, Sender, Arguments[0], Steam64Id))
        return EExecutionStatus::BAD_ARGUMENTS;

    int32   Duration;
    FString Reason;
    BanCmdHelper::ParseDurationAndReason(Arguments, 1, Duration, Reason);

    USteamBanSubsystem* Bans = BanCmdHelper::GetSteamBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] Steam ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    Bans->BanPlayer(Steam64Id, Reason, Duration, Sender->GetSenderName());

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] Steam player %s banned %s. Reason: %s"),
            *Steam64Id, *BanCmdHelper::FormatDuration(Duration), *Reason),
        FLinearColor::Green);

    // Cross-platform: asynchronously look up the linked EOS PUID via EOSSystem
    // and apply an EOS ban with the same reason/duration when the result arrives.
    // This uses UBanEnforcementSubsystem which checks the sync cache first and
    // falls back to an async EOS query when the PUID is not yet cached.
    {
        UWorld* World = GetWorld();
        UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
        if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
        {
            Enforcement->PropagateToEOSAsync(Steam64Id, Reason, Duration, Sender->GetSenderName());
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /steamunban
// ─────────────────────────────────────────────────────────────────────────────
ASteamUnbanCommand::ASteamUnbanCommand()
{
    CommandName          = TEXT("steamunban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("steamunban <Steam64Id>"));
}

EExecutionStatus ASteamUnbanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    const FString Steam64Id = Arguments[0];

    if (!USteamBanSubsystem::IsValidSteam64Id(Steam64Id))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] '%s' is not a valid Steam64 ID "
                "(must be 17 digits starting with 7656119)."), *Steam64Id),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    USteamBanSubsystem* Bans = BanCmdHelper::GetSteamBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] Steam ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (Bans->UnbanPlayer(Steam64Id))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Steam player %s has been unbanned."), *Steam64Id),
            FLinearColor::Green);

        // Cross-platform: remove any linked EOS ban.
        {
            UWorld* World = GetWorld();
            UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
            if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
            {
                Enforcement->PropagateUnbanToEOSAsync(Steam64Id);
            }
        }

        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] No active Steam ban found for %s."), *Steam64Id),
        FLinearColor::Yellow);
    return EExecutionStatus::UNCOMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /steambanlist
// ─────────────────────────────────────────────────────────────────────────────
ASteamBanListCommand::ASteamBanListCommand()
{
    CommandName          = TEXT("steambanlist");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("steambanlist"));
}

EExecutionStatus ASteamBanListCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    USteamBanSubsystem* Bans = BanCmdHelper::GetSteamBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] Steam ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    TArray<FBanEntry> AllBans = Bans->GetAllBans();
    if (AllBans.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanSystem] Steam ban list is empty."), FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] Steam bans (%d total):"), AllBans.Num()),
        FLinearColor::White);

    for (const FBanEntry& Entry : AllBans)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("  %s | %s | Expires: %s | By: %s"),
                *Entry.PlayerId, *Entry.Reason,
                *Entry.GetExpiryString(), *Entry.BannedBy),
            FLinearColor::White);
    }
    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /eosban  — accepts EOS PUID or player name
// ─────────────────────────────────────────────────────────────────────────────
AEOSBanCommand::AEOSBanCommand()
{
    CommandName          = TEXT("eosban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("eosban <EOSProductUserId|PlayerName> [duration_minutes] [reason...]"));
}

EExecutionStatus AEOSBanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    FString EOSID;
    if (!BanCmdHelper::ResolveEOSId(this, Sender, Arguments[0], EOSID))
        return EExecutionStatus::BAD_ARGUMENTS;

    int32   Duration;
    FString Reason;
    BanCmdHelper::ParseDurationAndReason(Arguments, 1, Duration, Reason);

    UEOSBanSubsystem* Bans = BanCmdHelper::GetEOSBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] EOS ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    Bans->BanPlayer(EOSID, Reason, Duration, Sender->GetSenderName());

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] EOS player %s banned %s. Reason: %s"),
            *EOSID, *BanCmdHelper::FormatDuration(Duration), *Reason),
        FLinearColor::Green);

    // Cross-platform: asynchronously look up the linked Steam64 ID via EOSSystem
    // and apply a Steam ban with the same reason/duration when the result arrives.
    // Uses UBanEnforcementSubsystem (sync cache check first, async fallback).
    {
        UWorld* World = GetWorld();
        UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
        if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
        {
            Enforcement->PropagateToSteamAsync(EOSID, Reason, Duration, Sender->GetSenderName());
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /eosunban
// ─────────────────────────────────────────────────────────────────────────────
AEOSUnbanCommand::AEOSUnbanCommand()
{
    CommandName          = TEXT("eosunban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("eosunban <EOSProductUserId>"));
}

EExecutionStatus AEOSUnbanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    const FString EOSID = Arguments[0].ToLower();

    if (!UEOSBanSubsystem::IsValidEOSProductUserId(EOSID))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] '%s' is not a valid EOS Product User ID "
                "(must be 32 lowercase hex chars)."), *Arguments[0]),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    UEOSBanSubsystem* Bans = BanCmdHelper::GetEOSBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] EOS ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (Bans->UnbanPlayer(EOSID))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] EOS player %s has been unbanned."), *EOSID),
            FLinearColor::Green);

        // Cross-platform: remove any linked Steam ban.
        {
            UWorld* World = GetWorld();
            UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
            if (UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr)
            {
                Enforcement->PropagateUnbanToSteamAsync(EOSID);
            }
        }

        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] No active EOS ban found for %s."), *EOSID),
        FLinearColor::Yellow);
    return EExecutionStatus::UNCOMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /eosbanlist
// ─────────────────────────────────────────────────────────────────────────────
AEOSBanListCommand::AEOSBanListCommand()
{
    CommandName          = TEXT("eosbanlist");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("eosbanlist"));
}

EExecutionStatus AEOSBanListCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    UEOSBanSubsystem* Bans = BanCmdHelper::GetEOSBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] EOS ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    TArray<FBanEntry> AllBans = Bans->GetAllBans();
    if (AllBans.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanSystem] EOS ban list is empty."), FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] EOS bans (%d total):"), AllBans.Num()),
        FLinearColor::White);

    for (const FBanEntry& Entry : AllBans)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("  %s | %s | Expires: %s | By: %s"),
                *Entry.PlayerId, *Entry.Reason,
                *Entry.GetExpiryString(), *Entry.BannedBy),
            FLinearColor::White);
    }
    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /banbyname  — ban by player name across ALL available platforms
// ─────────────────────────────────────────────────────────────────────────────
ABanByNameCommand::ABanByNameCommand()
{
    CommandName          = TEXT("banbyname");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("banbyname <PlayerName> [duration_minutes] [reason...]"));
}

EExecutionStatus ABanByNameCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] No world context."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString NameQuery = Arguments[0];

    FResolvedBanId   Ids;
    FString          PlayerName;
    TArray<FString>  Ambiguous;

    if (!FBanPlayerLookup::FindPlayerByName(World, NameQuery, Ids, PlayerName, Ambiguous))
    {
        if (Ambiguous.Num() > 1)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Ambiguous name '%s'. Matching players:"),
                    *NameQuery),
                FLinearColor::Yellow);
            for (const FString& AmbigName : Ambiguous)
                Sender->SendChatMessage(FString::Printf(TEXT("  • %s"), *AmbigName), FLinearColor::Yellow);
            Sender->SendChatMessage(
                TEXT("[BanSystem] Please use a more specific name or a raw ID."),
                FLinearColor::Yellow);
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] No online player found matching '%s'."), *NameQuery),
                FLinearColor::Red);
        }
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    int32   Duration;
    FString Reason;
    BanCmdHelper::ParseDurationAndReason(Arguments, 1, Duration, Reason);

    const FString Admin   = Sender->GetSenderName();
    const FString DurStr  = BanCmdHelper::FormatDuration(Duration);
    int32         BanCount = 0;

    UGameInstance* GI = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;
    UBanEnforcementSubsystem* Enforcement = GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr;

    // ── Steam ban ──────────────────────────────────────────────────────────
    if (Ids.HasSteamId())
    {
        USteamBanSubsystem* SteamBans = BanCmdHelper::GetSteamBans(this);
        if (SteamBans)
        {
            SteamBans->BanPlayer(Ids.Steam64Id, Reason, Duration, Admin);
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] Steam ID %s banned %s."),
                    *Ids.Steam64Id, *DurStr),
                FLinearColor::Green);
            ++BanCount;

            // Cross-platform: if the EOS PUID was not immediately available,
            // propagate the ban asynchronously via EOSSystem (cache-first, then
            // async EOS query) so linked Epic accounts are also banned.
            if (!Ids.HasEOSPuid() && Enforcement)
            {
                Enforcement->PropagateToEOSAsync(Ids.Steam64Id, Reason, Duration, Admin);
            }
        }
    }

    // ── EOS ban ────────────────────────────────────────────────────────────
    if (Ids.HasEOSPuid())
    {
        UEOSBanSubsystem* EOSBans = BanCmdHelper::GetEOSBans(this);
        if (EOSBans)
        {
            EOSBans->BanPlayer(Ids.EOSProductUserId, Reason, Duration, Admin);
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanSystem] EOS PUID %s banned %s."),
                    *Ids.EOSProductUserId, *DurStr),
                FLinearColor::Green);
            ++BanCount;

            // Cross-platform: if the Steam64 ID was not immediately available,
            // propagate the ban asynchronously via EOSSystem (cache-first, then
            // async reverse lookup) so linked Steam accounts are also banned.
            if (!Ids.HasSteamId() && Enforcement)
            {
                Enforcement->PropagateToSteamAsync(Ids.EOSProductUserId, Reason, Duration, Admin);
            }
        }
    }

    if (BanCount == 0)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Player '%s' was found but could not be banned "
                "(no subsystem available)."), *PlayerName),
            FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] '%s' banned %s on %d platform(s). Reason: %s"),
            *PlayerName, *DurStr, BanCount, *Reason),
        FLinearColor::Green);

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  /playerids  — show platform IDs of connected players
// ─────────────────────────────────────────────────────────────────────────────
APlayerIdsCommand::APlayerIdsCommand()
{
    CommandName          = TEXT("playerids");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("playerids [PlayerName]"));
}

EExecutionStatus APlayerIdsCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    UWorld* World = GetWorld();
    if (!World)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] No world context."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    // Optional: filter by name
    const bool    bFilter    = Arguments.Num() >= 1;
    const FString NameFilter = bFilter ? Arguments[0] : FString();

    auto AllPlayers = FBanPlayerLookup::GetAllConnectedPlayers(World);

    if (AllPlayers.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanSystem] No players currently connected."), FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    int32 Shown = 0;
    for (const auto& Pair : AllPlayers)
    {
        const FString&        Name = Pair.Key;
        const FResolvedBanId& Ids  = Pair.Value;

        if (bFilter && !Name.Contains(NameFilter, ESearchCase::IgnoreCase))
            continue;

        FString IdLine = FString::Printf(TEXT("  [%s]"), *Name);
        if (Ids.HasSteamId())
            IdLine += FString::Printf(TEXT("  Steam: %s"), *Ids.Steam64Id);
        if (Ids.HasEOSPuid())
            IdLine += FString::Printf(TEXT("  EOS: %s"), *Ids.EOSProductUserId);
        if (!Ids.IsValid())
            IdLine += TEXT("  (no platform ID resolved)");

        Sender->SendChatMessage(IdLine, FLinearColor::White);
        ++Shown;
    }

    if (Shown == 0)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] No connected players match '%s'."), *NameFilter),
            FLinearColor::Yellow);
    }

    return EExecutionStatus::COMPLETED;
}

