// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanCommands.h"
#include "Command/CommandSender.h"
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
} // namespace BanCmdHelper

// ─────────────────────────────────────────────────────────────────────────────
//  /steamban
// ─────────────────────────────────────────────────────────────────────────────
ASteamBanCommand::ASteamBanCommand()
{
    CommandName          = TEXT("steamban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("steamban <Steam64Id> [duration_minutes] [reason...]"));
}

EExecutionStatus ASteamBanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    const FString Steam64Id = Arguments[0];
    if (!USteamBanSubsystem::IsValidSteam64Id(Steam64Id))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Invalid Steam64 ID: %s  (must be 17 digits starting with 7656119)"),
                *Steam64Id),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    int32  Duration = 0;
    FString Reason  = TEXT("Banned by server administrator");

    if (Arguments.Num() >= 2)
    {
        if (Arguments[1].IsNumeric())
        {
            Duration = FCString::Atoi(*Arguments[1]);
            if (Arguments.Num() >= 3)
                Reason = BanCmdHelper::JoinArgs(Arguments, 2);
        }
        else
        {
            // No duration provided — treat everything after ID as reason
            Reason = BanCmdHelper::JoinArgs(Arguments, 1);
        }
    }

    USteamBanSubsystem* Bans = BanCmdHelper::GetSteamBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] Steam ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString Admin = Sender->GetSenderName();
    Bans->BanPlayer(Steam64Id, Reason, Duration, Admin);

    const FString DurStr = (Duration <= 0)
        ? TEXT("permanently")
        : FString::Printf(TEXT("for %d minute(s)"), Duration);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] Steam player %s banned %s. Reason: %s"),
            *Steam64Id, *DurStr, *Reason),
        FLinearColor::Green);

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
//  /eosban
// ─────────────────────────────────────────────────────────────────────────────
AEOSBanCommand::AEOSBanCommand()
{
    CommandName          = TEXT("eosban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = FText::FromString(TEXT("eosban <EOSProductUserId> [duration_minutes] [reason...]"));
}

EExecutionStatus AEOSBanCommand::ExecuteCommand_Implementation(
    UCommandSender*        Sender,
    const TArray<FString>& Arguments,
    const FString&         Label)
{
    const FString EOSID = Arguments[0];
    if (!UEOSBanSubsystem::IsValidEOSProductUserId(EOSID))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanSystem] Invalid EOS Product User ID: %s  (must be 32 hex chars)"),
                *EOSID),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    int32  Duration = 0;
    FString Reason  = TEXT("Banned by server administrator");

    if (Arguments.Num() >= 2)
    {
        if (Arguments[1].IsNumeric())
        {
            Duration = FCString::Atoi(*Arguments[1]);
            if (Arguments.Num() >= 3)
                Reason = BanCmdHelper::JoinArgs(Arguments, 2);
        }
        else
        {
            Reason = BanCmdHelper::JoinArgs(Arguments, 1);
        }
    }

    UEOSBanSubsystem* Bans = BanCmdHelper::GetEOSBans(this);
    if (!Bans)
    {
        Sender->SendChatMessage(TEXT("[BanSystem] EOS ban subsystem unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString Admin = Sender->GetSenderName();
    Bans->BanPlayer(EOSID, Reason, Duration, Admin);

    const FString DurStr = (Duration <= 0)
        ? TEXT("permanently")
        : FString::Printf(TEXT("for %d minute(s)"), Duration);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanSystem] EOS player %s banned %s. Reason: %s"),
            *EOSID, *DurStr, *Reason),
        FLinearColor::Green);

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
    const FString EOSID = Arguments[0];

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
