// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordSubsystem.h"

// BanSystem public API
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanTypes.h"
#include "PlayerSessionRegistry.h"
#include "PlayerWarningRegistry.h"
#include "BanDiscordNotifier.h"

// BanChatCommands public API
#include "MuteRegistry.h"

#include "Misc/DefaultValueHelper.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"

// ─────────────────────────────────────────────────────────────────────────────
// Internal namespace helpers
// ─────────────────────────────────────────────────────────────────────────────

namespace BanDiscordHelpers
{
	/** Retrieve the UBanDatabase subsystem via the GameInstance. */
	static UBanDatabase* GetDB(const UBanDiscordSubsystem* Self)
	{
		UGameInstance* GI = Self ? Self->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UBanDatabase>() : nullptr;
	}

	/** Retrieve the UPlayerSessionRegistry subsystem via the GameInstance. */
	static UPlayerSessionRegistry* GetRegistry(const UBanDiscordSubsystem* Self)
	{
		UGameInstance* GI = Self ? Self->GetGameInstance() : nullptr;
		return GI ? GI->GetSubsystem<UPlayerSessionRegistry>() : nullptr;
	}

	/** Format ban expiry for display in Discord messages. */
	static FString FormatExpiry(const FBanEntry& Entry)
	{
		if (Entry.bIsPermanent)
			return TEXT("never (permanent)");
		return Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")) + TEXT(" UTC");
	}

	/** Join Arguments[StartIdx..] into a single space-separated string. */
	static FString JoinArgs(const TArray<FString>& Args, int32 StartIdx)
	{
		FString Result;
		for (int32 i = StartIdx; i < Args.Num(); ++i)
		{
			if (i > StartIdx) Result += TEXT(" ");
			Result += Args[i];
		}
		return Result;
	}

	/** Truncate a string to MaxLen chars and append "..." when truncated. */
	static FString Truncate(const FString& Str, int32 MaxLen)
	{
		if (Str.Len() <= MaxLen) return Str;
		return Str.Left(MaxLen - 3) + TEXT("...");
	}

	/**
	 * Escape characters that have special meaning in Discord markdown so that
	 * player names with *, _, `, ~, |, > or \ are rendered literally.
	 */
	static FString EscapeMarkdown(const FString& Text)
	{
		static const TCHAR SpecialChars[] = { TEXT('*'), TEXT('_'), TEXT('`'), TEXT('~'),
		                                       TEXT('|'), TEXT('>'), TEXT('\\'), TEXT('\0') };
		FString Out;
		Out.Reserve(Text.Len() + 8);
		for (TCHAR C : Text)
		{
			bool bSpecial = false;
			for (int32 i = 0; SpecialChars[i] != TEXT('\0'); ++i)
			{
				if (C == SpecialChars[i]) { bSpecial = true; break; }
			}
			if (bSpecial) Out += TEXT('\\');
			Out += C;
		}
		return Out;
	}

	// Number of bans shown per !banlist page.
	static constexpr int32 BanListPageSize = 10;
}

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDiscordSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	return IsRunningDedicatedServer();
}

void UBanDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	Config = FBanBridgeConfig::Load();
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Initialized. Waiting for Discord provider via SetProvider()."));
}

void UBanDiscordSubsystem::Deinitialize()
{
	SetProvider(nullptr);
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Provider injection
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::SetProvider(IDiscordBridgeProvider* InProvider)
{
	// Guard: no-op when setting the same provider twice.
	if (InProvider == CachedProvider)
		return;

	// Unsubscribe from the old provider.
	if (CachedProvider && RawMessageDelegateHandle.IsValid())
	{
		CachedProvider->UnsubscribeRawMessage(RawMessageDelegateHandle);
		RawMessageDelegateHandle.Reset();
	}

	CachedProvider = InProvider;

	// Subscribe to the new provider.
	if (CachedProvider)
	{
		TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
		RawMessageDelegateHandle = CachedProvider->SubscribeRawMessage(
			[WeakThis](const TSharedPtr<FJsonObject>& MsgObj)
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
					Self->OnRawDiscordMessage(MsgObj);
			});
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Discord provider injected; "
		            "ban commands are %s."),
		       Config.AdminRoleId.IsEmpty() ? TEXT("DISABLED (AdminRoleId not set)") : TEXT("enabled"));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Message routing
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::OnRawDiscordMessage(const TSharedPtr<FJsonObject>& MessageObj)
{
	if (!MessageObj.IsValid())
		return;

	// Ban commands are disabled when AdminRoleId is not configured.
	if (Config.AdminRoleId.IsEmpty())
		return;

	// All ban commands start with "!".
	FString Content;
	MessageObj->TryGetStringField(TEXT("content"), Content);
	Content.TrimStartAndEndInline();
	if (!Content.StartsWith(TEXT("!")))
		return;

	// Optional: restrict to a configured channel.
	FString ChannelId;
	MessageObj->TryGetStringField(TEXT("channel_id"), ChannelId);
	if (!Config.BanCommandChannelId.IsEmpty() && ChannelId != Config.BanCommandChannelId)
		return;

	// Parse command token + arguments.
	TArray<FString> Tokens;
	Content.ParseIntoArrayWS(Tokens);
	if (Tokens.IsEmpty())
		return;

	const FString Command = Tokens[0].ToLower();

	// Only handle ban-related commands; let other commands pass through silently.
	static const TArray<FString> KnownCommands = {
		TEXT("!ban"), TEXT("!tempban"), TEXT("!unban"),
		TEXT("!bancheck"), TEXT("!banlist"), TEXT("!playerhistory"),
		TEXT("!kick"), TEXT("!mute"), TEXT("!unmute"),
		TEXT("!warn"), TEXT("!announce")
	};
	if (!KnownCommands.Contains(Command))
		return;

	// Verify the sender holds the configured admin role.
	TArray<FString> MemberRoles = ExtractMemberRoles(MessageObj);
	if (!IsAdminMember(MemberRoles))
	{
		if (CachedProvider)
		{
			CachedProvider->SendDiscordChannelMessage(
				ChannelId,
				TEXT("❌ You do not have permission to use ban commands."));
		}
		return;
	}

	// Build the argument list (everything after the command token).
	TArray<FString> Args;
	for (int32 i = 1; i < Tokens.Num(); ++i)
		Args.Add(Tokens[i]);

	const FString SenderName = ExtractSenderName(MessageObj);

	if (Command == TEXT("!ban"))
		HandleBanCommand(Args, ChannelId, SenderName, /*bTemporary=*/false);
	else if (Command == TEXT("!tempban"))
		HandleBanCommand(Args, ChannelId, SenderName, /*bTemporary=*/true);
	else if (Command == TEXT("!unban"))
		HandleUnbanCommand(Args, ChannelId, SenderName);
	else if (Command == TEXT("!bancheck"))
		HandleBanCheckCommand(Args, ChannelId);
	else if (Command == TEXT("!banlist"))
		HandleBanListCommand(Args, ChannelId);
	else if (Command == TEXT("!playerhistory"))
		HandlePlayerHistoryCommand(Args, ChannelId);
	else if (Command == TEXT("!kick"))
		HandleKickCommand(Args, ChannelId, SenderName);
	else if (Command == TEXT("!mute"))
		HandleMuteCommand(Args, ChannelId, SenderName, /*bMute=*/true);
	else if (Command == TEXT("!unmute"))
		HandleMuteCommand(Args, ChannelId, SenderName, /*bMute=*/false);
	else if (Command == TEXT("!warn"))
		HandleWarnCommand(Args, ChannelId, SenderName);
	else if (Command == TEXT("!announce"))
		HandleAnnounceCommand(Args, ChannelId, SenderName);
}

// ─────────────────────────────────────────────────────────────────────────────
// Authorisation / extraction helpers
// ─────────────────────────────────────────────────────────────────────────────

TArray<FString> UBanDiscordSubsystem::ExtractMemberRoles(const TSharedPtr<FJsonObject>& MessageObj)
{
	TArray<FString> Roles;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) || !MemberPtr)
		return Roles;

	const TArray<TSharedPtr<FJsonValue>>* RolesArray = nullptr;
	if (!(*MemberPtr)->TryGetArrayField(TEXT("roles"), RolesArray) || !RolesArray)
		return Roles;

	for (const TSharedPtr<FJsonValue>& RoleVal : *RolesArray)
	{
		FString RoleId;
		if (RoleVal->TryGetString(RoleId))
			Roles.Add(RoleId);
	}
	return Roles;
}

FString UBanDiscordSubsystem::ExtractSenderName(const TSharedPtr<FJsonObject>& MessageObj)
{
	// Priority: server nickname > global_name > username
	FString Name;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
		(*MemberPtr)->TryGetStringField(TEXT("nick"), Name);

	if (Name.IsEmpty())
	{
		const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
		if (MessageObj->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
		{
			if (!(*AuthorPtr)->TryGetStringField(TEXT("global_name"), Name) || Name.IsEmpty())
				(*AuthorPtr)->TryGetStringField(TEXT("username"), Name);
		}
	}

	return Name.IsEmpty() ? TEXT("Discord Admin") : Name;
}

bool UBanDiscordSubsystem::IsAdminMember(const TArray<FString>& Roles) const
{
	return Roles.Contains(Config.AdminRoleId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Target resolution
// ─────────────────────────────────────────────────────────────────────────────

bool UBanDiscordSubsystem::IsValidEOSPUID(const FString& Id)
{
	if (Id.Len() != 32) return false;
	for (TCHAR C : Id)
		if (!FChar::IsHexDigit(C)) return false;
	return true;
}

bool UBanDiscordSubsystem::IsValidIPQuery(const FString& Query)
{
	// Accept explicit "IP:<addr>" prefix or a bare address that contains a dot
	// (IPv4: "1.2.3.4" or partial "192.168.1.").
	FString Platform, RawId;
	UBanDatabase::ParseUid(Query, Platform, RawId);
	if (Platform == TEXT("IP") && !RawId.IsEmpty())
		return true;
	// "UNKNOWN" is what ParseUid returns for strings without a colon prefix.
	return Platform == TEXT("UNKNOWN") && Query.Contains(TEXT("."));
}

bool UBanDiscordSubsystem::ResolveTarget(const FString& Arg,
                                          FString& OutUid,
                                          FString& OutDisplayName,
                                          FString& OutErrorMsg) const
{
	// 1. Raw 32-character hex EOS PUID.
	if (IsValidEOSPUID(Arg))
	{
		const FString Lower = Arg.ToLower();
		OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), Lower);
		OutDisplayName = Lower;

		// Try to enrich with a real name from the session registry.
		if (UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this))
		{
			FPlayerSessionRecord Record;
			if (Registry->FindByUid(OutUid, Record) && !Record.DisplayName.IsEmpty())
				OutDisplayName = Record.DisplayName;
		}
		return true;
	}

	// 2. Compound UID: "EOS:<32hex>".
	{
		FString Platform, RawId;
		UBanDatabase::ParseUid(Arg, Platform, RawId);
		if (Platform == TEXT("EOS") && IsValidEOSPUID(RawId))
		{
			const FString Lower = RawId.ToLower();
			OutUid         = UBanDatabase::MakeUid(TEXT("EOS"), Lower);
			OutDisplayName = Lower;

			if (UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this))
			{
				FPlayerSessionRecord Record;
				if (Registry->FindByUid(OutUid, Record) && !Record.DisplayName.IsEmpty())
					OutDisplayName = Record.DisplayName;
			}
			return true;
		}
	}

	// 3. Display-name substring lookup against the PlayerSessionRegistry.
	//    This covers both currently-connected players (BanEnforcer records sessions
	//    on join) and players seen in previous server sessions.
	UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this);
	if (!Registry)
	{
		OutErrorMsg = FString::Printf(
			TEXT("❌ PlayerSessionRegistry is unavailable. "
			     "Use an EOS PUID directly (32-char hex or `EOS:<puid>`)."));
		return false;
	}

	TArray<FPlayerSessionRecord> Matches = Registry->FindByName(Arg);

	if (Matches.IsEmpty())
	{
		OutErrorMsg = FString::Printf(
			TEXT("❌ No player found matching `%s`. "
			     "Use an EOS PUID (32-char hex or `EOS:<puid>`) to target this player."),
			*Arg);
		return false;
	}

	if (Matches.Num() > 1)
	{
		// Build a short list of matching names to help the admin narrow down.
		FString MatchList;
		const int32 ShowCount = FMath::Min(Matches.Num(), 5);
		for (int32 i = 0; i < ShowCount; ++i)
		{
			if (i > 0) MatchList += TEXT(", ");
			MatchList += FString::Printf(TEXT("`%s`"), *Matches[i].DisplayName);
		}
		if (Matches.Num() > ShowCount)
			MatchList += FString::Printf(TEXT(", +%d more"), Matches.Num() - ShowCount);

		OutErrorMsg = FString::Printf(
			TEXT("❌ Ambiguous name `%s` — %d players match: %s. "
			     "Use an EOS PUID to target a specific player."),
			*Arg, Matches.Num(), *MatchList);
		return false;
	}

	OutUid         = Matches[0].Uid;
	OutDisplayName = Matches[0].DisplayName;
	return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban / !tempban
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanCommand(const TArray<FString>& Args,
                                             const FString& ChannelId,
                                             const FString& SenderName,
                                             bool bTemporary)
{
	const FString CmdName = bTemporary ? TEXT("!tempban") : TEXT("!ban");

	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	// Validate minimum argument count.
	// !ban:     <target> [reason]         → 1 arg minimum
	// !tempban: <target> <minutes> [reason] → 2 args minimum
	const int32 MinArgs = bTemporary ? 2 : 1;
	if (Args.Num() < MinArgs)
	{
		const FString Usage = bTemporary
			? TEXT("Usage: `!tempban <PUID|name> <minutes> [reason]`")
			: TEXT("Usage: `!ban <PUID|name> [reason]`");
		CachedProvider->SendDiscordChannelMessage(ChannelId, Usage);
		return;
	}

	// Resolve target.
	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	// Parse duration for !tempban.
	int32 DurationMinutes = 0;
	int32 ReasonStartIdx  = 1;
	if (bTemporary)
	{
		if (!FDefaultValueHelper::ParseInt(Args[1], DurationMinutes) || DurationMinutes <= 0)
		{
			CachedProvider->SendDiscordChannelMessage(
				ChannelId,
				TEXT("❌ `<minutes>` must be a positive integer.\n"
				     "Usage: `!tempban <PUID|name> <minutes> [reason]`"));
			return;
		}
		ReasonStartIdx = 2;
	}

	const FString Reason = BanDiscordHelpers::JoinArgs(Args, ReasonStartIdx);

	// Build the ban entry.
	FBanEntry Entry;
	Entry.Uid        = Uid;
	UBanDatabase::ParseUid(Uid, Entry.Platform, Entry.PlayerUID);
	Entry.PlayerName = DisplayName;
	Entry.Reason     = Reason.IsEmpty() ? TEXT("No reason given") : Reason;
	Entry.BannedBy   = SenderName;
	Entry.BanDate    = FDateTime::UtcNow();

	if (bTemporary)
	{
		Entry.bIsPermanent = false;
		Entry.ExpireDate   = FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);
	}
	else
	{
		Entry.bIsPermanent = true;
		Entry.ExpireDate   = FDateTime(0);
	}

	if (!DB->AddBan(Entry))
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ Failed to write the ban to the database. Check server logs."));
		return;
	}

	// Kick if the player is currently connected.
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		UBanEnforcer::KickConnectedPlayer(World, Uid, Entry.GetKickMessage());

	// Format confirmation message.
	const FString SafeName = BanDiscordHelpers::EscapeMarkdown(DisplayName);
	FString Msg;
	if (bTemporary)
	{
		Msg = FString::Printf(
			TEXT("✅ **%s** (`%s`) has been temporarily banned for **%d minute(s)**.\n"
			     "Expires: %s\nReason: %s\nBanned by: %s"),
			*SafeName, *Uid, DurationMinutes,
			*BanDiscordHelpers::FormatExpiry(Entry),
			*Entry.Reason, *SenderName);
	}
	else
	{
		Msg = FString::Printf(
			TEXT("✅ **%s** (`%s`) has been **permanently** banned.\n"
			     "Reason: %s\nBanned by: %s"),
			*SafeName, *Uid, *Entry.Reason, *SenderName);
	}

	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: %s banned %s (%s). Reason: %s"),
	       *SenderName, *DisplayName, *Uid, *Entry.Reason);

	CachedProvider->SendDiscordChannelMessage(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !unban
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleUnbanCommand(const TArray<FString>& Args,
                                               const FString& ChannelId,
                                               const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("Usage: `!unban <PUID>`\nPUID can be a 32-char hex string or `EOS:<puid>`."));
		return;
	}

	// Resolve to a compound UID.
	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	if (!DB->RemoveBanByUid(Uid))
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			FString::Printf(TEXT("⚠️ No active ban found for `%s` (`%s`)."),
			                *DisplayName, *Uid));
		return;
	}

	const FString Msg = FString::Printf(
		TEXT("✅ Ban removed for **%s** (`%s`).\nUnbanned by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: %s unbanned %s (%s)."),
	       *SenderName, *DisplayName, *Uid);

	CachedProvider->SendDiscordChannelMessage(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !bancheck
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanCheckCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("Usage: `!bancheck <PUID|name>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	// Reload from disk before checking so manual edits take immediate effect.
	DB->ReloadIfChanged();

	FBanEntry Entry;
	const bool bBanned = DB->IsCurrentlyBannedByAnyId(Uid, Entry);

	FString Msg;
	if (bBanned)
	{
		Msg = FString::Printf(
			TEXT("🔨 **%s** (`%s`) is **currently banned**.\n"
			     "Reason: %s\n"
			     "Banned by: %s\n"
			     "Ban date: %s UTC\n"
			     "Expires: %s"),
			*DisplayName, *Uid,
			*Entry.Reason,
			*Entry.BannedBy,
			*Entry.BanDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
			*BanDiscordHelpers::FormatExpiry(Entry));

		if (Entry.LinkedUids.Num() > 0)
		{
			Msg += FString::Printf(TEXT("\nLinked UIDs: `%s`"),
			                       *FString::Join(Entry.LinkedUids, TEXT("`, `")));
		}
	}
	else
	{
		// Check if there is any record at all (expired ban).
		FBanEntry AnyEntry;
		if (DB->GetBanByUid(Uid, AnyEntry))
		{
			Msg = FString::Printf(
				TEXT("✅ **%s** (`%s`) is **not currently banned**.\n"
				     "(An expired ban record exists — reason: %s, expired: %s)"),
				*DisplayName, *Uid,
				*AnyEntry.Reason,
				*BanDiscordHelpers::FormatExpiry(AnyEntry));
		}
		else
		{
			Msg = FString::Printf(
				TEXT("✅ **%s** (`%s`) is **not banned**."),
				*DisplayName, *Uid);
		}
	}

	CachedProvider->SendDiscordChannelMessage(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !banlist
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanListCommand(const TArray<FString>& Args,
                                                 const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	DB->ReloadIfChanged();
	TArray<FBanEntry> ActiveBans = DB->GetActiveBans();

	if (ActiveBans.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, TEXT("✅ No active bans."));
		return;
	}

	// Determine the requested page (1-indexed).
	int32 Page = 1;
	if (Args.Num() > 0)
		FDefaultValueHelper::ParseInt(Args[0], Page);
	if (Page < 1) Page = 1;

	const int32 Total      = ActiveBans.Num();
	const int32 TotalPages = FMath::DivideAndRoundUp(Total, BanDiscordHelpers::BanListPageSize);
	if (Page > TotalPages) Page = TotalPages;

	const int32 StartIdx = (Page - 1) * BanDiscordHelpers::BanListPageSize;
	const int32 EndIdx   = FMath::Min(StartIdx + BanDiscordHelpers::BanListPageSize, Total);

	FString Body;
	Body.Reserve(1600);

	// Fixed-width header.
	Body += TEXT("```\n");
	Body += FString::Printf(TEXT("%-4s  %-22s  %-16s  %-20s  %s\n"),
	                        TEXT("ID"), TEXT("UID (truncated)"),
	                        TEXT("Name"), TEXT("Expires"), TEXT("Reason"));
	Body += FString(TEXT("─"), 80) + TEXT("\n");

	for (int32 i = StartIdx; i < EndIdx; ++i)
	{
		const FBanEntry& E = ActiveBans[i];

		// Truncate UID display to keep lines manageable.
		const FString UidShort = BanDiscordHelpers::Truncate(E.Uid, 22);
		const FString NameShort = BanDiscordHelpers::Truncate(E.PlayerName, 16);
		const FString ExpiryShort = E.bIsPermanent
			? TEXT("permanent")
			: E.ExpireDate.ToString(TEXT("%m-%d %H:%M UTC"));
		const FString ReasonShort = BanDiscordHelpers::Truncate(E.Reason, 28);

		Body += FString::Printf(TEXT("%-4lld  %-22s  %-16s  %-20s  %s\n"),
		                        E.Id, *UidShort, *NameShort, *ExpiryShort, *ReasonShort);
	}

	Body += TEXT("```");

	const FString Header = FString::Printf(
		TEXT("**Active Bans — Page %d/%d (%d total)**\n"),
		Page, TotalPages, Total);

	FString Msg = Header + Body;

	// Safety clamp: Discord messages are limited to 2000 characters.
	// If the body is too long, truncate and note the overflow.
	if (Msg.Len() > 1990)
	{
		Msg = Msg.Left(1940) + TEXT("\n...(truncated)```");
	}

	CachedProvider->SendDiscordChannelMessage(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !playerhistory
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandlePlayerHistoryCommand(const TArray<FString>& Args,
                                                       const FString& ChannelId)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("Usage: `!playerhistory <name|PUID|IP>`"));
		return;
	}

	UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this);
	if (!Registry)
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			TEXT("❌ PlayerSessionRegistry is not available on this server."));
		return;
	}

	const FString Query = Args[0];
	TArray<FPlayerSessionRecord> Results;

	// Compound UID or raw PUID lookup.
	FString Platform, RawId;
	UBanDatabase::ParseUid(Query, Platform, RawId);
	if (Platform == TEXT("EOS") && IsValidEOSPUID(RawId))
	{
		FPlayerSessionRecord Record;
		if (Registry->FindByUid(UBanDatabase::MakeUid(TEXT("EOS"), RawId.ToLower()), Record))
			Results.Add(Record);
	}
	else if (IsValidEOSPUID(Query))
	{
		FPlayerSessionRecord Record;
		if (Registry->FindByUid(UBanDatabase::MakeUid(TEXT("EOS"), Query.ToLower()), Record))
			Results.Add(Record);
	}
	else if (IsValidIPQuery(Query))
	{
		// Strip the "IP:" prefix if present before querying.
		const FString IpQuery = (Platform == TEXT("IP")) ? RawId : Query;
		Results = Registry->FindByIp(IpQuery);
	}
	else
	{
		Results = Registry->FindByName(Query);
	}

	if (Results.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(
			ChannelId,
			FString::Printf(TEXT("⚠️ No session records found for `%s`."), *Query));
		return;
	}

	// Limit output to avoid exceeding Discord's 2000-char limit.
	constexpr int32 MaxResults = 10;
	const int32 ShowCount = FMath::Min(Results.Num(), MaxResults);

	FString Body;
	Body.Reserve(1600);
	Body += TEXT("```\n");
	Body += FString::Printf(TEXT("%-16s  %-40s  %-15s  %-20s\n"),
	                        TEXT("Name"), TEXT("UID"), TEXT("IP"), TEXT("Last Seen (UTC)"));
	Body += FString(TEXT("─"), 97) + TEXT("\n");

	for (int32 i = 0; i < ShowCount; ++i)
	{
		const FPlayerSessionRecord& R = Results[i];
		const FString NameShort = BanDiscordHelpers::Truncate(R.DisplayName, 16);
		const FString UidShort  = BanDiscordHelpers::Truncate(R.Uid, 40);
		const FString IpShort   = R.IpAddress.IsEmpty() ? TEXT("—") : BanDiscordHelpers::Truncate(R.IpAddress, 15);
		const FString LastSeen  = BanDiscordHelpers::Truncate(R.LastSeen, 20);
		Body += FString::Printf(TEXT("%-16s  %-40s  %-15s  %s\n"),
		                        *NameShort, *UidShort, *IpShort, *LastSeen);
	}
	Body += TEXT("```");

	FString Header = FString::Printf(
		TEXT("**Player History for `%s`** (%d record(s))\n"),
		*Query, Results.Num());

	if (Results.Num() > MaxResults)
	{
		Header += FString::Printf(TEXT("_(Showing first %d of %d results)_\n"), MaxResults, Results.Num());
	}

	FString Msg = Header + Body;
	if (Msg.Len() > 1990)
		Msg = Msg.Left(1940) + TEXT("\n...(truncated)```");

	CachedProvider->SendDiscordChannelMessage(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !kick
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleKickCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, TEXT("Usage: `!kick <PUID|name> [reason]`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	const FString Reason = (Args.Num() > 1) ? BanDiscordHelpers::JoinArgs(Args, 1) : TEXT("Kicked by Discord admin");

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			TEXT("❌ No active game world found. Is the server running?"));
		return;
	}

	const bool bKicked = UBanEnforcer::KickConnectedPlayer(World, Uid, Reason);
	if (bKicked)
	{
		FBanDiscordNotifier::NotifyPlayerKicked(DisplayName, Reason, SenderName);
		const FString KickMsg = FString::Printf(
			TEXT("✅ Kicked **%s** (`%s`).\nReason: %s\nKicked by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *Reason, *SenderName);
		CachedProvider->SendDiscordChannelMessage(ChannelId, KickMsg);
		PostModerationLog(KickMsg);
	}
	else
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			FString::Printf(TEXT("⚠️ Player **%s** (`%s`) is not currently connected."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// !mute / !unmute
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleMuteCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName,
                                              bool bMute)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			bMute
			? TEXT("Usage: `!mute <PUID|name> [minutes] [reason]`")
			: TEXT("Usage: `!unmute <PUID|name>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	if (!bMute)
	{
		if (!MuteReg->UnmutePlayer(Uid))
		{
			CachedProvider->SendDiscordChannelMessage(ChannelId,
				FString::Printf(TEXT("⚠️ **%s** (`%s`) is not currently muted."),
					*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
			return;
		}
		const FString UnmuteMsg = FString::Printf(
			TEXT("✅ Unmuted **%s** (`%s`).\nUnmuted by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);
		CachedProvider->SendDiscordChannelMessage(ChannelId, UnmuteMsg);
		PostModerationLog(UnmuteMsg);
		return;
	}

	// Parse optional [minutes].
	int32 ReasonStart = 1;
	int32 Minutes = 0;
	if (Args.Num() > 1)
	{
		int32 Parsed = 0;
		if (FDefaultValueHelper::ParseInt(Args[1], Parsed) && Parsed > 0)
		{
			Minutes = Parsed;
			ReasonStart = 2;
		}
	}
	const FString Reason = (Args.Num() > ReasonStart)
		? BanDiscordHelpers::JoinArgs(Args, ReasonStart)
		: TEXT("Muted by Discord admin");

	MuteReg->MutePlayer(Uid, DisplayName, Reason, SenderName, Minutes);

	const FString DurStr = (Minutes > 0)
		? FString::Printf(TEXT(" for **%d minute(s)**"), Minutes)
		: TEXT(" **indefinitely**");

	const FString MuteMsg = FString::Printf(
		TEXT("🔇 Muted **%s** (`%s`)%s.\nReason: %s\nMuted by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *DurStr, *Reason, *SenderName);
	CachedProvider->SendDiscordChannelMessage(ChannelId, MuteMsg);
	PostModerationLog(MuteMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !warn
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleWarnCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.Num() < 2)
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, TEXT("Usage: `!warn <PUID|name> <reason...>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, ErrorMsg);
		return;
	}

	const FString Reason = BanDiscordHelpers::JoinArgs(Args, 1);

	UGameInstance* GI = GetGameInstance();
	UPlayerWarningRegistry* WarnReg = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
	if (!WarnReg)
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			TEXT("❌ The warn command requires the BanSystem mod to be installed."));
		return;
	}

	WarnReg->AddWarning(Uid, DisplayName, Reason, SenderName);
	const int32 WarnCount = WarnReg->GetWarningCount(Uid);

	FBanDiscordNotifier::NotifyWarningIssued(Uid, DisplayName, Reason, SenderName, WarnCount);

	const FString WarnMsg = FString::Printf(
		TEXT("⚠️ Warned **%s** (`%s`).\nReason: %s\nTotal warnings: **%d**\nWarned by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *Reason, WarnCount, *SenderName);
	CachedProvider->SendDiscordChannelMessage(ChannelId, WarnMsg);
	PostModerationLog(WarnMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// !announce
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleAnnounceCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId,
                                                  const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId, TEXT("Usage: `!announce <message...>`"));
		return;
	}

	const FString Message = BanDiscordHelpers::JoinArgs(Args, 0);

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		CachedProvider->SendDiscordChannelMessage(ChannelId,
			TEXT("❌ No active game world found. Is the server running?"));
		return;
	}

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (PC) PC->ClientMessage(FString::Printf(TEXT("[Announcement] %s"), *Message));
	}

	const FString AnnounceConfirm = FString::Printf(
		TEXT("📢 Announcement sent to all in-game players:\n> %s\nSent by: %s"),
		*Message, *SenderName);
	CachedProvider->SendDiscordChannelMessage(ChannelId, AnnounceConfirm);
	PostModerationLog(AnnounceConfirm);
}

// ─────────────────────────────────────────────────────────────────────────────
// Moderation log
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::PostModerationLog(const FString& Message) const
{
if (!CachedProvider) return;
if (Config.ModerationLogChannelId.IsEmpty()) return;
if (Config.ModerationLogChannelId == TEXT("0")) return;

CachedProvider->SendDiscordChannelMessage(Config.ModerationLogChannelId, Message);
}
