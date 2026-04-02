// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanChatCommands.h"
#include "BanChatCommandsConfig.h"
#include "Command/CommandSender.h"
#include "BanIdResolver.h"
#include "BanPlayerLookup.h"
#include "BanEnforcementSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanChatCommands, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  Internal helpers
// ─────────────────────────────────────────────────────────────────────────────
namespace BanChat
{
    static USteamBanSubsystem* GetSteamBans(UObject* Ctx)
    {
        if (!Ctx) return nullptr;
        UWorld* World = Ctx->GetWorld();
        if (!World) return nullptr;
        UGameInstance* GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
    }

    static UEOSBanSubsystem* GetEOSBans(UObject* Ctx)
    {
        if (!Ctx) return nullptr;
        UWorld* World = Ctx->GetWorld();
        if (!World) return nullptr;
        UGameInstance* GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
    }

    static UBanEnforcementSubsystem* GetEnforcement(UObject* Ctx)
    {
        if (!Ctx) return nullptr;
        UWorld* World = Ctx->GetWorld();
        if (!World) return nullptr;
        UGameInstance* GI = World->GetGameInstance();
        return GI ? GI->GetSubsystem<UBanEnforcementSubsystem>() : nullptr;
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

    /**
     * Check whether the command sender is allowed to run admin commands.
     *
     * Console senders (non-player) are always permitted.  Player senders must
     * have their Steam64 ID listed in UBanChatCommandsConfig::AdminSteam64Ids.
     * When the list is empty, player access is denied (console-only mode).
     *
     * @param Sender  The command sender to check.
     * @param OutSteam64Id  Populated with the sender's Steam64 ID when IsPlayer is true.
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

        // Resolve sender name → their platform identity via connected player list.
        const FString SenderName = Sender->GetSenderName();
        UWorld* World = nullptr;
        if (APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer()))
            World = PC->GetWorld();

        if (World)
        {
            FResolvedBanId Ids;
            FString        FoundName;
            TArray<FString> Ambiguous;
            if (FBanPlayerLookup::FindPlayerByName(World, SenderName, Ids, FoundName, Ambiguous, true))
                OutSteam64Id = Ids.Steam64Id;
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
     * Resolve a player argument to a FResolvedBanId.
     *
     * Resolution order:
     *   1. 17-digit decimal → Steam64 ID.
     *   2. 32-char hex       → EOS PUID.
     *   3. Otherwise         → display-name lookup against connected players.
     *
     * Returns true on success and populates OutIds + OutDisplayName.
     */
    static bool ResolvePlayerArg(UObject* Ctx, UCommandSender* Sender,
                                 const FString& Arg,
                                 FResolvedBanId& OutIds,
                                 FString& OutDisplayName)
    {
        // 1. Raw Steam64 ID
        if (USteamBanSubsystem::IsValidSteam64Id(Arg))
        {
            OutIds.Steam64Id    = Arg;
            OutDisplayName = Arg;
            return true;
        }

        // 2. Raw EOS PUID
        if (UEOSBanSubsystem::IsValidEOSProductUserId(Arg))
        {
            OutIds.EOSProductUserId = Arg.ToLower();
            OutDisplayName      = Arg;
            return true;
        }

        // 3. Display-name lookup
        UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
        if (!World)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid Steam64 ID or EOS PUID."), *Arg),
                FLinearColor::Red);
            return false;
        }

        TArray<FString> Ambiguous;
        if (!FBanPlayerLookup::FindPlayerByName(World, Arg, OutIds, OutDisplayName, Ambiguous))
        {
            if (Ambiguous.Num() > 1)
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Ambiguous name '%s'. Matching players: %s"),
                        *Arg, *FString::Join(Ambiguous, TEXT(", "))),
                    FLinearColor::Yellow);
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] No connected player found matching '%s'. "
                        "Use a Steam64 ID or EOS PUID to target offline players."), *Arg),
                    FLinearColor::Red);
            }
            return false;
        }

        if (!OutIds.IsValid())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Found player '%s' but could not resolve "
                    "their platform identity."), *OutDisplayName),
                FLinearColor::Red);
            return false;
        }

        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Resolved '%s' → %s"),
                *Arg, OutIds.HasSteamId()
                    ? *FString::Printf(TEXT("Steam64: %s"), *OutIds.Steam64Id)
                    : *FString::Printf(TEXT("EOS PUID: %s"), *OutIds.EOSProductUserId)),
            FLinearColor::White);
        return true;
    }

    /**
     * Shared ban-and-propagate logic used by both /ban and /tempban.
     */
    static EExecutionStatus DoBan(UObject* Ctx, UCommandSender* Sender,
                                  const FResolvedBanId& Ids, const FString& DisplayName,
                                  int32 DurationMinutes, const FString& Reason,
                                  const FString& BannedBy)
    {
        const FString DurStr = FormatDuration(DurationMinutes);
        bool bBanned = false;

        if (Ids.HasSteamId())
        {
            USteamBanSubsystem* Steam = GetSteamBans(Ctx);
            if (!Steam)
            {
                Sender->SendChatMessage(TEXT("[BanChatCommands] USteamBanSubsystem unavailable."), FLinearColor::Red);
                return EExecutionStatus::UNCOMPLETED;
            }
            if (Steam->BanPlayer(Ids.Steam64Id, Reason, DurationMinutes, BannedBy))
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Banned '%s' (Steam64: %s) %s — reason: %s"),
                        *DisplayName, *Ids.Steam64Id, *DurStr, *Reason),
                    FLinearColor::Green);
                bBanned = true;

                // Cross-platform propagation
                if (UBanEnforcementSubsystem* Enf = GetEnforcement(Ctx))
                    Enf->PropagateToEOSAsync(Ids.Steam64Id, Reason, DurationMinutes, BannedBy);
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Failed to ban Steam64 '%s'."), *Ids.Steam64Id),
                    FLinearColor::Red);
            }
        }
        else if (Ids.HasEOSPuid())
        {
            UEOSBanSubsystem* EOS = GetEOSBans(Ctx);
            if (!EOS)
            {
                Sender->SendChatMessage(TEXT("[BanChatCommands] UEOSBanSubsystem unavailable."), FLinearColor::Red);
                return EExecutionStatus::UNCOMPLETED;
            }
            if (EOS->BanPlayer(Ids.EOSProductUserId, Reason, DurationMinutes, BannedBy))
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Banned '%s' (EOS PUID: %s) %s — reason: %s"),
                        *DisplayName, *Ids.EOSProductUserId, *DurStr, *Reason),
                    FLinearColor::Green);
                bBanned = true;

                // Cross-platform propagation
                if (UBanEnforcementSubsystem* Enf = GetEnforcement(Ctx))
                    Enf->PropagateToSteamAsync(Ids.EOSProductUserId, Reason, DurationMinutes, BannedBy);
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Failed to ban EOS PUID '%s'."), *Ids.EOSProductUserId),
                    FLinearColor::Red);
            }
        }

        return bBanned ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
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

    FResolvedBanId Ids;
    FString        DisplayName;
    if (!BanChat::ResolvePlayerArg(this, Sender, Arguments[0], Ids, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Banned by server administrator");

    const FString BannedBy = Sender->GetSenderName();
    return BanChat::DoBan(this, Sender, Ids, DisplayName, 0, Reason, BannedBy);
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

    FResolvedBanId Ids;
    FString        DisplayName;
    if (!BanChat::ResolvePlayerArg(this, Sender, Arguments[0], Ids, DisplayName))
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

    return BanChat::DoBan(this, Sender, Ids, DisplayName, DurationMinutes, Reason, BannedBy);
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
    bool bRemovedAny = false;

    // Try Steam unban
    if (USteamBanSubsystem::IsValidSteam64Id(Arg))
    {
        if (USteamBanSubsystem* Steam = BanChat::GetSteamBans(this))
        {
            if (Steam->UnbanPlayer(Arg))
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Removed Steam ban for %s."), *Arg),
                    FLinearColor::Green);
                bRemovedAny = true;
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] No active Steam ban found for %s."), *Arg),
                    FLinearColor::Yellow);
            }
        }
        return bRemovedAny ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
    }

    // Try EOS unban
    if (UEOSBanSubsystem::IsValidEOSProductUserId(Arg))
    {
        const FString PuidLower = Arg.ToLower();
        if (UEOSBanSubsystem* EOS = BanChat::GetEOSBans(this))
        {
            if (EOS->UnbanPlayer(PuidLower))
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Removed EOS ban for %s."), *PuidLower),
                    FLinearColor::Green);
                bRemovedAny = true;
            }
            else
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] No active EOS ban found for %s."), *PuidLower),
                    FLinearColor::Yellow);
            }
        }
        return bRemovedAny ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid Steam64 ID (17 digits) or "
            "EOS PUID (32 hex chars). /unban requires an exact ID."), *Arg),
        FLinearColor::Red);
    return EExecutionStatus::BAD_ARGUMENTS;
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

    FResolvedBanId Ids;
    FString        DisplayName;
    if (!BanChat::ResolvePlayerArg(this, Sender, Arguments[0], Ids, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    bool bFoundAny = false;

    if (Ids.HasSteamId())
    {
        if (USteamBanSubsystem* Steam = BanChat::GetSteamBans(this))
        {
            FBanEntry Entry;
            const EBanCheckResult Result = Steam->CheckPlayerBan(Ids.Steam64Id, Entry);
            switch (Result)
            {
            case EBanCheckResult::Banned:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] STEAM BANNED — %s (Steam64: %s)  "
                        "Reason: %s  Expires: %s  Banned by: %s"),
                        *DisplayName, *Ids.Steam64Id,
                        *Entry.Reason, *Entry.GetExpiryString(), *Entry.BannedBy),
                    FLinearColor::Red);
                bFoundAny = true;
                break;
            case EBanCheckResult::BanExpired:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Steam ban for %s was expired and removed."),
                        *Ids.Steam64Id),
                    FLinearColor::Yellow);
                bFoundAny = true;
                break;
            case EBanCheckResult::NotBanned:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Not Steam-banned: %s"), *Ids.Steam64Id),
                    FLinearColor::White);
                break;
            }
        }
    }

    if (Ids.HasEOSPuid())
    {
        if (UEOSBanSubsystem* EOS = BanChat::GetEOSBans(this))
        {
            FBanEntry Entry;
            const EBanCheckResult Result = EOS->CheckPlayerBan(Ids.EOSProductUserId, Entry);
            switch (Result)
            {
            case EBanCheckResult::Banned:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] EOS BANNED — %s (PUID: %s)  "
                        "Reason: %s  Expires: %s  Banned by: %s"),
                        *DisplayName, *Ids.EOSProductUserId,
                        *Entry.Reason, *Entry.GetExpiryString(), *Entry.BannedBy),
                    FLinearColor::Red);
                bFoundAny = true;
                break;
            case EBanCheckResult::BanExpired:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] EOS ban for %s was expired and removed."),
                        *Ids.EOSProductUserId),
                    FLinearColor::Yellow);
                bFoundAny = true;
                break;
            case EBanCheckResult::NotBanned:
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Not EOS-banned: %s"), *Ids.EOSProductUserId),
                    FLinearColor::White);
                break;
            }
        }
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

    // Collect combined ban list: Steam entries tagged "S", EOS tagged "E".
    struct FBanRow { FString Tag; FBanEntry Entry; };
    TArray<FBanRow> AllBans;

    if (USteamBanSubsystem* Steam = BanChat::GetSteamBans(this))
    {
        for (const FBanEntry& E : Steam->GetAllBans())
            AllBans.Add({ TEXT("S"), E });
    }
    if (UEOSBanSubsystem* EOS = BanChat::GetEOSBans(this))
    {
        for (const FBanEntry& E : EOS->GetAllBans())
            AllBans.Add({ TEXT("E"), E });
    }

    if (AllBans.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] No active bans."), FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    const int32 Page      = (Arguments.Num() > 0 && Arguments[0].IsNumeric())
        ? FMath::Max(1, FCString::Atoi(*Arguments[0])) : 1;
    const int32 TotalPages = FMath::DivideAndRoundUp(AllBans.Num(), PageSize);
    const int32 PageClamped = FMath::Clamp(Page, 1, TotalPages);
    const int32 Start       = (PageClamped - 1) * PageSize;
    const int32 End         = FMath::Min(Start + PageSize, AllBans.Num());

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Active bans (%d total) — page %d/%d:"),
            AllBans.Num(), PageClamped, TotalPages),
        FLinearColor::White);

    for (int32 i = Start; i < End; ++i)
    {
        const FBanRow& Row = AllBans[i];
        Sender->SendChatMessage(
            FString::Printf(TEXT("  [%s] %s | %s | By: %s | Expires: %s"),
                *Row.Tag, *Row.Entry.PlayerId, *Row.Entry.Reason,
                *Row.Entry.BannedBy, *Row.Entry.GetExpiryString()),
            FLinearColor::White);
    }

    if (TotalPages > 1)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Use /banlist <page> to view more. "
                "(S=Steam, E=EOS)")),
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
    UWorld* World = nullptr;
    if (APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer()))
        World = PC->GetWorld();

    if (!World)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] No world context available."),
            FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    // Look up the caller's own IDs by their display name (exact match).
    const FString SenderName = Sender->GetSenderName();
    FResolvedBanId Ids;
    FString        FoundName;
    TArray<FString> Ambiguous;

    if (!FBanPlayerLookup::FindPlayerByName(World, SenderName, Ids, FoundName, Ambiguous, true))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Could not resolve your identity (name: '%s'). "
                "Try again in a moment."), *SenderName),
            FLinearColor::Yellow);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (Ids.HasSteamId())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Your Steam64: %s"), *Ids.Steam64Id),
            FLinearColor::Green);
    }
    else
    {
        Sender->SendChatMessage(
            TEXT("[BanChatCommands] Steam64: not available (non-Steam connection)"),
            FLinearColor::Yellow);
    }

    if (Ids.HasEOSPuid())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Your EOS PUID: %s"), *Ids.EOSProductUserId),
            FLinearColor::Green);
    }
    else
    {
        // Check the EOSSystem sync cache for a Steam64→PUID mapping.
        UGameInstance* GI = World->GetGameInstance();
        FString CachedPUID;
        if (Ids.HasSteamId() &&
            FBanIdResolver::TryGetCachedPUIDFromSteam64(GI, Ids.Steam64Id, CachedPUID))
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Your EOS PUID: %s (cached)"), *CachedPUID),
                FLinearColor::Green);
        }
        else
        {
            Sender->SendChatMessage(
                TEXT("[BanChatCommands] EOS PUID: not yet resolved. "
                     "Try again in a few seconds or ask the server admin."),
                FLinearColor::Yellow);
        }
    }

    return EExecutionStatus::COMPLETED;
}
