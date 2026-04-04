// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanChatCommands.h"
#include "BanChatCommandsConfig.h"
#include "Command/CommandSender.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanTypes.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/OnlineReplStructs.h"
// Full definition required for Cast<> between AFGPlayerController and APlayerController
// (UE Cast<> rejects incomplete types via static_assert in Casts.h).
#include "FGPlayerController.h"

DEFINE_LOG_CATEGORY(LogBanChatCommands);

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace BanChat
{
    static UBanDatabase* GetDB(UObject* Ctx)
    {
        if (!Ctx) return nullptr;
        UWorld* World = Ctx->GetWorld();
        if (!World) return nullptr;
        UGameInstance* GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<UBanDatabase>() : nullptr;
    }

    /** Join Arguments[StartIdx..] into a space-separated string. */
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

    /** Format ban duration for human-readable confirmation. */
    static FString FormatDuration(int32 DurationMinutes)
    {
        return (DurationMinutes <= 0)
            ? TEXT("permanently")
            : FString::Printf(TEXT("for %d minute(s)"), DurationMinutes);
    }

    /** Format ban expiry date for display output. */
    static FString FormatExpiry(const FBanEntry& Entry)
    {
        if (Entry.bIsPermanent)
            return TEXT("never (permanent)");
        return Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")) + TEXT(" UTC");
    }

    /** Returns true if Id is a 17-digit decimal Steam64 ID. */
    static bool IsValidSteam64(const FString& Id)
    {
        if (Id.Len() != 17) return false;
        for (TCHAR c : Id)
            if (c < '0' || c > '9') return false;
        return true;
    }

    /** Returns true if Id is a 32-character lowercase hex EOS Product User ID. */
    static bool IsValidEOSPUID(const FString& Id)
    {
        if (Id.Len() != 32) return false;
        for (TCHAR c : Id)
            if (!FChar::IsHexDigit(c)) return false;
        return true;
    }

    /**
     * Check whether the command sender is allowed to run admin commands.
     *
     * Console senders (non-player) are always permitted.  Player senders must
     * have their Steam64 ID listed in UBanChatCommandsConfig::AdminSteam64Ids.
     * When the list is empty, player access is denied (console-only mode).
     *
     * @param Sender  The command sender to check.
     * @param OutSteam64Id  Populated with the sender's Steam64 ID when they connect via Steam.
     * @return true when the sender is authorised to run admin commands.
     */
    static bool IsAdminSender(UCommandSender* Sender, FString& OutSteam64Id)
    {
        if (!Sender->IsPlayerSender())
        {
            // Console / server operator — always allowed.
            OutSteam64Id.Reset();
            return true;
        }

        // Resolve the sender's platform identity directly from their PlayerState.
        APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer());
        if (PC && PC->PlayerState)
        {
            const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
            if (UniqueId.IsValid())
            {
                const FString Type = UniqueId->GetType().ToString().ToUpper();
                if (Type == TEXT("STEAM"))
                    OutSteam64Id = UniqueId->ToString();
            }
        }

        const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
        if (!Cfg || !Cfg->IsAdmin(OutSteam64Id))
        {
            Sender->SendChatMessage(
                TEXT("[BanChatCommands] You do not have permission to use this command."),
                FLinearColor::Red);
            return false;
        }
        return true;
    }

    /**
     * Resolve a player argument to a compound ban UID.
     *
     * Resolution order:
     *   1. 17-digit decimal → Steam64 ID  → UID "STEAM:xxx"
     *   2. 32-char hex       → EOS PUID   → UID "EOS:xxx"
     *   3. Otherwise         → display-name substring lookup against connected players.
     *
     * Returns true on success and populates OutUid + OutDisplayName.
     */
    static bool ResolveTarget(UObject* Ctx, UCommandSender* Sender,
                              const FString& Arg,
                              FString& OutUid,
                              FString& OutDisplayName)
    {
        // 1. Raw Steam64 ID
        if (IsValidSteam64(Arg))
        {
            OutUid         = UBanDatabase::MakeUid(TEXT("STEAM"), Arg);
            OutDisplayName = Arg;
            return true;
        }

        // 2. Raw EOS PUID
        if (IsValidEOSPUID(Arg))
        {
            const FString Lower = Arg.ToLower();
            OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), Lower);
            OutDisplayName = Lower;
            return true;
        }

        // 3. Display-name lookup against connected PlayerControllers.
        UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
        if (!World)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid Steam64 ID or EOS PUID."), *Arg),
                FLinearColor::Red);
            return false;
        }

        APlayerController* FoundPC = nullptr;
        TArray<FString>    Matches;
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC || !PC->PlayerState) continue;
            const FString Name = PC->PlayerState->GetPlayerName();
            if (Name.Contains(Arg, ESearchCase::IgnoreCase))
            {
                Matches.Add(Name);
                if (!FoundPC) FoundPC = PC;
            }
        }

        if (Matches.IsEmpty())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No connected player found matching '%s'. "
                    "Use a Steam64 ID or EOS PUID to target offline players."), *Arg),
                FLinearColor::Red);
            return false;
        }

        if (Matches.Num() > 1)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Ambiguous name '%s'. Matching players: %s"),
                    *Arg, *FString::Join(Matches, TEXT(", "))),
                FLinearColor::Yellow);
            return false;
        }

        // Unique match — resolve their platform identity.
        const FUniqueNetIdRepl& UniqueId = FoundPC->PlayerState->GetUniqueId();
        if (!UniqueId.IsValid())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Found player '%s' but could not resolve "
                    "their platform identity."), *Matches[0]),
                FLinearColor::Red);
            return false;
        }

        const FString Platform = UniqueId->GetType().ToString().ToUpper();
        const FString RawId    = UniqueId->ToString();
        OutUid         = UBanDatabase::MakeUid(Platform, RawId);
        OutDisplayName = Matches[0];

        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Resolved '%s' → %s: %s"),
                *Arg, *Platform, *RawId),
            FLinearColor::White);
        return true;
    }

    /**
     * Shared ban logic used by both /ban and /tempban.
     */
    static EExecutionStatus DoBan(UObject* Ctx, UCommandSender* Sender,
                                  const FString& Uid, const FString& DisplayName,
                                  int32 DurationMinutes, const FString& Reason,
                                  const FString& BannedBy)
    {
        UBanDatabase* DB = GetDB(Ctx);
        if (!DB)
        {
            Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
            return EExecutionStatus::UNCOMPLETED;
        }

        FString Platform, RawId;
        UBanDatabase::ParseUid(Uid, Platform, RawId);

        FBanEntry Entry;
        Entry.Uid        = Uid;
        Entry.PlayerUID  = RawId;
        Entry.Platform   = Platform;
        Entry.PlayerName = DisplayName;
        Entry.Reason     = Reason;
        Entry.BannedBy   = BannedBy;
        Entry.BanDate    = FDateTime::UtcNow();

        if (DurationMinutes <= 0)
        {
            Entry.bIsPermanent = true;
            Entry.ExpireDate   = FDateTime(0);
        }
        else
        {
            Entry.bIsPermanent = false;
            Entry.ExpireDate   = FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);
        }

        const FString DurStr = FormatDuration(DurationMinutes);
        if (DB->AddBan(Entry))
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Banned '%s' (%s: %s) %s — reason: %s"),
                    *DisplayName, *Platform, *RawId, *DurStr, *Reason),
                FLinearColor::Green);

            // Kick immediately if the player is currently connected.
            UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
            UBanEnforcer::KickConnectedPlayer(World, Uid, Entry.GetKickMessage());

            return EExecutionStatus::COMPLETED;
        }

        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Failed to ban %s '%s'."), *Platform, *RawId),
            FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

} // namespace BanChat

// ─────────────────────────────────────────────────────────────────────────────
//  ABanChatCommand  — /ban
// ─────────────────────────────────────────────────────────────────────────────

ABanChatCommand::ABanChatCommand()
{
    CommandName          = TEXT("ban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false; // allow console too
    Usage = NSLOCTEXT("BanChatCommands", "BanUsage",
        "/ban <player|Steam64|PUID> [reason...]");
}

EExecutionStatus ABanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Banned by server administrator");

    const FString BannedBy = Sender->GetSenderName();
    return BanChat::DoBan(this, Sender, Uid, DisplayName, 0, Reason, BannedBy);
}

// ─────────────────────────────────────────────────────────────────────────────
//  ATempBanChatCommand  — /tempban
// ─────────────────────────────────────────────────────────────────────────────

ATempBanChatCommand::ATempBanChatCommand()
{
    CommandName          = TEXT("tempban");
    MinNumberOfArguments = 2;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "TempBanUsage",
        "/tempban <player|Steam64|PUID> <minutes> [reason...]");
}

EExecutionStatus ATempBanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    if (!Arguments[1].IsNumeric())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Expected a number for <minutes>, got '%s'."), *Arguments[1]),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    const int32   DurationMinutes = FMath::Max(1, FCString::Atoi(*Arguments[1]));
    const FString Reason          = Arguments.Num() > 2
        ? BanChat::JoinArgs(Arguments, 2)
        : TEXT("Temporarily banned by server administrator");
    const FString BannedBy = Sender->GetSenderName();

    return BanChat::DoBan(this, Sender, Uid, DisplayName, DurationMinutes, Reason, BannedBy);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AUnbanChatCommand  — /unban
// ─────────────────────────────────────────────────────────────────────────────

AUnbanChatCommand::AUnbanChatCommand()
{
    CommandName          = TEXT("unban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "UnbanUsage",
        "/unban <Steam64|PUID>");
}

EExecutionStatus AUnbanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    const FString& Arg = Arguments[0];

    // Build the compound UID — accept Steam64 or EOS PUID only (exact IDs required for unban).
    FString Uid;
    if (BanChat::IsValidSteam64(Arg))
    {
        Uid = UBanDatabase::MakeUid(TEXT("STEAM"), Arg);
    }
    else if (BanChat::IsValidEOSPUID(Arg))
    {
        Uid = UBanDatabase::MakeUid(TEXT("EOS"), Arg.ToLower());
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid Steam64 ID (17 digits) or "
                "EOS PUID (32 hex chars). /unban requires an exact ID."), *Arg),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (DB->RemoveBanByUid(Uid))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Removed ban for %s."), *Uid),
            FLinearColor::Green);
        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] No active ban found for %s."), *Uid),
        FLinearColor::Yellow);
    return EExecutionStatus::UNCOMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ABanCheckChatCommand  — /bancheck
// ─────────────────────────────────────────────────────────────────────────────

ABanCheckChatCommand::ABanCheckChatCommand()
{
    CommandName          = TEXT("bancheck");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "BanCheckUsage",
        "/bancheck <player|Steam64|PUID>");
}

EExecutionStatus ABanCheckChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    FBanEntry Entry;
    if (DB->IsCurrentlyBanned(Uid, Entry))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] BANNED — %s (%s)  "
                "Reason: %s  Expires: %s  Banned by: %s"),
                *DisplayName, *Uid,
                *Entry.Reason, *BanChat::FormatExpiry(Entry), *Entry.BannedBy),
            FLinearColor::Red);
        return EExecutionStatus::COMPLETED;
    }

    // Check whether there is an expired ban record.
    if (DB->GetBanByUid(Uid, Entry) && Entry.IsExpired())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Ban for %s was expired and has been removed."), *Uid),
            FLinearColor::Yellow);
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Not banned: %s"), *Uid),
            FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ABanListChatCommand  — /banlist
// ─────────────────────────────────────────────────────────────────────────────

ABanListChatCommand::ABanListChatCommand()
{
    CommandName          = TEXT("banlist");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "BanListUsage", "/banlist [page]");
}

EExecutionStatus ABanListChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const TArray<FBanEntry> AllBans = DB->GetActiveBans();

    if (AllBans.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] No active bans."), FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    const int32 Page        = (Arguments.Num() > 0 && Arguments[0].IsNumeric())
        ? FMath::Max(1, FCString::Atoi(*Arguments[0])) : 1;
    const int32 TotalPages  = FMath::DivideAndRoundUp(AllBans.Num(), PageSize);
    const int32 PageClamped = FMath::Clamp(Page, 1, TotalPages);
    const int32 Start       = (PageClamped - 1) * PageSize;
    const int32 End         = FMath::Min(Start + PageSize, AllBans.Num());

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Active bans (%d total) — page %d/%d:"),
            AllBans.Num(), PageClamped, TotalPages),
        FLinearColor::White);

    for (int32 i = Start; i < End; ++i)
    {
        const FBanEntry& E = AllBans[i];
        Sender->SendChatMessage(
            FString::Printf(TEXT("  [%s] %s | %s | By: %s | Expires: %s"),
                *E.Platform, *E.PlayerUID,
                E.PlayerName.IsEmpty() ? TEXT("(unknown)") : *E.PlayerName,
                *E.BannedBy, *BanChat::FormatExpiry(E)),
            FLinearColor::White);
    }

    if (TotalPages > 1)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Use /banlist <page> to view more.")),
            FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AWhoAmIChatCommand  — /whoami
// ─────────────────────────────────────────────────────────────────────────────

AWhoAmIChatCommand::AWhoAmIChatCommand()
{
    CommandName          = TEXT("whoami");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = true; // requires a player; no IDs for console
    Usage = NSLOCTEXT("BanChatCommands", "WhoAmIUsage", "/whoami");
}

EExecutionStatus AWhoAmIChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer());
    if (!PC || !PC->PlayerState)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] No player state available."),
            FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
    if (!UniqueId.IsValid())
    {
        Sender->SendChatMessage(
            TEXT("[BanChatCommands] Could not resolve your platform identity. "
                 "Try again in a moment."),
            FLinearColor::Yellow);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString Platform = UniqueId->GetType().ToString().ToUpper();
    const FString RawId    = UniqueId->ToString();

    if (Platform == TEXT("STEAM"))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Your Steam64: %s"), *RawId),
            FLinearColor::Green);
    }
    else if (Platform == TEXT("EOS"))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Your EOS PUID: %s"), *RawId),
            FLinearColor::Green);
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Your platform ID (%s): %s"), *Platform, *RawId),
            FLinearColor::Green);
    }

    return EExecutionStatus::COMPLETED;
}
