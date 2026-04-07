// Copyright Yamahasxviper. All Rights Reserved.

#include "Commands/BanChatCommands.h"
#include "BanChatCommandsConfig.h"
#include "BanChatCommandsModule.h"
#include "Command/CommandSender.h"
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanTypes.h"
#include "MuteRegistry.h"
#include "PlayerSessionRegistry.h"
#include "PlayerWarningRegistry.h"
#include "BanSystemConfig.h"
#include "BanDiscordNotifier.h"
#include "Engine/GameInstance.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/OnlineReplStructs.h"
// Full definition required for Cast<> between AFGPlayerController and APlayerController
// (UE Cast<> rejects incomplete types via static_assert in Casts.h).
#include "FGPlayerController.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"

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
     * Given a lowercase EOS PUID, attempts to resolve the player's real display name.
     *
     * Resolution order:
     *   1. Currently-connected PlayerController whose EOS PUID matches.
     *   2. PlayerSessionRegistry (persisted from previous sessions).
     *   3. Falls back to the raw PUID string when neither source has a name.
     */
    static FString ResolveDisplayNameForPuid(UObject* Ctx, const FString& LowerPuid)
    {
        UWorld* World = Ctx ? Ctx->GetWorld() : nullptr;
        if (!World) return LowerPuid;

        // 1. Check currently-connected players.
        for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
        {
            APlayerController* PC = It->Get();
            if (!PC || !PC->PlayerState) continue;

            const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
            FString ConnectedPuid;
            if (UniqueId.IsValid() && UniqueId.GetType() != FName(TEXT("NONE")))
                ConnectedPuid = UniqueId.ToString().ToLower();
            else
                ConnectedPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);

            if (!ConnectedPuid.IsEmpty() && ConnectedPuid == LowerPuid)
                return PC->PlayerState->GetPlayerName();
        }

        // 2. Check the session registry for a previously-seen display name.
        UGameInstance* GI = World->GetGameInstance();
        UPlayerSessionRegistry* Registry = GI ? GI->GetSubsystem<UPlayerSessionRegistry>() : nullptr;
        if (Registry)
        {
            FPlayerSessionRecord Record;
            if (Registry->FindByUid(UBanDatabase::MakeUid(TEXT("EOS"), LowerPuid), Record)
                && !Record.DisplayName.IsEmpty())
            {
                return Record.DisplayName;
            }
        }

        // 3. No name found — fall back to the raw PUID.
        return LowerPuid;
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
            OutDisplayName = ResolveDisplayNameForPuid(Ctx, Lower);
            return true;
        }

        // 2. Compound UID supplied directly:
        //      "EOS:<32hex>"  → resolve without requiring player to be connected
        //      "IP:<address>" → IP ban UID (e.g. from /banname or manual entry)
        {
            FString Platform, RawId;
            UBanDatabase::ParseUid(Arg, Platform, RawId);
            if (Platform == TEXT("EOS") && IsValidEOSPUID(RawId))
            {
                const FString Lower = RawId.ToLower();
                OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), Lower);
                OutDisplayName = ResolveDisplayNameForPuid(Ctx, Lower);
                return true;
            }
            if (Platform == TEXT("IP") && !RawId.IsEmpty())
            {
                OutUid         = UBanDatabase::MakeUid(TEXT("IP"), RawId);
                OutDisplayName = RawId;
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
     *   "IP:<address>"  → IP ban UID (IPv4 or IPv6)
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

        if (Platform == TEXT("IP") && !RawId.IsEmpty())
        {
            OutUid = UBanDatabase::MakeUid(TEXT("IP"), RawId);
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
                    "(EOS:<32hex> or IP:<address>)."), *Arg),
                FLinearColor::Red);
        }
        return false;
    }

    /**
     * Check whether the command sender is allowed to run moderator commands.
     *
     * Console senders are always permitted.  Player senders must have their EOS
     * PUID listed in either AdminEosPUIDs or ModeratorEosPUIDs.
     */
    static bool IsModeratorSender(UCommandSender* Sender, FString& OutUid)
    {
        if (!Sender->IsPlayerSender())
        {
            OutUid.Reset();
            return true;
        }

        APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer());
        if (PC && PC->PlayerState)
        {
            const FUniqueNetIdRepl& UniqueId = PC->PlayerState->GetUniqueId();
            if (UniqueId.IsValid() && UniqueId.GetType() != FName(TEXT("NONE")))
            {
                OutUid = UBanDatabase::MakeUid(TEXT("EOS"), UniqueId.ToString().ToLower());
            }
            else
            {
                const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);
                if (!EosPuid.IsEmpty())
                    OutUid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
            }
        }

        const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
        if (!Cfg || !Cfg->IsModeratorUid(OutUid))
        {
            Sender->SendChatMessage(
                TEXT("[BanChatCommands] You do not have permission to use this command."),
                FLinearColor::Red);
            return false;
        }
        return true;
    }

    /**
     * Parse a duration string into a number of minutes.
     *
     * Accepts:
     *   - Bare integers       → treated as minutes (e.g. "60")
     *   - Shorthand strings   → combinations of d/h/m suffixes
     *     (e.g. "30m", "1h", "2h30m", "1d", "7d", "1d12h")
     *
     * Returns -1 if the format is invalid.
     */
    static int32 ParseDurationMinutes(const FString& DurationStr)
    {
        if (DurationStr.IsEmpty()) return -1;

        // Bare integer — legacy format, treat as minutes.
        if (DurationStr.IsNumeric())
            return FMath::Max(1, FCString::Atoi(*DurationStr));

        int32 Total   = 0;
        bool  bHadToken = false;
        const TCHAR* p = *DurationStr;

        while (*p)
        {
            if (!FChar::IsDigit(*p)) return -1;

            int32 Num = 0;
            while (*p && FChar::IsDigit(*p))
            {
                Num = Num * 10 + (*p - TEXT('0'));
                ++p;
            }

            if (!*p) return -1; // digits without a trailing unit

            const TCHAR Unit = FChar::ToLower(*p);
            ++p;

            if      (Unit == TEXT('d')) Total += Num * 1440;
            else if (Unit == TEXT('h')) Total += Num * 60;
            else if (Unit == TEXT('m')) Total += Num;
            else return -1;

            bHadToken = true;
        }

        if (!bHadToken || Total <= 0) return -1;
        return Total;
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
        "/ban <player|PUID|IP:address> [reason...]");
}

EExecutionStatus ABanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    const FString& Arg = Arguments[0];
    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arg, Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Banned by server administrator");

    const FString BannedBy = Sender->GetSenderName();
    const EExecutionStatus BanResult = BanChat::DoBan(this, Sender, Uid, DisplayName, 0, Reason, BannedBy);
    if (BanResult != EExecutionStatus::COMPLETED)
        return BanResult;

    // If the argument was a display name (not a raw PUID or compound UID), also
    // ban the player's recorded IP address and link the two ban records together.
    {
        FString TmpPlatform, TmpRawId;
        UBanDatabase::ParseUid(Arg, TmpPlatform, TmpRawId);
        const bool bWasNameResolution = !BanChat::IsValidEOSPUID(Arg)
            && TmpPlatform == TEXT("UNKNOWN");

        if (bWasNameResolution)
        {
            UWorld* World = GetWorld();
            UGameInstance* GI = World ? World->GetGameInstance() : nullptr;
            UPlayerSessionRegistry* Registry = GI
                ? GI->GetSubsystem<UPlayerSessionRegistry>() : nullptr;
            UBanDatabase* DB = BanChat::GetDB(this);

            if (Registry && DB)
            {
                FPlayerSessionRecord Rec;
                const bool bFound = Registry->FindByUid(Uid, Rec);
                if (bFound && !Rec.IpAddress.IsEmpty())
                {
                    const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Rec.IpAddress);

                    FBanEntry IpEntry;
                    IpEntry.Uid          = IpUid;
                    IpEntry.PlayerUID    = Rec.IpAddress;
                    IpEntry.Platform     = TEXT("IP");
                    IpEntry.PlayerName   = DisplayName;
                    IpEntry.Reason       = Reason;
                    IpEntry.BannedBy     = BannedBy;
                    IpEntry.BanDate      = FDateTime::UtcNow();
                    IpEntry.bIsPermanent = true;
                    IpEntry.ExpireDate   = FDateTime(0);

                    if (DB->AddBan(IpEntry))
                    {
                        DB->LinkBans(Uid, IpUid);
                        Sender->SendChatMessage(
                            FString::Printf(TEXT("[BanChatCommands] Also banned IP %s — linked to EOS ban."),
                                *Rec.IpAddress),
                            FLinearColor::Green);
                    }
                    else
                    {
                        Sender->SendChatMessage(
                            FString::Printf(TEXT("[BanChatCommands] Warning: failed to add IP ban for %s."),
                                *Rec.IpAddress),
                            FLinearColor::Yellow);
                    }
                }
                else if (bFound)
                {
                    Sender->SendChatMessage(
                        TEXT("[BanChatCommands] No IP address on record for this player — EOS PUID ban applied only."),
                        FLinearColor::Yellow);
                }
            }
        }
    }

    return EExecutionStatus::COMPLETED;
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
        "/tempban <player|PUID|IP:address> <duration> [reason...]\n"
        "  duration: minutes (e.g. 60) or shorthand (e.g. 30m, 1h, 2h30m, 1d, 7d, 1d12h)");
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

    const int32 DurationMinutes = BanChat::ParseDurationMinutes(Arguments[1]);
    if (DurationMinutes < 0)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Invalid duration '%s'. "
                "Use minutes (e.g. 60) or shorthand (e.g. 30m, 1h, 2h30m, 1d, 7d, 1d12h)."),
                *Arguments[1]),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    const FString Reason   = Arguments.Num() > 2
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
        "/unban <PUID|IP:address>");
}

EExecutionStatus AUnbanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    const FString& Arg = Arguments[0];

    // Build the compound UID — accept EOS PUID or IP:<address> (exact ID required for unban).
    FString Uid;
    if (BanChat::IsValidEOSPUID(Arg))
    {
        Uid = UBanDatabase::MakeUid(TEXT("EOS"), Arg.ToLower());
    }
    else
    {
        FString Platform, RawId;
        UBanDatabase::ParseUid(Arg, Platform, RawId);
        if (Platform == TEXT("IP") && !RawId.IsEmpty())
        {
            Uid = UBanDatabase::MakeUid(TEXT("IP"), RawId);
        }
        else if (Platform == TEXT("EOS") && BanChat::IsValidEOSPUID(RawId))
        {
            Uid = UBanDatabase::MakeUid(TEXT("EOS"), RawId.ToLower());
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' is not a valid "
                    "UID. Use an EOS PUID (32 hex chars) or IP:<address>."), *Arg),
                FLinearColor::Red);
            return EExecutionStatus::BAD_ARGUMENTS;
        }
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
//  AUnbanNameChatCommand  — /unbanname
// ─────────────────────────────────────────────────────────────────────────────

AUnbanNameChatCommand::AUnbanNameChatCommand()
{
    CommandName          = TEXT("unbanname");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "UnbanNameUsage",
        "/unbanname <name_substring>");
}

EExecutionStatus AUnbanNameChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
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

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString NameArg = Arguments[0];
    TArray<FPlayerSessionRecord> Results = Registry->FindByName(NameArg);

    if (Results.IsEmpty())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No session history found for '%s'. "
                "The player must have connected at least once for /unbanname to work.  "
                "Use /unban <PUID> to unban a player directly."), *NameArg),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    if (Results.Num() > 1)
    {
        TArray<FString> Descriptions;
        for (const FPlayerSessionRecord& R : Results)
            Descriptions.Add(FString::Printf(TEXT("\"%s\" (%s)"), *R.DisplayName, *R.Uid));
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Ambiguous name '%s' — %d matches: %s.  "
                "Use a more specific substring."),
                *NameArg, Results.Num(), *FString::Join(Descriptions, TEXT(", "))),
            FLinearColor::Yellow);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    const FPlayerSessionRecord& Rec = Results[0];
    bool bAnyRemoved = false;

    // Remove the EOS PUID ban.
    if (DB->RemoveBanByUid(Rec.Uid))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Removed EOS ban for '%s' (%s)."),
                *Rec.DisplayName, *Rec.Uid),
            FLinearColor::Green);
        bAnyRemoved = true;
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No active EOS ban found for '%s' (%s)."),
                *Rec.DisplayName, *Rec.Uid),
            FLinearColor::Yellow);
    }

    // Remove the IP ban if there is a recorded IP address.
    if (!Rec.IpAddress.IsEmpty())
    {
        const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Rec.IpAddress);
        if (DB->RemoveBanByUid(IpUid))
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Also removed IP ban for %s."),
                    *Rec.IpAddress),
                FLinearColor::Green);
            bAnyRemoved = true;
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No active IP ban found for %s."),
                    *Rec.IpAddress),
                FLinearColor::Yellow);
        }
    }

    return bAnyRemoved ? EExecutionStatus::COMPLETED : EExecutionStatus::UNCOMPLETED;
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
        "/bancheck <player|PUID|IP:address>");
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

    const int32 PageSize    = UBanChatCommandsConfig::Get()
        ? FMath::Clamp(UBanChatCommandsConfig::Get()->BanListPageSize, 1, 50)
        : 10;
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
        "/playerhistory <name_substring|UID|IP>");
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

    // Determine lookup mode: compound UID, IP address/prefix, or name substring.
    FString ParsePlatform, ParseRawId;
    UBanDatabase::ParseUid(Arg, ParsePlatform, ParseRawId);

    TArray<FPlayerSessionRecord> Results;
    if (ParsePlatform == TEXT("EOS") && BanChat::IsValidEOSPUID(ParseRawId))
    {
        // EOS:<puid>  — exact UID lookup.
        FPlayerSessionRecord Rec;
        if (Registry->FindByUid(UBanDatabase::MakeUid(TEXT("EOS"), ParseRawId.ToLower()), Rec))
            Results.Add(Rec);
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No session history found for UID %s."),
                    *UBanDatabase::MakeUid(TEXT("EOS"), ParseRawId.ToLower())),
                FLinearColor::Yellow);
            return EExecutionStatus::COMPLETED;
        }
    }
    else if (BanChat::IsValidEOSPUID(Arg))
    {
        // Raw 32-hex PUID without prefix.
        FPlayerSessionRecord Rec;
        const FString SearchUid = UBanDatabase::MakeUid(TEXT("EOS"), Arg.ToLower());
        if (Registry->FindByUid(SearchUid, Rec))
            Results.Add(Rec);
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No session history found for UID %s."), *SearchUid),
                FLinearColor::Yellow);
            return EExecutionStatus::COMPLETED;
        }
    }
    else if (ParsePlatform == TEXT("IP") || (ParsePlatform == TEXT("UNKNOWN") && Arg.Contains(TEXT("."))))
    {
        // IP:<addr>  or bare address substring (e.g. "192.168.1." for subnet search).
        const FString IpQuery = (ParsePlatform == TEXT("IP")) ? ParseRawId : Arg;
        Results = Registry->FindByIp(IpQuery);
        if (Results.IsEmpty())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] No session history found for IP '%s'."), *IpQuery),
                FLinearColor::Yellow);
            return EExecutionStatus::COMPLETED;
        }
    }
    else
    {
        // Display-name substring search.
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
            FString::Printf(TEXT("  %s | \"%s\" | ip: %s | last seen: %s%s"),
                *Rec.Uid, *Rec.DisplayName,
                Rec.IpAddress.IsEmpty() ? TEXT("—") : *Rec.IpAddress,
                *Rec.LastSeen, *BanStatus),
            bBanned ? FLinearColor::Red : FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  ABanNameChatCommand  — /banname
// ─────────────────────────────────────────────────────────────────────────────

ABanNameChatCommand::ABanNameChatCommand()
{
    CommandName          = TEXT("banname");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "BanNameUsage",
        "/banname <name_substring> [reason...]");
}

EExecutionStatus ABanNameChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
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

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const FString NameArg  = Arguments[0];
    const FString Reason   = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Banned by server administrator");
    const FString BannedBy = Sender->GetSenderName();

    // Look up the player by display-name substring.  Works for offline players
    // as long as they connected at least once while the session registry was active.
    TArray<FPlayerSessionRecord> Results = Registry->FindByName(NameArg);

    if (Results.IsEmpty())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No session history found for '%s'. "
                "The player must have connected at least once for /banname to work.  "
                "Use /ban <PUID> to ban an unknown player directly."), *NameArg),
            FLinearColor::Red);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    if (Results.Num() > 1)
    {
        TArray<FString> Descriptions;
        for (const FPlayerSessionRecord& R : Results)
            Descriptions.Add(FString::Printf(TEXT("\"%s\" (%s)"), *R.DisplayName, *R.Uid));
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Ambiguous name '%s' — %d matches: %s.  "
                "Use a more specific substring."),
                *NameArg, Results.Num(), *FString::Join(Descriptions, TEXT(", "))),
            FLinearColor::Yellow);
        return EExecutionStatus::BAD_ARGUMENTS;
    }

    const FPlayerSessionRecord& Rec = Results[0];

    // Ban the EOS PUID.  DoBan also kicks the player if they are currently online.
    const EExecutionStatus EosBanResult =
        BanChat::DoBan(this, Sender, Rec.Uid, Rec.DisplayName, 0, Reason, BannedBy);
    if (EosBanResult != EExecutionStatus::COMPLETED)
        return EosBanResult;

    // If an IP address was recorded for this player, add an IP ban and link it
    // to the EOS ban so enforcement triggers on either identity.
    if (!Rec.IpAddress.IsEmpty())
    {
        const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Rec.IpAddress);

        FBanEntry IpEntry;
        IpEntry.Uid          = IpUid;
        IpEntry.PlayerUID    = Rec.IpAddress;
        IpEntry.Platform     = TEXT("IP");
        IpEntry.PlayerName   = Rec.DisplayName;
        IpEntry.Reason       = Reason;
        IpEntry.BannedBy     = BannedBy;
        IpEntry.BanDate      = FDateTime::UtcNow();
        IpEntry.bIsPermanent = true;
        IpEntry.ExpireDate   = FDateTime(0);

        if (DB->AddBan(IpEntry))
        {
            DB->LinkBans(Rec.Uid, IpUid);
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Also banned IP %s — linked to EOS ban."),
                    *Rec.IpAddress),
                FLinearColor::Green);
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Warning: failed to add IP ban for %s."),
                    *Rec.IpAddress),
                FLinearColor::Yellow);
        }
    }
    else
    {
        Sender->SendChatMessage(
            TEXT("[BanChatCommands] No IP address on record for this player — EOS PUID ban applied only."),
            FLinearColor::Yellow);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AReloadConfigChatCommand  — /reloadconfig
// ─────────────────────────────────────────────────────────────────────────────

AReloadConfigChatCommand::AReloadConfigChatCommand()
{
    CommandName          = TEXT("reloadconfig");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "ReloadConfigUsage", "/reloadconfig");
}

EExecutionStatus AReloadConfigChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminId;
    if (!BanChat::IsAdminSender(Sender, AdminId))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    // Force UE to re-read all UPROPERTY(Config) fields from the ini files.
    GetMutableDefault<UBanChatCommandsConfig>()->ReloadConfig();

    // Update the persistent backup so it reflects the freshly-loaded values.
    FBanChatCommandsModule::BackupConfigIfNeeded();

    const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
    const int32 AdminCount = Cfg ? Cfg->AdminEosPUIDs.Num() : 0;
    UE_LOG(LogBanChatCommands, Log,
        TEXT("BanChatCommands: config reloaded by %s — %d admin(s) now active."),
        Sender->IsPlayerSender() ? *AdminId : TEXT("console"), AdminCount);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Config reloaded — %d admin(s) active."), AdminCount),
        FLinearColor::Green);

    // Optionally notify an external dashboard via HTTP POST.
    if (Cfg && !Cfg->ReloadConfigWebhookUrl.IsEmpty() &&
        FModuleManager::Get().IsModuleLoaded(TEXT("HTTP")))
    {
        const FString Payload = FString::Printf(
            TEXT("{\"event\":\"config_reloaded\",\"adminCount\":%d,\"timestamp\":\"%s\"}"),
            AdminCount, *FDateTime::UtcNow().ToIso8601());

        TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
            FHttpModule::Get().CreateRequest();
        Request->SetURL(Cfg->ReloadConfigWebhookUrl);
        Request->SetVerb(TEXT("POST"));
        Request->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
        Request->SetContentAsString(Payload);
        Request->ProcessRequest();

        UE_LOG(LogBanChatCommands, Log,
            TEXT("BanChatCommands: config-reload notification posted to webhook."));
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AKickChatCommand  — /kick
// ─────────────────────────────────────────────────────────────────────────────

AKickChatCommand::AKickChatCommand()
{
    CommandName          = TEXT("kick");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "KickUsage",
        "/kick <player|PUID> [reason...]");
}

EExecutionStatus AKickChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString ModUid;
    if (!BanChat::IsModeratorSender(Sender, ModUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason   = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Kicked by server moderator");
    const FString KickedBy = Sender->GetSenderName();

    UBanEnforcer::KickConnectedPlayer(GetWorld(), Uid, Reason);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Kicked '%s' — reason: %s"), *DisplayName, *Reason),
        FLinearColor::Green);

    FBanDiscordNotifier::NotifyPlayerKicked(DisplayName, Reason, KickedBy);

    // Optionally create a warning so kick reasons are preserved in history.
    const UBanChatCommandsConfig* CmdCfg = UBanChatCommandsConfig::Get();
    if (CmdCfg && CmdCfg->bCreateWarnOnKick)
    {
        if (UWorld* W = GetWorld())
        {
            if (UGameInstance* GI = W->GetGameInstance())
            {
                if (UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>())
                {
                    WarnReg->AddWarning(Uid, DisplayName,
                        FString::Printf(TEXT("[Kick] %s"), *Reason), KickedBy);
                    const int32 WarnCount = WarnReg->GetWarningCount(Uid);
                    FBanDiscordNotifier::NotifyWarningIssued(Uid, DisplayName,
                        FString::Printf(TEXT("[Kick] %s"), *Reason), KickedBy, WarnCount);
                }
            }
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AModBanChatCommand  — /modban
// ─────────────────────────────────────────────────────────────────────────────

AModBanChatCommand::AModBanChatCommand()
{
    CommandName          = TEXT("modban");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "ModBanUsage",
        "/modban <player|PUID> [reason...]");
}

EExecutionStatus AModBanChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString ModUid;
    if (!BanChat::IsModeratorSender(Sender, ModUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason   = Arguments.Num() > 1
        ? BanChat::JoinArgs(Arguments, 1)
        : TEXT("Temporarily banned by server moderator");
    const FString BannedBy = Sender->GetSenderName();

    return BanChat::DoBan(this, Sender, Uid, DisplayName, 30, Reason, BannedBy);
}

// ─────────────────────────────────────────────────────────────────────────────
//  AWarnChatCommand  — /warn
// ─────────────────────────────────────────────────────────────────────────────

AWarnChatCommand::AWarnChatCommand()
{
    CommandName          = TEXT("warn");
    MinNumberOfArguments = 2;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "WarnUsage",
        "/warn <player|PUID> <reason...>");
}

EExecutionStatus AWarnChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    const FString Reason   = BanChat::JoinArgs(Arguments, 1);
    const FString WarnedBy = Sender->GetSenderName();

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
    if (!WarnReg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] PlayerWarningRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    WarnReg->AddWarning(Uid, DisplayName, Reason, WarnedBy);
    const int32 WarnCount = WarnReg->GetWarningCount(Uid);

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Warned '%s' — reason: %s  (total warnings: %d)"),
            *DisplayName, *Reason, WarnCount),
        FLinearColor::Yellow);

    FBanDiscordNotifier::NotifyWarningIssued(Uid, DisplayName, Reason, WarnedBy, WarnCount);

    // Auto-ban the player when they reach the configured warning threshold.
    // Check escalation tiers first; fall back to AutoBanWarnCount if no tiers are configured.
    const UBanSystemConfig* SysCfg = UBanSystemConfig::Get();
    if (SysCfg)
    {
        int32 BanDurationMinutes = -1;

        if (SysCfg->WarnEscalationTiers.Num() > 0)
        {
            for (const FWarnEscalationTier& Tier : SysCfg->WarnEscalationTiers)
            {
                if (WarnCount >= Tier.WarnCount)
                    BanDurationMinutes = Tier.DurationMinutes;
            }
        }
        else if (SysCfg->AutoBanWarnCount > 0 && WarnCount >= SysCfg->AutoBanWarnCount)
        {
            BanDurationMinutes = SysCfg->AutoBanWarnMinutes;
        }

        if (BanDurationMinutes >= 0)
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] '%s' reached the auto-ban threshold (%d warnings) — banning."),
                    *DisplayName, WarnCount),
                FLinearColor::Red);
            BanChat::DoBan(this, Sender, Uid, DisplayName, BanDurationMinutes,
                TEXT("Auto-banned: reached warning threshold"), TEXT("system"));
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AWarningsChatCommand  — /warnings
// ─────────────────────────────────────────────────────────────────────────────

AWarningsChatCommand::AWarningsChatCommand()
{
    CommandName          = TEXT("warnings");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "WarningsUsage",
        "/warnings <player|PUID>");
}

EExecutionStatus AWarningsChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    // Optional page argument: /warnings <target> [page]
    int32 Page = 1;
    if (Arguments.Num() >= 2 && Arguments[1].IsNumeric())
    {
        Page = FMath::Max(1, FCString::Atoi(*Arguments[1]));
    }

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
    if (!WarnReg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] PlayerWarningRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const TArray<FWarningEntry> Warnings = WarnReg->GetWarningsForUid(Uid);
    if (Warnings.IsEmpty())
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No warnings on record for '%s'."), *DisplayName),
            FLinearColor::White);
        return EExecutionStatus::COMPLETED;
    }

    const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
    const int32 PageSize   = Cfg ? Cfg->BanListPageSize : 10;
    const int32 TotalPages = (Warnings.Num() + PageSize - 1) / FMath::Max(PageSize, 1);
    Page = FMath::Clamp(Page, 1, TotalPages);
    const int32 StartIdx   = (Page - 1) * PageSize;
    const int32 EndIdx     = FMath::Min(StartIdx + PageSize, Warnings.Num());

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] %d warning(s) for '%s' (%s) — page %d/%d:"),
            Warnings.Num(), *DisplayName, *Uid, Page, TotalPages),
        FLinearColor::Yellow);

    for (int32 i = StartIdx; i < EndIdx; ++i)
    {
        const FWarningEntry& W = Warnings[i];
        Sender->SendChatMessage(
            FString::Printf(TEXT("  #%d | %s | By: %s | %s"),
                i + 1, *W.Reason, *W.WarnedBy,
                *W.WarnDate.ToString(TEXT("%Y-%m-%d %H:%M:%S"))),
            FLinearColor::White);
    }

    if (TotalPages > 1 && Page < TotalPages)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("  Use /warnings %s %d to see more."), *Arguments[0], Page + 1),
            FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AClearWarnsChatCommand  — /clearwarns
// ─────────────────────────────────────────────────────────────────────────────

AClearWarnsChatCommand::AClearWarnsChatCommand()
{
    CommandName          = TEXT("clearwarns");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "ClearWarnsUsage",
        "/clearwarns <player|PUID>");
}

EExecutionStatus AClearWarnsChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
    if (!WarnReg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] PlayerWarningRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    const int32 Cleared = WarnReg->ClearWarningsForUid(Uid);
    if (Cleared > 0)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Cleared %d warning(s) for '%s'."),
                Cleared, *DisplayName),
            FLinearColor::Green);
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No warnings on record for '%s'."), *DisplayName),
            FLinearColor::Yellow);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AAnnounceChatCommand  — /announce
// ─────────────────────────────────────────────────────────────────────────────

AAnnounceChatCommand::AAnnounceChatCommand()
{
    CommandName          = TEXT("announce");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "AnnounceUsage",
        "/announce <message...>");
}

EExecutionStatus AAnnounceChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    const FString Message    = BanChat::JoinArgs(Arguments, 0);
    const FString SenderName = Sender->GetSenderName();

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;

    // Broadcast to all connected players.
    const FString Announcement = FString::Printf(TEXT("[Server Announcement] %s"), *Message);
    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC) continue;
        PC->ClientMessage(Announcement);
    }

    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Announcement sent: %s"), *Message),
        FLinearColor::Green);

    UE_LOG(LogBanChatCommands, Log, TEXT("Announce [%s]: %s"), *SenderName, *Message);
    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AStaffListChatCommand  — /stafflist
// ─────────────────────────────────────────────────────────────────────────────

AStaffListChatCommand::AStaffListChatCommand()
{
    CommandName          = TEXT("stafflist");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "StaffListUsage",
        "/stafflist");
}

EExecutionStatus AStaffListChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    const UBanChatCommandsConfig* Cfg = UBanChatCommandsConfig::Get();
    if (!Cfg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] Config unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;

    TArray<FString> OnlineAdmins;
    TArray<FString> OnlineModerators;

    for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        if (!PC || !PC->PlayerState) continue;

        FString PlayerUid;
        const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
        if (NetId.IsValid() && NetId.GetType() != FName(TEXT("NONE")))
            PlayerUid = UBanDatabase::MakeUid(TEXT("EOS"), NetId.ToString().ToLower());
        else
        {
            const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);
            if (!EosPuid.IsEmpty())
                PlayerUid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
        }
        if (PlayerUid.IsEmpty()) continue;

        const FString Name = PC->PlayerState->GetPlayerName();
        if (Cfg->IsAdminUid(PlayerUid))
            OnlineAdmins.Add(Name);
        else if (Cfg->IsModeratorUid(PlayerUid))
            OnlineModerators.Add(Name);
    }

    if (OnlineAdmins.IsEmpty() && OnlineModerators.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] No staff online at this time."), FLinearColor::White);
    }
    else
    {
        if (!OnlineAdmins.IsEmpty())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Admins online: %s"),
                    *FString::Join(OnlineAdmins, TEXT(", "))),
                FLinearColor::Cyan);
        }
        if (!OnlineModerators.IsEmpty())
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] Moderators online: %s"),
                    *FString::Join(OnlineModerators, TEXT(", "))),
                FLinearColor(0.0f, 0.8f, 1.0f));
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AReasonChatCommand  — /reason
// ─────────────────────────────────────────────────────────────────────────────

AReasonChatCommand::AReasonChatCommand()
{
    CommandName          = TEXT("reason");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "ReasonUsage",
        "/reason <UID>");
}

EExecutionStatus AReasonChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString Uid;
    if (!BanChat::ParseAndNormaliseUidArg(Sender, Arguments[0], Uid))
        return EExecutionStatus::BAD_ARGUMENTS;

    UBanDatabase* DB = BanChat::GetDB(this);
    if (!DB)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] UBanDatabase unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    FBanEntry Entry;
    if (!DB->GetBanByUid(Uid, Entry))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] No ban record found for '%s'."), *Uid),
            FLinearColor::Yellow);
        return EExecutionStatus::COMPLETED;
    }

    if (Entry.bIsPermanent)
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Ban reason for %s: %s (permanent, by %s, %s)"),
                *Uid, *Entry.Reason, *Entry.BannedBy,
                *Entry.BanDate.ToString(TEXT("%Y-%m-%d"))),
            FLinearColor::White);
    }
    else
    {
        const FTimespan Remaining = Entry.ExpireDate - FDateTime::UtcNow();
        const FString RemainingStr = Remaining.GetTicks() > 0
            ? FString::Printf(TEXT("%.0f min remaining"), Remaining.GetTotalMinutes())
            : TEXT("expired");
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Ban reason for %s: %s (temp — %s, by %s)"),
                *Uid, *Entry.Reason, *RemainingStr, *Entry.BannedBy),
            FLinearColor::White);
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AHistoryChatCommand  — /history (self)
// ─────────────────────────────────────────────────────────────────────────────

AHistoryChatCommand::AHistoryChatCommand()
{
    CommandName          = TEXT("history");
    MinNumberOfArguments = 0;
    bOnlyUsableByPlayer  = true;
    Usage = NSLOCTEXT("BanChatCommands", "HistoryUsage",
        "/history");
}

EExecutionStatus AHistoryChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    APlayerController* PC = Cast<APlayerController>(Sender->GetPlayer());
    if (!PC || !PC->PlayerState)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] Could not resolve your identity."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    FString Uid;
    const FUniqueNetIdRepl& NetId = PC->PlayerState->GetUniqueId();
    if (NetId.IsValid() && NetId.GetType() != FName(TEXT("NONE")))
        Uid = UBanDatabase::MakeUid(TEXT("EOS"), NetId.ToString().ToLower());
    else
    {
        const FString EosPuid = UBanEnforcer::ExtractEosPuidFromConnectionUrl(PC);
        if (!EosPuid.IsEmpty())
            Uid = UBanDatabase::MakeUid(TEXT("EOS"), EosPuid);
    }

    if (Uid.IsEmpty())
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] Could not resolve your platform identity."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    // Session info.
    UPlayerSessionRegistry* SessionReg = GI->GetSubsystem<UPlayerSessionRegistry>();
    if (SessionReg)
    {
        for (const FPlayerSessionRecord& R : SessionReg->GetAllRecords())
        {
            if (R.Uid.Equals(Uid, ESearchCase::IgnoreCase))
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("[BanChatCommands] Your last session: %s"), *R.LastSeen),
                    FLinearColor::White);
                break;
            }
        }
    }

    // Warning history.
    UPlayerWarningRegistry* WarnReg = GI->GetSubsystem<UPlayerWarningRegistry>();
    if (WarnReg)
    {
        const TArray<FWarningEntry> Warnings = WarnReg->GetWarningsForUid(Uid);
        if (Warnings.IsEmpty())
        {
            Sender->SendChatMessage(TEXT("[BanChatCommands] You have no warnings on record."), FLinearColor::Green);
        }
        else
        {
            Sender->SendChatMessage(
                FString::Printf(TEXT("[BanChatCommands] You have %d warning(s) on record."), Warnings.Num()),
                FLinearColor::Yellow);
            const int32 MaxShow = FMath::Min(Warnings.Num(), 5);
            for (int32 i = 0; i < MaxShow; ++i)
            {
                const FWarningEntry& W = Warnings[i];
                Sender->SendChatMessage(
                    FString::Printf(TEXT("  #%d %s — %s"),
                        i + 1, *W.WarnDate.ToString(TEXT("%Y-%m-%d")), *W.Reason),
                    FLinearColor::White);
            }
            if (Warnings.Num() > MaxShow)
            {
                Sender->SendChatMessage(
                    FString::Printf(TEXT("  … and %d more."), Warnings.Num() - MaxShow),
                    FLinearColor::White);
            }
        }
    }

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AMuteChatCommand  — /mute
// ─────────────────────────────────────────────────────────────────────────────

AMuteChatCommand::AMuteChatCommand()
{
    CommandName          = TEXT("mute");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "MuteUsage",
        "/mute <player|PUID> [minutes] [reason...]");
}

EExecutionStatus AMuteChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    int32 Minutes     = 0;
    int32 ReasonStart = 1;
    if (Arguments.Num() >= 2 && Arguments[1].IsNumeric())
    {
        Minutes      = FMath::Max(0, FCString::Atoi(*Arguments[1]));
        ReasonStart  = 2;
    }

    const FString Reason   = Arguments.Num() > ReasonStart
        ? BanChat::JoinArgs(Arguments, ReasonStart)
        : TEXT("Muted by admin");
    const FString MutedBy = Sender->GetSenderName();

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UMuteRegistry* MuteReg = GI->GetSubsystem<UMuteRegistry>();
    if (!MuteReg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] MuteRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    MuteReg->MutePlayer(Uid, DisplayName, Reason, MutedBy, Minutes);

    const FString DurStr = Minutes > 0
        ? FString::Printf(TEXT(" for %d minute(s)"), Minutes)
        : TEXT(" indefinitely");
    Sender->SendChatMessage(
        FString::Printf(TEXT("[BanChatCommands] Muted '%s'%s — %s"), *DisplayName, *DurStr, *Reason),
        FLinearColor::Green);

    return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
//  AUnmuteChatCommand  — /unmute
// ─────────────────────────────────────────────────────────────────────────────

AUnmuteChatCommand::AUnmuteChatCommand()
{
    CommandName          = TEXT("unmute");
    MinNumberOfArguments = 1;
    bOnlyUsableByPlayer  = false;
    Usage = NSLOCTEXT("BanChatCommands", "UnmuteUsage",
        "/unmute <player|PUID>");
}

EExecutionStatus AUnmuteChatCommand::ExecuteCommand_Implementation(
    UCommandSender* Sender, const TArray<FString>& Arguments, const FString& Label)
{
    FString AdminUid;
    if (!BanChat::IsAdminSender(Sender, AdminUid))
        return EExecutionStatus::INSUFFICIENT_PERMISSIONS;

    FString Uid, DisplayName;
    if (!BanChat::ResolveTarget(this, Sender, Arguments[0], Uid, DisplayName))
        return EExecutionStatus::BAD_ARGUMENTS;

    UWorld* World = GetWorld();
    if (!World) return EExecutionStatus::UNCOMPLETED;
    UGameInstance* GI = World->GetGameInstance();
    if (!GI) return EExecutionStatus::UNCOMPLETED;

    UMuteRegistry* MuteReg = GI->GetSubsystem<UMuteRegistry>();
    if (!MuteReg)
    {
        Sender->SendChatMessage(TEXT("[BanChatCommands] MuteRegistry unavailable."), FLinearColor::Red);
        return EExecutionStatus::UNCOMPLETED;
    }

    if (MuteReg->UnmutePlayer(Uid))
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] Unmuted '%s'."), *DisplayName),
            FLinearColor::Green);
    }
    else
    {
        Sender->SendChatMessage(
            FString::Printf(TEXT("[BanChatCommands] '%s' is not currently muted."), *DisplayName),
            FLinearColor::Yellow);
    }

    return EExecutionStatus::COMPLETED;
}
