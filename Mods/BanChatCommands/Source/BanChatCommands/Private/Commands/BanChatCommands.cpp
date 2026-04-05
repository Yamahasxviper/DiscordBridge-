// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanChatCommands.h"
#include "BanChatCommandsConfig.h"
#include "Command/CommandSender.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanTypes.h"
#include "PlayerSessionRegistry.h"
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
     * have their EOS PUID listed in UBanChatCommandsConfig::AdminEosPUIDs.
     * When the list is empty, player access is denied (console-only mode).
     *
     * Note: On CSS Dedicated Server all players are identified by their EOS
     * Product User ID regardless of launch platform (Steam, Epic, etc.).
     *
     * @param Sender  The command sender to check.
     * @param OutUid  Populated with the sender's compound UID when they connect via a known platform.
     * @return true when the sender is authorised to run admin commands.
     */
    static bool IsAdminSender(UCommandSender* Sender, FString& OutUid)
    {
        if (!Sender->IsPlayerSender())
        {
            // Console / server operator — always allowed.
            OutUid.Reset();
            return true;
        }

        // Resolve the sender's platform identity directly from their PlayerState.
        APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer());
        if (PC && PC->PlayerState)
        {
            const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
            // Use direct FUniqueNetIdRepl member accessors — do NOT dereference
            // via operator-> (UniqueId->GetType() / UniqueId->ToString()).
            // On CSS DS with EOS V2 PUIDs the inner TSharedPtr slot holds a raw
            // EOS handle, not a valid C++ FUniqueNetId object; operator-> returns
            // a garbage pointer that crashes.  FUniqueNetIdRepl::GetType() and
            // FUniqueNetIdRepl::ToString() are safe for EOS PUID (V2) identities.
            // Guard against a NONE-type identity that IsValid() can return true for
            // before the EOS PUID provider assigns it (GetType()=="NONE", ToString()=="").
            if (UniqueId.IsValid() && UniqueId.GetType() != FName(TEXT("NONE")))
            {
                // CSS DS exclusively assigns EOS PUIDs — always build an EOS compound UID,
                // normalized to lowercase to match bans stored via /ban and /tempban.
                OutUid = UBanDatabase::MakeUid(TEXT("EOS"), UniqueId.ToString().ToLower());
            }
            else
            {
                // CSS DS 1.1.0 workaround: GetType()==NONE because EOS online
                // subsystem is offline.  Extract the EOS PUID from the connection
                // URL's ClientIdentity option instead.
                const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);
                if (!EosPuid.IsEmpty())
                    OutUid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
            }
        }

        const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
        if (!Cfg || !Cfg->IsAdminUid(OutUid))
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
     *   1. 32-char hex       → EOS PUID   → UID "EOS:xxx"
     *   2. Otherwise         → display-name substring lookup against connected players.
     *
     * Returns true on success and populates OutUid + OutDisplayName.
     */
    static bool ResolveTarget(UObject* Ctx, UCommandSender* Sender,
                              const FString& Arg,
                              FString& OutUid,
                              FString& OutDisplayName)
    {
        // 1. Raw EOS PUID (32-char hex)
        if (IsValidEOSPUID(Arg))
        {
            const FString Lower = Arg.ToLower();
            OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), Lower);
            OutDisplayName = Lower;
            return true;
        }

        // 2. Compound UID supplied directly, e.g. "EOS:<32hex>" — resolve without
        //    requiring the player to be currently connected (offline ban / pre-ban).
        {
            FString Platform, RawId;
            UBanDatabase::ParseUid(Arg, Platform, RawId);
            if (Platform == TEXT("EOS") && IsValidEOSPUID(RawId))
            {
                OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), RawId.ToLower());
                OutDisplayName = RawId.ToLower();
                return true;
            }
        }

        // 3. Display-name lookup against connected PlayerControllers.
        UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
        if (!World)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid EOS PUID."), *Arg),
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
                    "Use an EOS PUID to target offline players."), *Arg),
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
        // Guard against NONE-type: IsValid() can return true before the EOS PUID
        // provider assigns the identity (GetType()=="NONE", ToString()=="").
        // Also use direct member accessors — NOT operator-> — to avoid a crash on
        // CSS DS with EOS V2 PUIDs (see IsAdminSender for the full explanation).
        if (!UniqueId.IsValid() || UniqueId.GetType() == FName(TEXT("NONE")))
        {
            // CSS DS 1.1.0 workaround: GetType()==NONE because the EOS online
            // subsystem is offline (IsOnline=false).  The EOS PUID is still
            // transmitted in the ClientIdentity URL option.  Try that before
            // giving up — this is the same fallback used by IsAdminSender,
            // OnPostLogin, ProcessPendingBanChecks, and KickConnectedPlayer.
            const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(FoundPC);
            if (!EosPuid.IsEmpty())
            {
                OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
                OutDisplayName = Matches[0];
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Resolved '%s' → EOS: %s (via connection URL)"),
                        *Arg, *EosPuid),
                    FLinearColor::White);
                return true;
            }

            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Found player '%s' but could not resolve "
                    "their platform identity."), *Matches[0]),
                FLinearColor::Red);
            return false;
        }

        // CSS DS exclusively assigns EOS PUIDs — always build an EOS compound UID,
        // normalized to lowercase to match bans stored via /ban or /tempban.
        const FString RawId = UniqueId.ToString().ToLower();
        OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), RawId);
        OutDisplayName = Matches[0];

        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Resolved '%s' → EOS: %s"),
                *Arg, *RawId),
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

    /**
     * Validates and normalises a compound UID argument entered by the user.
     *
     * Accepts:
     *   "EOS:<32-hex>"  → returned with lowercase hex part
     *   "<32-hex>"      → "EOS:" prefix added automatically
     *
     * On failure, sends an error to the sender and returns false.
     */
    static bool ParseAndNormaliseUidArg(UCommandSender* Sender,
                                        const FString& Arg,
                                        FString& OutUid)
    {
        // Try to parse an explicit compound UID supplied by the user (e.g. "EOS:xxx").
        FString Platform, RawId;
        UBanDatabase::ParseUid(Arg, Platform, RawId);

        if (Platform == TEXT("EOS") && IsValidEOSPUID(RawId))
        {
            OutUid = UBanDatabase::MakeUid(TEXT("EOS"), RawId.ToLower());
            return true;
        }

        // Accept raw EOS PUID without prefix.
        if (IsValidEOSPUID(Arg))
        {
            OutUid = UBanDatabase::MakeUid(TEXT("EOS"), Arg.ToLower());
            return true;
        }

        if (Sender)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid compound UID "
                    "(EOS:<32hex>)."), *Arg),
                FLinearColor::Red);
        }
        return false;
    }

} // namespace BanChat

// ─────────────────────────────────────────────────────────────────────────────
//  ALinkBansChatCommand  — /linkbans
// ─────────────────────────────────────────────────────────────────────────────

ALinkBansChatCommand::ALinkBansChatCommand()
{
    CommandName          = TEXT("linkbans");
    MinNumberOfArguments = 2;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "LinkBansUsage",
        "/linkbans <UID1> <UID2>");
}

EExecutionStatus ALinkBansChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString UidA, UidB;
    if (!BanChat::ParseAndNormaliseUidArg(Sender, Arguments[0], UidA))
        return EExecutionStatus::BAD_ARGUMENTS;
    if (!BanChat::ParseAndNormaliseUidArg(Sender, Arguments[1], UidB))
        return EExecutionStatus::BAD_ARGUMENTS;

    if (UidA.Equals(UidB, ESearchCase::IgnoreCase))
    {
        Sender->SendChatMessage(
            TEXT("[BanChatCommands] Cannot link a UID to itself."),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (DB->LinkBans(UidA, UidB))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Linked %s ↔ %s. "
                "A ban on either identity will now block both."), *UidA, *UidB),
            FLinearColor::Green);
        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Could not link — no ban record found for '%s' or '%s'. "
            "Both UIDs must have existing ban records before they can be linked."),
            *UidA, *UidB),
        FLinearColor::Red);
    return EExecutionStatus::UNCOMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AUnlinkBansChatCommand  — /unlinkbans
// ─────────────────────────────────────────────────────────────────────────────

AUnlinkBansChatCommand::AUnlinkBansChatCommand()
{
    CommandName          = TEXT("unlinkbans");
    MinNumberOfArguments = 2;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "UnlinkBansUsage",
        "/unlinkbans <UID1> <UID2>");
}

EExecutionStatus AUnlinkBansChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString UidA, UidB;
    if (!BanChat::ParseAndNormaliseUidArg(Sender, Arguments[0], UidA))
        return EExecutionStatus::BAD_ARGUMENTS;
    if (!BanChat::ParseAndNormaliseUidArg(Sender, Arguments[1], UidB))
        return EExecutionStatus::BAD_ARGUMENTS;

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (DB->UnlinkBans(UidA, UidB))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Removed link between %s and %s."), *UidA, *UidB),
            FLinearColor::Green);
        return EExecutionStatus::COMPLETED;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] No link found between %s and %s."), *UidA, *UidB),
        FLinearColor::Yellow);
    return EExecutionStatus::UNCOMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ABanChatCommand  — /ban
// ─────────────────────────────────────────────────────────────────────────────

ABanChatCommand::ABanChatCommand()
{
    CommandName          = TEXT("ban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false; // allow console too
    Usage = NSLOCTEXT("BanChatCommands", "BanUsage",
        "/ban <player|PUID> [reason...]");
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
        "/tempban <player|PUID> <minutes> [reason...]");
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
        "/unban <PUID>");
}

EExecutionStatus AUnbanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    const FString& Arg = Arguments[0];

    // Build the compound UID — accept EOS PUID only (exact ID required for unban).
    FString Uid;
    if (BanChat::IsValidEOSPUID(Arg))
    {
        Uid = UBanDatabase::MakeUid(TEXT("EOS"), Arg.ToLower());
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid "
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
        "/bancheck <player|PUID>");
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
    // Guard against NONE-type and use direct member accessors — NOT operator->
    // (see IsAdminSender for the full explanation on why operator-> crashes on
    // CSS DS with EOS V2 PUIDs).
    FString RawId;
    if (UniqueId.IsValid() && UniqueId.GetType() != FName(TEXT("NONE")))
    {
        // CSS DS exclusively assigns EOS PUIDs — always normalize to lowercase.
        RawId = UniqueId.ToString().ToLower();
    }
    else
    {
        // CSS DS 1.1.0 workaround: GetType()==NONE because EOS online subsystem
        // is offline (IsOnline=false).  The EOS PUID is still embedded in the
        // ClientIdentity URL option of the player's connection — use it.
        const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);
        if (EosPuid.IsEmpty())
        {
            Sender->SendChatMessage(
                TEXT("[BanChatCommands] Could not resolve your platform identity. "
                     "Try again in a moment."),
                FLinearColor::Yellow);
            return EExecutionStatus::UNCOMPLETED;
        }
        RawId = EosPuid;
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Your EOS PUID: %s"), *RawId),
        FLinearColor::Green);

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  APlayerHistoryChatCommand  — /playerhistory
// ─────────────────────────────────────────────────────────────────────────────

APlayerHistoryChatCommand::APlayerHistoryChatCommand()
{
    CommandName          = TEXT("playerhistory");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "PlayerHistoryUsage",
        "/playerhistory <name_substring|UID>");
}

EExecutionStatus APlayerHistoryChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UPlayerSessionRegistry* Registry = GI->GetSubsystem<UPlayerSessionRegistry>();
    if (!Registry)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] PlayerSessionRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString& Arg = Arguments[0];

    // Determine whether the argument looks like a compound UID (contains ':')
    // or a display-name substring.
    FString SearchUid;
    const bool bIsUid = BanChat::ParseAndNormaliseUidArg(nullptr, Arg, SearchUid);

    TArray<FPlayerSessionRecord> Results;
    if (bIsUid)
    {
        // Look up by UID — show the recorded display name.
        FPlayerSessionRecord Rec;
        if (Registry->FindByUid(SearchUid, Rec))
        {
            Results.Add(Rec);
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No session history found for UID %s."), *SearchUid),
                FLinearColor::Yellow);
            return EExecutionStatus::COMPLETED;
        }
    }
    else
    {
        // Look up by display-name substring.
        Results = Registry->FindByName(Arg);
        if (Results.IsEmpty())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No session history found for name '%s'."), *Arg),
                FLinearColor::Yellow);
            return EExecutionStatus::COMPLETED;
        }
    }

    if (Results.Num() > MaxResults)
        Results.SetNum(MaxResults);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Session history for '%s' (%d result(s)):"), *Arg, Results.Num()),
        FLinearColor::White);

    UBanDatabase* DB = BanChat::GetDB(this);

    for (const FPlayerSessionRecord& Rec : Results)
    {
        FBanEntry BanEntry;
        const bool bBanned = DB && DB->IsCurrentlyBannedByAnyId(Rec.Uid, BanEntry);
        const FString BanStatus = bBanned
            ? FString::Printf(TEXT(" [BANNED: %s]"), *BanEntry.Reason)
            : TEXT("");

        Sender->SendChatMessage(
            FString::Printf(TEXT("  %s | \"%s\" | last seen: %s%s"),
                *Rec.Uid, *Rec.DisplayName, *Rec.LastSeen, *BanStatus),
            bBanned ? FLinearColor::Red : FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}
