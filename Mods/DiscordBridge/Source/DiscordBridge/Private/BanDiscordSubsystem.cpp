// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordSubsystem.h"

// BanSystem public API
#include "BanDatabase.h"
#include "BanEnforcer.h"
#include "BanTypes.h"
#include "BanSystemConfig.h"
#include "PlayerSessionRegistry.h"
#include "PlayerWarningRegistry.h"
#include "BanDiscordNotifier.h"
#include "BanAppealRegistry.h"
#include "BanAuditLog.h"
#include "ScheduledBanRegistry.h"

// BanChatCommands public API
#include "MuteRegistry.h"
#include "Commands/BanChatCommands.h"
#include "BanChatCommandsConfig.h"

#include "Misc/DefaultValueHelper.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"

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

	// Number of bans shown per /ban list page.
	static constexpr int32 BanListPageSize = 10;

	/**
	 * After a primary ban for PrimaryUid has been committed, look up the
	 * player's other identifiers in the session registry and create matching
	 * ban records:
	 *  - EOS UID → also bans the IP address recorded for that UID.
	 *  - IP UID  → also bans every EOS UID that has connected from that IP.
	 * All newly created records are cross-linked via UBanDatabase::LinkBans().
	 * Returns a list of additionally created UIDs (for confirmation messages).
	 */
	static TArray<FString> AddCounterpartBans(const UBanDiscordSubsystem* Self,
	                                           UBanDatabase* DB,
	                                           const FString& PrimaryUid,
	                                           const FString& DisplayName,
	                                           const FString& Reason,
	                                           const FString& BannedBy,
	                                           bool bIsPermanent,
	                                           FDateTime ExpireDate)
	{
		TArray<FString> Added;
		if (!DB) return Added;

		UPlayerSessionRegistry* Registry = GetRegistry(Self);
		if (!Registry) return Added;

		FString Platform, RawId;
		UBanDatabase::ParseUid(PrimaryUid, Platform, RawId);

		auto MakeEntry = [&](const FString& Uid, const FString& Plat,
		                     const FString& RawUid, const FString& Name) -> FBanEntry
		{
			FBanEntry E;
			E.Uid        = Uid;
			E.Platform   = Plat;
			E.PlayerUID  = RawUid;
			E.PlayerName = Name;
			E.Reason     = Reason;
			E.BannedBy   = BannedBy;
			E.BanDate    = FDateTime::UtcNow();
			E.bIsPermanent = bIsPermanent;
			E.ExpireDate   = ExpireDate;
			E.LinkedUids.Add(PrimaryUid);
			return E;
		};

		if (Platform == TEXT("EOS"))
		{
			FPlayerSessionRecord Rec;
			if (Registry->FindByUid(PrimaryUid, Rec) && !Rec.IpAddress.IsEmpty())
			{
				const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Rec.IpAddress);
				FBanEntry IpEntry = MakeEntry(IpUid, TEXT("IP"), Rec.IpAddress, DisplayName);
				if (DB->AddBan(IpEntry))
				{
					DB->LinkBans(PrimaryUid, IpUid);
					Added.Add(IpUid);
				}
			}
		}
		else if (Platform == TEXT("IP"))
		{
			TArray<FPlayerSessionRecord> Records = Registry->FindByIp(RawId);
			for (const FPlayerSessionRecord& Rec : Records)
			{
				if (Rec.Uid.IsEmpty()) continue;
				FString RecPlat, RecRawId;
				UBanDatabase::ParseUid(Rec.Uid, RecPlat, RecRawId);
				const FString RecName = Rec.DisplayName.IsEmpty() ? DisplayName : Rec.DisplayName;
				FBanEntry EosEntry = MakeEntry(Rec.Uid, RecPlat, RecRawId, RecName);
				if (DB->AddBan(EosEntry))
				{
					DB->LinkBans(PrimaryUid, Rec.Uid);
					Added.Add(Rec.Uid);
				}
			}
		}
		return Added;
	}

	/**
	 * After removing the ban for PrimaryUid, remove all linked bans so no
	 * counterpart (IP or EOS) ban is left behind.
	 * Uses the LinkedUids collected from the ban entry before deletion and also
	 * checks the session registry for any unlinked counterparts.
	 * Returns the number of additional records removed.
	 */
	static int32 RemoveCounterpartBans(const UBanDiscordSubsystem* Self,
	                                    UBanDatabase* DB,
	                                    const FString& PrimaryUid,
	                                    const TArray<FString>& LinkedUids)
	{
		if (!DB) return 0;
		int32 Removed = 0;

		for (const FString& LinkedUid : LinkedUids)
		{
			if (DB->RemoveBanByUid(LinkedUid))
				++Removed;
		}

		UPlayerSessionRegistry* Registry = GetRegistry(Self);
		if (!Registry) return Removed;

		FString Platform, RawId;
		UBanDatabase::ParseUid(PrimaryUid, Platform, RawId);

		if (Platform == TEXT("EOS"))
		{
			FPlayerSessionRecord Rec;
			if (Registry->FindByUid(PrimaryUid, Rec) && !Rec.IpAddress.IsEmpty())
			{
				const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Rec.IpAddress);
				if (!LinkedUids.Contains(IpUid) && DB->RemoveBanByUid(IpUid))
					++Removed;
			}
		}
		else if (Platform == TEXT("IP"))
		{
			TArray<FPlayerSessionRecord> Records = Registry->FindByIp(RawId);
			for (const FPlayerSessionRecord& Rec : Records)
			{
				if (Rec.Uid.IsEmpty()) continue;
				if (!LinkedUids.Contains(Rec.Uid) && DB->RemoveBanByUid(Rec.Uid))
					++Removed;
			}
		}
		return Removed;
	}
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
	FBanBridgeConfig::RestoreDefaultConfigIfNeeded();
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
	if (CachedProvider && InteractionDelegateHandle.IsValid())
	{
		CachedProvider->UnsubscribeInteraction(InteractionDelegateHandle);
		InteractionDelegateHandle.Reset();
	}

	// Unbind appeal-submitted notification when clearing the provider.
	if (AppealSubmittedDelegateHandle.IsValid())
	{
		UBanAppealRegistry::OnBanAppealSubmitted.Remove(AppealSubmittedDelegateHandle);
		AppealSubmittedDelegateHandle.Reset();
	}

	CachedProvider = InProvider;

	// Subscribe to the new provider.
	if (CachedProvider)
	{
		TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);

		InteractionDelegateHandle = CachedProvider->SubscribeInteraction(
			[WeakThis](const TSharedPtr<FJsonObject>& InteractionObj)
			{
				if (UBanDiscordSubsystem* Self = WeakThis.Get())
					Self->OnDiscordInteraction(InteractionObj);
			});

		// Bind to appeal-submitted notifications so new appeals are posted to Discord.
		AppealSubmittedDelegateHandle = UBanAppealRegistry::OnBanAppealSubmitted.AddLambda(
			[WeakThis](const FBanAppealEntry& Entry)
			{
				UBanDiscordSubsystem* Self = WeakThis.Get();
				if (!Self || !Self->CachedProvider)
					return;

				const FString& ChannelId = Self->Config.ModerationLogChannelId;
				if (ChannelId.IsEmpty())
					return;

				const FString SubmittedStr = Entry.SubmittedAt.ToIso8601();
				const FString Reason       = Entry.Reason.IsEmpty()      ? TEXT("(none)") : Entry.Reason;
				const FString Contact      = Entry.ContactInfo.IsEmpty() ? TEXT("(none)") : Entry.ContactInfo;

				const FString Message = FString::Printf(
					TEXT(":scales: **New Ban Appeal Submitted**\n")
					TEXT("**ID:** #%lld\n")
					TEXT("**UID:** %s\n")
					TEXT("**Contact:** %s\n")
					TEXT("**Submitted:** %s\n")
					TEXT("**Reason:** %s\n\n")
					TEXT("React with ✅ to approve (unban) or ❌ to deny (dismiss).\n")
					TEXT("Use `/appeal approve %lld` or `/appeal deny %lld` to act."),
					Entry.Id, *Entry.Uid, *Contact, *SubmittedStr, *Reason,
					Entry.Id, Entry.Id);

				Self->Respond(ChannelId, Message);
			});

		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Discord provider injected; "
		            "ban commands are %s."),
		       (Config.AdminRoleId.IsEmpty() && Config.ModeratorRoleId.IsEmpty())
		           ? TEXT("DISABLED (neither AdminRoleId nor ModeratorRoleId is set)")
		           : TEXT("enabled"));
	}
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
	return Config.IsAdminRole(Roles);
}

bool UBanDiscordSubsystem::IsModeratorMember(const TArray<FString>& Roles) const
{
	return Config.IsModeratorRole(Roles);
}

void UBanDiscordSubsystem::Respond(const FString& ChannelId, const FString& Message)
{
	if (!CachedProvider) return;
	// Always post to the command channel so the result is visible to all mods.
	if (!ChannelId.IsEmpty())
		CachedProvider->SendDiscordChannelMessage(ChannelId, Message);
	// Also send as an ephemeral follow-up to the slash command interaction so
	// the admin sees the result inline even if they missed the channel message.
	if (!PendingInteractionToken.IsEmpty())
		CachedProvider->FollowUpInteraction(PendingInteractionToken, Message, /*bEphemeral=*/true);
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

int32 UBanDiscordSubsystem::ParseDurationMinutes(const FString& DurationStr)
{
	if (DurationStr.IsEmpty()) return 0;
	const FString Lower = DurationStr.ToLower();

	// "perm" / "permanent" → 0 (permanent)
	if (Lower == TEXT("perm") || Lower == TEXT("permanent")) return 0;

	// Multiplier suffixes: m=minutes, h=hours, d=days, w=weeks.
	auto ParseWithSuffix = [](const FString& S, TCHAR Suffix, int32 Multiplier) -> int32
	{
		if (!S.EndsWith(FString::Chr(Suffix))) return -1;
		const FString NumPart = S.LeftChop(1);
		int32 Val = 0;
		if (!FDefaultValueHelper::ParseInt(NumPart, Val) || Val <= 0) return -1;
		return Val * Multiplier;
	};

	int32 Result;
	if ((Result = ParseWithSuffix(Lower, TEXT('w'), 10080)) > 0) return Result;
	if ((Result = ParseWithSuffix(Lower, TEXT('d'), 1440))  > 0) return Result;
	if ((Result = ParseWithSuffix(Lower, TEXT('h'), 60))    > 0) return Result;
	if ((Result = ParseWithSuffix(Lower, TEXT('m'), 1))     > 0) return Result;

	// Plain number — assume minutes.
	int32 Val = 0;
	if (FDefaultValueHelper::ParseInt(DurationStr, Val) && Val > 0)
		return Val;

	return 0;
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

		// 2.5 IP UID: "IP:<address>" — used for explicit IP bans.
		if (Platform == TEXT("IP") && !RawId.IsEmpty())
		{
			OutUid         = UBanDatabase::MakeUid(TEXT("IP"), RawId);
			OutDisplayName = RawId;
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
		const int32 TotalRecords = Registry->GetCount();
		if (TotalRecords == 0)
		{
			OutErrorMsg = FString::Printf(
				TEXT("❌ No player found matching `%s`.\n"
				     "⚠️ The session registry is **empty** — no players have connected to the server "
				     "since BanSystem was installed.\n"
				     "💡 To ban a player who has not yet connected, use their EOS PUID directly:\n"
				     "`/ban add player:<32-char-hex-PUID>`\n"
				     "Find the PUID in the server log (`BanEnforcer: cached EOS PUID …`) or your "
				     "server admin panel."),
				*Arg);
		}
		else
		{
			OutErrorMsg = FString::Printf(
				TEXT("❌ No player found matching `%s` in the session registry (%d record(s)).\n"
				     "Check the spelling or use the player's EOS PUID directly:\n"
				     "`/ban add player:<32-char-hex-PUID>`"),
				*Arg, TotalRecords);
		}
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
// /ban add / /ban temp
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanCommand(const TArray<FString>& Args,
                                             const FString& ChannelId,
                                             const FString& SenderName,
                                             bool bTemporary)
{
	const FString CmdName = bTemporary ? TEXT("/ban temp") : TEXT("/ban add");

	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	// Validate minimum argument count.
	// /ban add:  <target> [reason]          → 1 arg minimum
	// /ban temp: <target> <minutes> [reason] → 2 args minimum
	const int32 MinArgs = bTemporary ? 2 : 1;
	if (Args.Num() < MinArgs)
	{
		const FString Usage = bTemporary
			? TEXT("Usage: `/ban temp <PUID|name> <minutes> [reason]`")
			: TEXT("Usage: `/ban add <PUID|name> [reason]`");
		Respond(ChannelId, Usage);
		return;
	}

	// Resolve target.
	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	// Parse duration for /ban temp.
	int32 DurationMinutes = 0;
	int32 ReasonStartIdx  = 1;
	if (bTemporary)
	{
		if (!FDefaultValueHelper::ParseInt(Args[1], DurationMinutes) || DurationMinutes <= 0)
		{
			Respond(ChannelId,
				TEXT("❌ `<minutes>` must be a positive integer.\n"
				     "Usage: `/ban temp <PUID|name> <minutes> [reason]`"));
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
		Respond(ChannelId,
			TEXT("❌ Failed to write the ban to the database. Check server logs."));
		return;
	}

	// Kick if the player is currently connected.
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		UBanEnforcer::KickConnectedPlayer(World, Uid, Entry.GetKickMessage());

	// Also ban the counterpart identifier (IP↔EOS) so every identity is blocked.
	const TArray<FString> ExtraUids = BanDiscordHelpers::AddCounterpartBans(
		this, DB, Uid, DisplayName, Entry.Reason, SenderName,
		Entry.bIsPermanent, Entry.ExpireDate);

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

	if (ExtraUids.Num() > 0)
	{
		Msg += FString::Printf(TEXT("\nAlso banned: `%s`"),
			*FString::Join(ExtraUids, TEXT("`, `")));
	}

	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: %s banned %s (%s). Reason: %s"),
	       *SenderName, *DisplayName, *Uid, *Entry.Reason);

	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban remove
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleUnbanCommand(const TArray<FString>& Args,
                                               const FString& ChannelId,
                                               const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `!unban <PUID>`\nPUID can be a 32-char hex string or `EOS:<puid>`."));
		return;
	}

	// Resolve to a compound UID.
	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	// Collect linked UIDs before removing the entry so we can clean up counterparts.
	FBanEntry BanRecord;
	const bool bHadRecord = DB->GetBanByUid(Uid, BanRecord);

	if (!DB->RemoveBanByUid(Uid))
	{
		// No ban record was found in the database for this UID.  That means the
		// player is already not banned (the file may have been absent or the ban
		// was never persisted).  Report this as a non-fatal note so an admin can
		// always clear a stale or missing-file situation.
		const FString Msg = FString::Printf(
			TEXT("ℹ️ No active ban record found for **%s** (`%s`) — the player is already unbanned.\nUnbanned by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);

		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: %s issued !unban for %s (%s) — no record in DB (already clear)."),
		       *SenderName, *DisplayName, *Uid);

		Respond(ChannelId, Msg);
		PostModerationLog(Msg);
		return;
	}

	// Remove counterpart bans (linked UIDs + session registry lookup).
	int32 ExtraRemoved = 0;
	if (bHadRecord)
		ExtraRemoved = BanDiscordHelpers::RemoveCounterpartBans(this, DB, Uid, BanRecord.LinkedUids);

	FString Msg = FString::Printf(
		TEXT("✅ Ban removed for **%s** (`%s`).\nUnbanned by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);

	if (ExtraRemoved > 0)
		Msg += FString::Printf(TEXT("\nAlso removed %d linked ban(s)."), ExtraRemoved);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: %s unbanned %s (%s)."),
	       *SenderName, *DisplayName, *Uid);

	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban check
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanCheckCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban check <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
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

	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban list
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanListCommand(const TArray<FString>& Args,
                                                 const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	DB->ReloadIfChanged();
	TArray<FBanEntry> ActiveBans = DB->GetActiveBans();

	if (ActiveBans.IsEmpty())
	{
		Respond(ChannelId, TEXT("✅ No active bans."));
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

	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /player history
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandlePlayerHistoryCommand(const TArray<FString>& Args,
                                                       const FString& ChannelId)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/player history <name|PUID|IP>`");
		return;
	}

	UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this);
	if (!Registry)
	{
		Respond(ChannelId,
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
		Respond(ChannelId,
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

	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod kick
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleKickCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId, TEXT("Usage: `/mod kick <PUID|name> [reason]`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	const FString Reason = (Args.Num() > 1) ? BanDiscordHelpers::JoinArgs(Args, 1) : TEXT("Kicked by Discord admin");

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		Respond(ChannelId,
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
		Respond(ChannelId, KickMsg);
		PostModerationLog(KickMsg);
	}
	else
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ Player **%s** (`%s`) is not currently connected."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod mute / /mod unmute
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleMuteCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName,
                                              bool bMute)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			bMute
			? TEXT("Usage: `/mod mute <PUID|name> [minutes] [reason]`")
			: TEXT("Usage: `/mod unmute <PUID|name>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	if (!bMute)
	{
		if (!MuteReg->UnmutePlayer(Uid))
		{
			Respond(ChannelId,
				FString::Printf(TEXT("⚠️ **%s** (`%s`) is not currently muted."),
					*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
			return;
		}
		const FString UnmuteMsg = FString::Printf(
			TEXT("✅ Unmuted **%s** (`%s`).\nUnmuted by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);
		Respond(ChannelId, UnmuteMsg);
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
	Respond(ChannelId, MuteMsg);
	PostModerationLog(MuteMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /warn add
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleWarnCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.Num() < 2)
	{
		Respond(ChannelId, TEXT("Usage: `/warn add <PUID|name> <reason...>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	const FString Reason = BanDiscordHelpers::JoinArgs(Args, 1);

	UGameInstance* GI = GetGameInstance();
	UPlayerWarningRegistry* WarnReg = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
	if (!WarnReg)
	{
		Respond(ChannelId,
			TEXT("❌ The warn command requires the BanSystem mod to be installed."));
		return;
	}

	WarnReg->AddWarning(Uid, DisplayName, Reason, SenderName);
	const int32 WarnCount = WarnReg->GetWarningCount(Uid);

	FBanDiscordNotifier::NotifyWarningIssued(Uid, DisplayName, Reason, SenderName, WarnCount);

	const FString WarnMsg = FString::Printf(
		TEXT("⚠️ Warned **%s** (`%s`).\nReason: %s\nTotal warnings: **%d**\nWarned by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *Reason, WarnCount, *SenderName);
	Respond(ChannelId, WarnMsg);
	PostModerationLog(WarnMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod announce
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleAnnounceCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId,
                                                  const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId, TEXT("Usage: `/mod announce <message...>`"));
		return;
	}

	const FString Message = BanDiscordHelpers::JoinArgs(Args, 0);

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		Respond(ChannelId,
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
	Respond(ChannelId, AnnounceConfirm);
	PostModerationLog(AnnounceConfirm);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban removename
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleUnbanNameCommand(const TArray<FString>& Args,
                                                   const FString& ChannelId,
                                                   const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `!unbanname <name_substring>`"));
		return;
	}

	UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this);
	if (!Registry)
	{
		Respond(ChannelId,
			TEXT("❌ PlayerSessionRegistry is not available."));
		return;
	}

	const FString NameQuery = BanDiscordHelpers::JoinArgs(Args, 0);
	TArray<FPlayerSessionRecord> Matches = Registry->FindByName(NameQuery);

	if (Matches.IsEmpty())
	{
		Respond(ChannelId,
			FString::Printf(TEXT("❌ No session record found for name `%s`."), *NameQuery));
		return;
	}
	if (Matches.Num() > 1)
	{
		FString List;
		const int32 Show = FMath::Min(Matches.Num(), 5);
		for (int32 i = 0; i < Show; ++i)
		{
			if (i > 0) List += TEXT(", ");
			List += FString::Printf(TEXT("`%s`"), *Matches[i].DisplayName);
		}
		if (Matches.Num() > Show)
			List += FString::Printf(TEXT(", +%d more"), Matches.Num() - Show);
		Respond(ChannelId,
			FString::Printf(TEXT("❌ Ambiguous name `%s` — %d matches: %s. Use `!unban <PUID>` instead."),
				*NameQuery, Matches.Num(), *List));
		return;
	}

	const FPlayerSessionRecord& Record = Matches[0];
	int32 Removed = 0;

	// Remove EOS ban.
	if (DB->RemoveBanByUid(Record.Uid))
		++Removed;

	// Remove IP ban if recorded.
	if (!Record.IpAddress.IsEmpty())
	{
		const FString IpUid = UBanDatabase::MakeUid(TEXT("IP"), Record.IpAddress);
		if (DB->RemoveBanByUid(IpUid))
			++Removed;
	}

	if (Removed == 0)
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ No active ban found for **%s** (`%s`)."),
				*BanDiscordHelpers::EscapeMarkdown(Record.DisplayName), *Record.Uid));
		return;
	}

	const FString Msg = FString::Printf(
		TEXT("✅ Removed %d ban record(s) for **%s** (`%s`).\nUnbanned by: %s"),
		Removed,
		*BanDiscordHelpers::EscapeMarkdown(Record.DisplayName),
		*Record.Uid,
		*SenderName);
	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: %s unbanname %s (%s)."),
		*SenderName, *Record.DisplayName, *Record.Uid);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban byname
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanNameCommand(const TArray<FString>& Args,
                                                 const FString& ChannelId,
                                                 const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban byname <name_substring> [reason]`");
		return;
	}

	UPlayerSessionRegistry* Registry = BanDiscordHelpers::GetRegistry(this);
	if (!Registry)
	{
		Respond(ChannelId,
			TEXT("❌ PlayerSessionRegistry is not available."));
		return;
	}

	TArray<FPlayerSessionRecord> Matches = Registry->FindByName(Args[0]);
	if (Matches.IsEmpty())
	{
		Respond(ChannelId,
			FString::Printf(TEXT("❌ No session record found for name `%s`."), *Args[0]));
		return;
	}
	if (Matches.Num() > 1)
	{
		FString List;
		const int32 Show = FMath::Min(Matches.Num(), 5);
		for (int32 i = 0; i < Show; ++i)
		{
			if (i > 0) List += TEXT(", ");
			List += FString::Printf(TEXT("`%s`"), *Matches[i].DisplayName);
		}
		if (Matches.Num() > Show)
			List += FString::Printf(TEXT(", +%d more"), Matches.Num() - Show);
		Respond(ChannelId,
			FString::Printf(TEXT("❌ Ambiguous name `%s` — %d matches: %s. Use `/ban add <PUID>` instead."),
				*Args[0], Matches.Num(), *List));
		return;
	}

	const FPlayerSessionRecord& Record = Matches[0];
	const FString Reason = (Args.Num() > 1)
		? BanDiscordHelpers::JoinArgs(Args, 1)
		: TEXT("No reason given");

	int32 Banned = 0;

	// Ban EOS PUID.
	{
		FBanEntry Entry;
		Entry.Uid        = Record.Uid;
		UBanDatabase::ParseUid(Record.Uid, Entry.Platform, Entry.PlayerUID);
		Entry.PlayerName = Record.DisplayName;
		Entry.Reason     = Reason;
		Entry.BannedBy   = SenderName;
		Entry.BanDate    = FDateTime::UtcNow();
		Entry.bIsPermanent = true;
		Entry.ExpireDate   = FDateTime(0);
		if (DB->AddBan(Entry)) ++Banned;
	}

	// Also ban IP if recorded.
	FString IpUid;
	if (!Record.IpAddress.IsEmpty())
	{
		IpUid = UBanDatabase::MakeUid(TEXT("IP"), Record.IpAddress);
		FBanEntry IpEntry;
		IpEntry.Uid        = IpUid;
		IpEntry.Platform   = TEXT("IP");
		IpEntry.PlayerUID  = Record.IpAddress;
		IpEntry.PlayerName = Record.DisplayName;
		IpEntry.Reason     = Reason;
		IpEntry.BannedBy   = SenderName;
		IpEntry.BanDate    = FDateTime::UtcNow();
		IpEntry.bIsPermanent = true;
		IpEntry.ExpireDate   = FDateTime(0);
		IpEntry.LinkedUids.Add(Record.Uid);
		if (DB->AddBan(IpEntry))
		{
			++Banned;
			// Cross-link the EOS ban.
			DB->LinkBans(Record.Uid, IpUid);
		}
	}

	// Kick if currently connected.
	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		UBanEnforcer::KickConnectedPlayer(World, Record.Uid, FBanEntry().GetKickMessage());

	const FString SafeName = BanDiscordHelpers::EscapeMarkdown(Record.DisplayName);
	FString Msg = FString::Printf(
		TEXT("✅ Banned **%s** (`%s`) — %d record(s) added.\nReason: %s\nBanned by: %s"),
		*SafeName, *Record.Uid, Banned, *Reason, *SenderName);
	if (!IpUid.IsEmpty())
		Msg += FString::Printf(TEXT("\nIP ban: `%s`"), *IpUid);

	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: %s banname %s (%s). Reason: %s"),
		*SenderName, *Record.DisplayName, *Record.Uid, *Reason);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban reason
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanReasonCommand(const TArray<FString>& Args,
                                                   const FString& ChannelId,
                                                   const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban reason <PUID|name> <new reason...>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	FBanEntry Entry;
	if (!DB->GetBanByUid(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ No ban record found for **%s** (`%s`)."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	const FString OldReason = Entry.Reason;
	Entry.Reason = BanDiscordHelpers::JoinArgs(Args, 1);
	DB->AddBan(Entry);

	const FString Msg = FString::Printf(
		TEXT("✅ Ban reason updated for **%s** (`%s`).\nOld reason: %s\nNew reason: %s\nUpdated by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid,
		*OldReason, *Entry.Reason, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban link / /ban unlink
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleLinkBansCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId,
                                                  const FString& SenderName,
                                                  bool bLink)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	const FString CmdName = bLink ? TEXT("/ban link") : TEXT("/ban unlink");
	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			FString::Printf(TEXT("Usage: `%s <UID1> <UID2>`"), *CmdName));
		return;
	}

	// Resolve both arguments to compound UIDs.
	FString Uid1, Name1, Err1;
	FString Uid2, Name2, Err2;
	if (!ResolveTarget(Args[0], Uid1, Name1, Err1))
	{
		Respond(ChannelId, Err1);
		return;
	}
	if (!ResolveTarget(Args[1], Uid2, Name2, Err2))
	{
		Respond(ChannelId, Err2);
		return;
	}

	if (Uid1.Equals(Uid2, ESearchCase::IgnoreCase))
	{
		Respond(ChannelId,
			TEXT("❌ Both UIDs are the same; no change made."));
		return;
	}

	const bool bOk = bLink ? DB->LinkBans(Uid1, Uid2) : DB->UnlinkBans(Uid1, Uid2);
	if (!bOk)
	{
		Respond(ChannelId,
			bLink
			? TEXT("⚠️ Could not link bans. Ensure both UIDs have ban records.")
			: TEXT("⚠️ No link found between those UIDs."));
		return;
	}

	const FString Verb = bLink ? TEXT("linked") : TEXT("unlinked");
	const FString Msg = FString::Printf(
		TEXT("✅ Successfully %s `%s` and `%s`.\nBy: %s"),
		*Verb, *Uid1, *Uid2, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban extend
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleExtendBanCommand(const TArray<FString>& Args,
                                                   const FString& ChannelId,
                                                   const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban extend <PUID|name> <minutes>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	int32 Minutes = 0;
	if (!FDefaultValueHelper::ParseInt(Args[1], Minutes) || Minutes <= 0)
	{
		Respond(ChannelId,
			TEXT("❌ `<minutes>` must be a positive integer."));
		return;
	}

	FBanEntry Entry;
	if (!DB->IsCurrentlyBannedByAnyId(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ **%s** (`%s`) is not currently banned."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	if (Entry.bIsPermanent)
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ **%s** has a **permanent** ban — cannot extend."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName)));
		return;
	}

	Entry.ExpireDate = Entry.ExpireDate + FTimespan::FromMinutes(Minutes);
	DB->AddBan(Entry);

	const FString Msg = FString::Printf(
		TEXT("✅ Extended ban for **%s** (`%s`) by **%d minute(s)**.\nNew expiry: %s UTC\nBy: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, Minutes,
		*Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
		*SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban duration
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleDurationCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban duration <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	FBanEntry Entry;
	if (!DB->IsCurrentlyBannedByAnyId(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("✅ **%s** (`%s`) is not currently banned."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	FString DurStr;
	if (Entry.bIsPermanent)
	{
		DurStr = TEXT("**permanent**");
	}
	else
	{
		const FTimespan Remaining = Entry.ExpireDate - FDateTime::UtcNow();
		if (Remaining.GetTicks() <= 0)
		{
			DurStr = TEXT("expired (ban will be pruned on next check)");
		}
		else
		{
			const int32 TotalMins  = static_cast<int32>(Remaining.GetTotalMinutes());
			const int32 Days       = TotalMins / (60 * 24);
			const int32 Hours      = (TotalMins % (60 * 24)) / 60;
			const int32 Mins       = TotalMins % 60;
			DurStr = FString::Printf(TEXT("**%dd %dh %dm** remaining (expires %s UTC)"),
				Days, Hours, Mins,
				*Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
		}
	}

	Respond(ChannelId,
		FString::Printf(TEXT("⏱️ **%s** (`%s`) — ban duration: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *DurStr));
}

// ─────────────────────────────────────────────────────────────────────────────
// /warn list
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleWarningsCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/warn list <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayerWarningRegistry* WarnReg = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
	if (!WarnReg)
	{
		Respond(ChannelId,
			TEXT("❌ The warnings system requires the BanSystem mod to be installed."));
		return;
	}

	TArray<FWarningEntry> Warnings = WarnReg->GetWarningsForUid(Uid);
	if (Warnings.IsEmpty())
	{
		Respond(ChannelId,
			FString::Printf(TEXT("✅ No warnings on record for **%s** (`%s`)."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	FString Body;
	Body.Reserve(1600);
	Body += TEXT("```\n");
	Body += FString::Printf(TEXT("%-4s  %-20s  %-12s  %s\n"),
		TEXT("ID"), TEXT("Date (UTC)"), TEXT("By"), TEXT("Reason"));
	Body += FString(TEXT("─"), 72) + TEXT("\n");

	const int32 ShowMax = FMath::Min(Warnings.Num(), 15);
	for (int32 i = 0; i < ShowMax; ++i)
	{
		const FWarningEntry& W = Warnings[i];
		const FString DateStr  = W.WarnDate.ToString(TEXT("%m-%d %H:%M"));
		const FString ByShort  = BanDiscordHelpers::Truncate(W.WarnedBy, 12);
		const FString ReasonSh = BanDiscordHelpers::Truncate(W.Reason, 40);
		Body += FString::Printf(TEXT("%-4lld  %-20s  %-12s  %s\n"),
			W.Id, *DateStr, *ByShort, *ReasonSh);
	}
	Body += TEXT("```");

	FString Header = FString::Printf(
		TEXT("**Warnings for %s** (`%s`) — %d total\n"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, Warnings.Num());
	if (Warnings.Num() > ShowMax)
		Header += FString::Printf(TEXT("_(Showing first %d)_\n"), ShowMax);

	FString Msg = Header + Body;
	if (Msg.Len() > 1990)
		Msg = Msg.Left(1940) + TEXT("\n...(truncated)```");
	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /warn clearall
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleClearWarnsCommand(const TArray<FString>& Args,
                                                    const FString& ChannelId,
                                                    const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/warn clearall <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayerWarningRegistry* WarnReg = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
	if (!WarnReg)
	{
		Respond(ChannelId,
			TEXT("❌ The warnings system requires the BanSystem mod to be installed."));
		return;
	}

	const int32 Removed = WarnReg->ClearWarningsForUid(Uid);
	const FString Msg = FString::Printf(
		TEXT("✅ Cleared **%d** warning(s) for **%s** (`%s`).\nBy: %s"),
		Removed,
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /warn clearone (by ID)
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleClearWarnByIdCommand(const TArray<FString>& Args,
                                                       const FString& ChannelId,
                                                       const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/warn clearone <warning_id>`\nSee warning IDs with `/warn list <player>`."));
		return;
	}

	int64 WarnId = 0;
	{
		// Parse as int64; FDefaultValueHelper only parses int32, use FCString.
		TCHAR* End = nullptr;
		WarnId = static_cast<int64>(FCString::Strtoi64(*Args[0], &End, 10));
		if (WarnId <= 0)
		{
			Respond(ChannelId,
				TEXT("❌ `<warning_id>` must be a positive integer."));
			return;
		}
	}

	UGameInstance* GI = GetGameInstance();
	UPlayerWarningRegistry* WarnReg = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
	if (!WarnReg)
	{
		Respond(ChannelId,
			TEXT("❌ The warnings system requires the BanSystem mod to be installed."));
		return;
	}

	if (!WarnReg->DeleteWarningById(WarnId))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ No warning found with ID `%lld`."), WarnId));
		return;
	}

	const FString Msg = FString::Printf(
		TEXT("✅ Deleted warning #%lld.\nBy: %s"), WarnId, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /player note
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleNoteCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			TEXT("Usage: `/player note <PUID|name> <text...>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayerNoteRegistry* NoteReg = GI ? GI->GetSubsystem<UPlayerNoteRegistry>() : nullptr;
	if (!NoteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Player notes require the BanChatCommands mod to be installed."));
		return;
	}

	const FString NoteText = BanDiscordHelpers::JoinArgs(Args, 1);
	NoteReg->AddNote(Uid, DisplayName, NoteText, SenderName);

	Respond(ChannelId,
		FString::Printf(TEXT("📝 Note added for **%s** (`%s`).\nNote: %s\nAdded by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *NoteText, *SenderName));
}

// ─────────────────────────────────────────────────────────────────────────────
// /player notes
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleNotesCommand(const TArray<FString>& Args,
                                               const FString& ChannelId)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/player notes <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UPlayerNoteRegistry* NoteReg = GI ? GI->GetSubsystem<UPlayerNoteRegistry>() : nullptr;
	if (!NoteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Player notes require the BanChatCommands mod to be installed."));
		return;
	}

	TArray<FPlayerNoteEntry> Notes = NoteReg->GetNotesForUid(Uid);
	if (Notes.IsEmpty())
	{
		Respond(ChannelId,
			FString::Printf(TEXT("📝 No notes on record for **%s** (`%s`)."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	FString Body;
	Body.Reserve(1400);
	Body += TEXT("```\n");

	const int32 ShowMax = FMath::Min(Notes.Num(), 10);
	for (int32 i = 0; i < ShowMax; ++i)
	{
		const FPlayerNoteEntry& N = Notes[i];
		const FString DateStr = N.NoteDate.ToString(TEXT("%Y-%m-%d %H:%M"));
		Body += FString::Printf(TEXT("[%s] %s — by %s: %s\n"),
			*DateStr,
			*FString::Printf(TEXT("#%lld"), N.Id),
			*BanDiscordHelpers::Truncate(N.AddedBy, 16),
			*BanDiscordHelpers::Truncate(N.Note, 60));
	}
	Body += TEXT("```");

	FString Header = FString::Printf(
		TEXT("**Notes for %s** (`%s`) — %d total\n"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, Notes.Num());
	FString Msg = Header + Body;
	if (Msg.Len() > 1990)
		Msg = Msg.Left(1940) + TEXT("\n...(truncated)```");
	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban reason
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleReasonCommand(const TArray<FString>& Args,
                                                const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/ban reason <UID>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	FBanEntry Entry;
	if (!DB->GetBanByUid(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("✅ No ban record found for `%s`."), *Uid));
		return;
	}

	Respond(ChannelId,
		FString::Printf(TEXT("🔨 **%s** (`%s`) — ban reason: %s\n(Banned by: %s on %s UTC)"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid,
			*Entry.Reason, *Entry.BannedBy,
			*Entry.BanDate.ToString(TEXT("%Y-%m-%d %H:%M:%S"))));
}

// ─────────────────────────────────────────────────────────────────────────────
// /admin reloadconfig
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleReloadConfigCommand(const FString& ChannelId,
                                                      const FString& SenderName)
{
	if (!CachedProvider) return;

	Config = FBanBridgeConfig::Load();

	UE_LOG(LogBanDiscord, Log,
		TEXT("BanDiscordSubsystem: Config reloaded by %s."), *SenderName);

	Respond(ChannelId,
		TEXT(":white_check_mark: BanBridge config reloaded."));
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod ban
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleModBanCommand(const TArray<FString>& Args,
                                                const FString& ChannelId,
                                                const FString& SenderName)
{
	if (!CachedProvider) return;

	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	if (!DB)
	{
		Respond(ChannelId,
			TEXT("❌ BanSystem is not available on this server."));
		return;
	}

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/mod ban <PUID|name> [reason]`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	// Use the ModBanDurationMinutes from BanChatCommandsConfig when available,
	// otherwise fall back to the classic 30-minute default.
	int32 DurationMinutes = 30;
	if (const UBanChatCommandsConfig* BccCfg = UBanChatCommandsConfig::Get())
		DurationMinutes = FMath::Max(1, BccCfg->ModBanDurationMinutes);

	const FString Reason = (Args.Num() > 1)
		? BanDiscordHelpers::JoinArgs(Args, 1)
		: TEXT("No reason given");

	FBanEntry Entry;
	Entry.Uid          = Uid;
	UBanDatabase::ParseUid(Uid, Entry.Platform, Entry.PlayerUID);
	Entry.PlayerName   = DisplayName;
	Entry.Reason       = Reason;
	Entry.BannedBy     = SenderName;
	Entry.BanDate      = FDateTime::UtcNow();
	Entry.bIsPermanent = false;
	Entry.ExpireDate   = FDateTime::UtcNow() + FTimespan::FromMinutes(DurationMinutes);

	if (!DB->AddBan(Entry))
	{
		Respond(ChannelId,
			TEXT("❌ Failed to write the ban to the database."));
		return;
	}

	if (UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr)
		UBanEnforcer::KickConnectedPlayer(World, Uid, Entry.GetKickMessage());

	// Also ban the counterpart identifier (IP↔EOS).
	const TArray<FString> ExtraUids = BanDiscordHelpers::AddCounterpartBans(
		this, DB, Uid, DisplayName, Entry.Reason, SenderName,
		Entry.bIsPermanent, Entry.ExpireDate);

	FString Msg = FString::Printf(
		TEXT("🔨 **%s** (`%s`) has been banned for **%d minute(s)** (mod action).\n"
		     "Expires: %s UTC\nReason: %s\nBanned by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, DurationMinutes,
		*Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")),
		*Reason, *SenderName);

	if (ExtraUids.Num() > 0)
		Msg += FString::Printf(TEXT("\nAlso banned: `%s`"),
			*FString::Join(ExtraUids, TEXT("`, `")));

	UE_LOG(LogBanDiscord, Log,
		TEXT("BanDiscordSubsystem: %s modban %s (%s) for %d min. Reason: %s"),
		*SenderName, *DisplayName, *Uid, DurationMinutes, *Reason);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod tempmute
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleTempMuteCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId,
                                                  const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			TEXT("Usage: `!tempmute <PUID|name> <minutes>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	int32 Minutes = 0;
	if (!FDefaultValueHelper::ParseInt(Args[1], Minutes) || Minutes <= 0)
	{
		Respond(ChannelId,
			TEXT("❌ `<minutes>` must be a positive integer."));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	MuteReg->MutePlayer(Uid, DisplayName, TEXT("Timed mute via Discord"), SenderName, Minutes);

	const FString Msg = FString::Printf(
		TEXT("🔇 Timed mute applied to **%s** (`%s`) for **%d minute(s)**.\nMuted by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, Minutes, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod mutecheck
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleMuteCheckCommand(const TArray<FString>& Args,
                                                   const FString& ChannelId)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/mod mutecheck <PUID|name>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	FMuteEntry Entry;
	if (!MuteReg->GetMuteEntry(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("🔊 **%s** (`%s`) is **not currently muted**."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	FString ExpiryStr;
	if (Entry.bIsIndefinite)
	{
		ExpiryStr = TEXT("**indefinitely**");
	}
	else
	{
		const FTimespan Remaining = Entry.ExpireDate - FDateTime::UtcNow();
		const int32 TotalMins = FMath::Max(0, static_cast<int32>(Remaining.GetTotalMinutes()));
		ExpiryStr = FString::Printf(TEXT("for **%dm** more (expires %s UTC)"),
			TotalMins,
			*Entry.ExpireDate.ToString(TEXT("%Y-%m-%d %H:%M:%S")));
	}

	Respond(ChannelId,
		FString::Printf(TEXT("🔇 **%s** (`%s`) is muted %s.\nReason: %s\nMuted by: %s"),
			*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid,
			*ExpiryStr, *Entry.Reason, *Entry.MutedBy));
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod mutelist
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleMuteListCommand(const FString& ChannelId)
{
	if (!CachedProvider) return;

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	TArray<FMuteEntry> Mutes = MuteReg->GetAllMutes();
	if (Mutes.IsEmpty())
	{
		Respond(ChannelId, TEXT("🔊 No players are currently muted."));
		return;
	}

	FString Body;
	Body.Reserve(1400);
	Body += TEXT("```\n");
	Body += FString::Printf(TEXT("%-18s  %-30s  %-18s  %s\n"),
		TEXT("Name"), TEXT("UID (truncated)"), TEXT("Expires"), TEXT("Reason"));
	Body += FString(TEXT("─"), 80) + TEXT("\n");

	const int32 ShowMax = FMath::Min(Mutes.Num(), 15);
	for (int32 i = 0; i < ShowMax; ++i)
	{
		const FMuteEntry& M = Mutes[i];
		const FString NameSh   = BanDiscordHelpers::Truncate(M.PlayerName, 18);
		const FString UidSh    = BanDiscordHelpers::Truncate(M.Uid, 30);
		const FString ExpirySh = M.bIsIndefinite
			? TEXT("permanent")
			: M.ExpireDate.ToString(TEXT("%m-%d %H:%M UTC"));
		const FString ReasonSh = BanDiscordHelpers::Truncate(M.Reason, 24);
		Body += FString::Printf(TEXT("%-18s  %-30s  %-18s  %s\n"),
			*NameSh, *UidSh, *ExpirySh, *ReasonSh);
	}
	Body += TEXT("```");

	FString Header = FString::Printf(TEXT("**Muted Players — %d total**\n"), Mutes.Num());
	if (Mutes.Num() > ShowMax)
		Header += FString::Printf(TEXT("_(Showing first %d)_\n"), ShowMax);

	FString Msg = Header + Body;
	if (Msg.Len() > 1990)
		Msg = Msg.Left(1940) + TEXT("\n...(truncated)```");
	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod tempunmute
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleTempUnmuteCommand(const TArray<FString>& Args,
                                                    const FString& ChannelId,
                                                    const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `!tempunmute <PUID|name>`"));
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	FMuteEntry Entry;
	if (!MuteReg->GetMuteEntry(Uid, Entry))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ **%s** (`%s`) is not currently muted."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	if (Entry.bIsIndefinite)
	{
		Respond(ChannelId,
			FString::Printf(
				TEXT("❌ **%s** has an indefinite mute, not a timed one. Use `/mod unmute` instead."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName)));
		return;
	}

	MuteReg->UnmutePlayer(Uid);

	const FString UnmuteMsg = FString::Printf(
		TEXT("🔊 Timed mute lifted from **%s** (`%s`) early.\nUnmuted by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *SenderName);
	Respond(ChannelId, UnmuteMsg);
	PostModerationLog(UnmuteMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod mutereason
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleMuteReasonCommand(const TArray<FString>& Args,
                                                    const FString& ChannelId,
                                                    const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.Num() < 2)
	{
		Respond(ChannelId,
			TEXT("Usage: `/mod mutereason <PUID|name> <new reason...>`");
		return;
	}

	FString Uid, DisplayName, ErrorMsg;
	if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
	{
		Respond(ChannelId, ErrorMsg);
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UMuteRegistry* MuteReg = GI ? GI->GetSubsystem<UMuteRegistry>() : nullptr;
	if (!MuteReg)
	{
		Respond(ChannelId,
			TEXT("❌ Mute commands require the BanChatCommands mod to be installed."));
		return;
	}

	const FString NewReason = BanDiscordHelpers::JoinArgs(Args, 1);

	if (!MuteReg->UpdateMuteReason(Uid, NewReason))
	{
		Respond(ChannelId,
			FString::Printf(TEXT("⚠️ **%s** (`%s`) is not currently muted."),
				*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid));
		return;
	}

	const FString Msg = FString::Printf(
		TEXT("✏️ Mute reason updated for **%s** (`%s`).\nNew reason: %s\nUpdated by: %s"),
		*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid, *NewReason, *SenderName);
	Respond(ChannelId, Msg);
	PostModerationLog(Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod stafflist
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleStaffListCommand(const FString& ChannelId)
{
	if (!CachedProvider) return;

	const UBanChatCommandsConfig* BccCfg = UBanChatCommandsConfig::Get();
	if (!BccCfg)
	{
		Respond(ChannelId,
			TEXT("❌ Staff list requires the BanChatCommands mod to be installed."));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		Respond(ChannelId,
			TEXT("❌ No active game world found."));
		return;
	}

	TArray<FString> Admins;
	TArray<FString> Mods;

	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		if (!PS) continue;

		FString PUIDStr;
		if (const FUniqueNetIdRepl& Id = PS->GetUniqueId(); Id.IsValid())
		{
			PUIDStr = Id.ToString();
			// Strip the "EOS:" prefix for the config check.
			FString Platform, RawId;
			UBanDatabase::ParseUid(PUIDStr, Platform, RawId);
			if (Platform == TEXT("EOS"))
				PUIDStr = RawId;
		}

		const FString CompoundUid = TEXT("EOS:") + PUIDStr.ToLower();
		const FString PlayerName  = PS->GetPlayerName();

		if (BccCfg->IsAdminUid(CompoundUid))
			Admins.Add(BanDiscordHelpers::EscapeMarkdown(PlayerName));
		else if (BccCfg->IsModeratorUid(CompoundUid))
			Mods.Add(BanDiscordHelpers::EscapeMarkdown(PlayerName));
	}

	if (Admins.IsEmpty() && Mods.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("📋 No staff are currently online."));
		return;
	}

	FString Msg = TEXT("📋 **Online Staff:**\n");
	if (!Admins.IsEmpty())
		Msg += FString::Printf(TEXT("🛡️ **Admins:** %s\n"), *FString::Join(Admins, TEXT(", ")));
	if (!Mods.IsEmpty())
		Msg += FString::Printf(TEXT("🔧 **Moderators:** %s\n"), *FString::Join(Mods, TEXT(", ")));

	Respond(ChannelId, Msg);
}

// ─────────────────────────────────────────────────────────────────────────────
// /mod staffchat
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleStaffChatCommand(const TArray<FString>& Args,
                                                   const FString& ChannelId,
                                                   const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/mod staffchat <message...>`");
		return;
	}

	const UBanChatCommandsConfig* BccCfg = UBanChatCommandsConfig::Get();
	if (!BccCfg)
	{
		Respond(ChannelId,
			TEXT("❌ Staff chat requires the BanChatCommands mod to be installed."));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UWorld* World = GI ? GI->GetWorld() : nullptr;
	if (!World)
	{
		Respond(ChannelId,
			TEXT("❌ No active game world found."));
		return;
	}

	const FString Message = BanDiscordHelpers::JoinArgs(Args, 0);
	const FString Formatted = FString::Printf(TEXT("[Discord Staff] %s: %s"), *SenderName, *Message);

	int32 DeliveredTo = 0;
	for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
	{
		APlayerController* PC = It->Get();
		if (!PC) continue;

		APlayerState* PS = PC->GetPlayerState<APlayerState>();
		if (!PS) continue;

		FString PUIDStr;
		if (const FUniqueNetIdRepl& Id = PS->GetUniqueId(); Id.IsValid())
		{
			PUIDStr = Id.ToString();
			FString Platform, RawId;
			UBanDatabase::ParseUid(PUIDStr, Platform, RawId);
			if (Platform == TEXT("EOS")) PUIDStr = RawId;
		}

		const FString CompoundUid = TEXT("EOS:") + PUIDStr.ToLower();
		if (BccCfg->IsModeratorUid(CompoundUid))
		{
			PC->ClientMessage(Formatted);
			++DeliveredTo;
		}
	}

	Respond(ChannelId,
		FString::Printf(TEXT("📨 Staff message delivered to **%d** online staff member(s)."), DeliveredTo));
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

// ─────────────────────────────────────────────────────────────────────────────
// /appeal list
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleAppealsCommand(const FString& ChannelId)
{
	if (!CachedProvider) return;

	UBanAppealRegistry* Registry =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UBanAppealRegistry>() : nullptr;

	if (!Registry)
	{
		Respond(ChannelId,
			TEXT(":x: BanAppealRegistry is not available (BanSystem may not be installed)."));
		return;
	}

	const TArray<FBanAppealEntry> Appeals = Registry->GetAllAppeals();

	if (Appeals.IsEmpty())
	{
		Respond(ChannelId,
			TEXT(":white_check_mark: No pending ban appeals."));
		return;
	}

	constexpr int32 MaxShow = 10;
	const int32 TotalCount  = Appeals.Num();
	const int32 ShowCount   = FMath::Min(TotalCount, MaxShow);

	FString Reply = FString::Printf(
		TEXT(":scales: **Pending Ban Appeals (%d):**\n"), TotalCount);

	for (int32 i = 0; i < ShowCount; ++i)
	{
		const FBanAppealEntry& A = Appeals[i];
		const FString DateStr = A.SubmittedAt.ToString(TEXT("%Y-%m-%d"));
		const FString Reason  = A.Reason.IsEmpty() ? TEXT("(none)") : A.Reason.Left(100);
		const FString Contact = A.ContactInfo.IsEmpty() ? TEXT("(none)") : A.ContactInfo.Left(80);

		Reply += FString::Printf(
			TEXT("`#%lld` uid=%s | contact: %s | submitted: %s | reason: %s\n"),
			A.Id, *A.Uid, *Contact, *DateStr, *Reason);
	}

	if (TotalCount > MaxShow)
	{
		Reply += FString::Printf(TEXT("*(+%d more)*"), TotalCount - MaxShow);
	}

	Respond(ChannelId, Reply.TrimEnd());
}

// ─────────────────────────────────────────────────────────────────────────────
// /appeal dismiss <id>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleDismissAppealCommand(const TArray<FString>& Args,
                                                       const FString& ChannelId,
                                                       const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `!dismissappeal <id>`"));
		return;
	}

	const int64 AppealId = FCString::Atoi64(*Args[0]);
	if (AppealId <= 0)
	{
		Respond(ChannelId,
			TEXT(":x: Invalid appeal ID. Must be a positive integer."));
		return;
	}

	UBanAppealRegistry* Registry =
		GetGameInstance() ? GetGameInstance()->GetSubsystem<UBanAppealRegistry>() : nullptr;

	if (!Registry)
	{
		Respond(ChannelId,
			TEXT(":x: BanAppealRegistry is not available (BanSystem may not be installed)."));
		return;
	}

	if (Registry->DeleteAppeal(AppealId))
	{
		UE_LOG(LogTemp, Log,
		       TEXT("BanDiscordSubsystem: Appeal #%lld dismissed by '%s'."),
		       AppealId, *SenderName);

		Respond(ChannelId,
			FString::Printf(TEXT(":white_check_mark: Appeal `#%lld` dismissed."), AppealId));
	}
	else
	{
		Respond(ChannelId,
			FString::Printf(TEXT(":x: No appeal found with ID `%lld`."), AppealId));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// /appeal approve <id>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleAppealApproveCommand(const TArray<FString>& Args,
                                                       const FString& ChannelId,
                                                       const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/appeal approve <id> [note...]`");
		return;
	}

	const int64 AppealId = FCString::Atoi64(*Args[0]);
	if (AppealId <= 0)
	{
		Respond(ChannelId,
			TEXT(":x: Invalid appeal ID. Must be a positive integer."));
		return;
	}

	FString ReviewNote;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		if (i > 1) ReviewNote += TEXT(" ");
		ReviewNote += Args[i];
	}

	UGameInstance* GI = GetGameInstance();
	UBanAppealRegistry* Registry = GI ? GI->GetSubsystem<UBanAppealRegistry>() : nullptr;
	if (!Registry)
	{
		Respond(ChannelId,
			TEXT(":x: BanAppealRegistry is not available (BanSystem may not be installed)."));
		return;
	}

	// Use the new ReviewAppeal workflow to record the decision with status/note.
	if (!Registry->ReviewAppeal(AppealId, EAppealStatus::Approved, SenderName, ReviewNote))
	{
		Respond(ChannelId,
			FString::Printf(TEXT(":x: No appeal found with ID `%lld`."), AppealId));
		return;
	}

	const FBanAppealEntry Entry = Registry->GetAppealById(AppealId);

	// Auto-unban the player on approval.
	UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
	bool bUnbanned = false;
	if (DB && !Entry.Uid.IsEmpty())
		bUnbanned = DB->RemoveBanByUid(Entry.Uid);

	const FString NoteStr = ReviewNote.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" Note: %s"), *ReviewNote);
	const FString Msg = bUnbanned
		? FString::Printf(TEXT(":white_check_mark: Appeal `#%lld` **approved** — ban for `%s` removed.%s"), AppealId, *Entry.Uid, *NoteStr)
		: FString::Printf(TEXT(":white_check_mark: Appeal `#%lld` **approved** — no active ban found for `%s`.%s"), AppealId, *Entry.Uid, *NoteStr);

	Respond(ChannelId, Msg);
	PostModerationLog(FString::Printf(TEXT("%s approved appeal #%lld (uid=%s)%s"), *SenderName, AppealId, *Entry.Uid, *NoteStr));

	FBanDiscordNotifier::NotifyAppealReviewed(Entry);

	UE_LOG(LogTemp, Log,
	       TEXT("BanDiscordSubsystem: Appeal #%lld approved by '%s' (uid=%s, unbanned=%s)."),
	       AppealId, *SenderName, *Entry.Uid, bUnbanned ? TEXT("yes") : TEXT("no"));
}

// ─────────────────────────────────────────────────────────────────────────────
// /appeal deny <id>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleAppealDenyCommand(const TArray<FString>& Args,
                                                    const FString& ChannelId,
                                                    const FString& SenderName)
{
	if (!CachedProvider) return;

	if (Args.IsEmpty())
	{
		Respond(ChannelId,
			TEXT("Usage: `/appeal deny <id> [note...]`");
		return;
	}

	const int64 AppealId = FCString::Atoi64(*Args[0]);
	if (AppealId <= 0)
	{
		Respond(ChannelId,
			TEXT(":x: Invalid appeal ID. Must be a positive integer."));
		return;
	}

	FString ReviewNote;
	for (int32 i = 1; i < Args.Num(); ++i)
	{
		if (i > 1) ReviewNote += TEXT(" ");
		ReviewNote += Args[i];
	}

	UGameInstance* GI = GetGameInstance();
	UBanAppealRegistry* Registry = GI ? GI->GetSubsystem<UBanAppealRegistry>() : nullptr;
	if (!Registry)
	{
		Respond(ChannelId,
			TEXT(":x: BanAppealRegistry is not available (BanSystem may not be installed)."));
		return;
	}

	// Use ReviewAppeal to record the denial with status/note.
	if (!Registry->ReviewAppeal(AppealId, EAppealStatus::Denied, SenderName, ReviewNote))
	{
		Respond(ChannelId,
			FString::Printf(TEXT(":x: No appeal found with ID `%lld`."), AppealId));
		return;
	}

	const FBanAppealEntry Entry = Registry->GetAppealById(AppealId);
	const FString NoteStr = ReviewNote.IsEmpty() ? TEXT("") : FString::Printf(TEXT(" Note: %s"), *ReviewNote);
	Respond(ChannelId,
		FString::Printf(TEXT(":x: Appeal `#%lld` **denied**.%s"), AppealId, *NoteStr));
	PostModerationLog(FString::Printf(TEXT("%s denied appeal #%lld (uid=%s)%s"), *SenderName, AppealId, *Entry.Uid, *NoteStr));

	FBanDiscordNotifier::NotifyAppealReviewed(Entry);

	UE_LOG(LogTemp, Log,
	       TEXT("BanDiscordSubsystem: Appeal #%lld denied by '%s'."), AppealId, *SenderName);
}

// ─────────────────────────────────────────────────────────────────────────────
// /player playtime
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandlePlaytimeCommand(const TArray<FString>& Args,
                                                  const FString& ChannelId)
{
if (!CachedProvider) return;

if (Args.IsEmpty())
{
Respond(ChannelId,
TEXT("Usage: `/player playtime <player|PUID>`");
return;
}

FString Uid, DisplayName, ErrorMsg;
if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
{
Respond(ChannelId,
FString::Printf(TEXT(":x: %s"), *ErrorMsg));
return;
}

UGameInstance* GI = GetGameInstance();
UPlayerSessionRegistry* Registry = GI ? GI->GetSubsystem<UPlayerSessionRegistry>() : nullptr;
if (!Registry)
{
Respond(ChannelId,
TEXT(":x: PlayerSessionRegistry is not available (BanSystem may not be installed)."));
return;
}

FPlayerSessionRecord Record;
const bool bFound = Registry->FindByUid(Uid, Record);

FString Reply;
if (!bFound || Record.LastSeen.IsEmpty())
{
Reply = FString::Printf(TEXT(":hourglass: No session record found for **%s** (`%s`)."),
*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid);
}
else
{
// Check if the player is currently online (present in the world).
bool bOnline = false;
if (UWorld* World = GI->GetWorld())
{
for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
{
APlayerController* PC = It->Get();
if (PC && PC->PlayerState && PC->PlayerState->GetPlayerName() == DisplayName)
{
bOnline = true;
break;
}
}
}

Reply = FString::Printf(
TEXT(":clock3: **%s** (`%s`)\n• **Last Seen:** %s UTC\n• **Status:** %s"),
*BanDiscordHelpers::EscapeMarkdown(DisplayName), *Uid,
*Record.LastSeen,
bOnline ? TEXT("🟢 Online") : TEXT("🔴 Offline"));
}

Respond(ChannelId, Reply);
}

// ─────────────────────────────────────────────────────────────────────────────
// /admin say  — Discord → game broadcast as [ADMIN]
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleSayCommand(const TArray<FString>& Args,
                                             const FString& ChannelId,
                                             const FString& SenderName)
{
if (!CachedProvider) return;

if (Args.IsEmpty())
{
Respond(ChannelId,
TEXT("Usage: `!say <message...>` — broadcasts as [ADMIN] in-game"));
return;
}

UGameInstance* GI = GetGameInstance();
UWorld* World = GI ? GI->GetWorld() : nullptr;
if (!World)
{
Respond(ChannelId,
TEXT(":x: No active game world found."));
return;
}

const FString Message  = BanDiscordHelpers::JoinArgs(Args, 0);
const FString Formatted = FString::Printf(TEXT("[ADMIN] %s: %s"), *SenderName, *Message);

int32 Delivered = 0;
for (FConstPlayerControllerIterator It = World->GetPlayerControllerIterator(); It; ++It)
{
if (APlayerController* PC = It->Get())
{
PC->ClientMessage(Formatted);
++Delivered;
}
}

Respond(ChannelId,
FString::Printf(TEXT("📢 Admin broadcast delivered to **%d** player(s): *%s*"),
Delivered, *BanDiscordHelpers::EscapeMarkdown(Message)));

PostModerationLog(FString::Printf(TEXT("%s broadcast [ADMIN]: %s"), *SenderName, *Message));
}

// ─────────────────────────────────────────────────────────────────────────────
// /admin poll  — create a Discord poll embed
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandlePollCommand(const TArray<FString>& Args,
                                              const FString& ChannelId)
{
if (!CachedProvider) return;

// Build full text by joining args then splitting on "|".
const FString FullText = BanDiscordHelpers::JoinArgs(Args, 0);
TArray<FString> Parts;
FullText.ParseIntoArray(Parts, TEXT("|"), true);
for (FString& P : Parts) P.TrimStartAndEndInline();

if (Parts.Num() < 3)
{
Respond(ChannelId,
TEXT("Usage: `!poll <question> | <optionA> | <optionB> [| <optionC> ...]`\n"
 "Example: `!poll Restart tonight? | Yes | No | Maybe`"));
return;
}

const FString Question = Parts[0];
const TArray<FString> Options(Parts.GetData() + 1, Parts.Num() - 1);

if (Options.Num() < 2 || Options.Num() > 10)
{
Respond(ChannelId,
TEXT(":x: A poll requires between 2 and 10 options."));
return;
}

// Number emoji labels for options.
static const TCHAR* NumEmoji[] = {
TEXT("1️⃣"), TEXT("2️⃣"), TEXT("3️⃣"), TEXT("4️⃣"), TEXT("5️⃣"),
TEXT("6️⃣"), TEXT("7️⃣"), TEXT("8️⃣"), TEXT("9️⃣"), TEXT("🔟")
};

FString FieldsJson;
for (int32 i = 0; i < Options.Num(); ++i)
{
if (i > 0) FieldsJson += TEXT(",");
const FString Label = FString::Printf(TEXT("%s %s"), NumEmoji[i],
*BanDiscordHelpers::Truncate(Options[i], 1024));
FieldsJson += FString::Printf(
TEXT("{\"name\":\"%s\",\"value\":\"React with %s to vote\",\"inline\":false}"),
*Label.Replace(TEXT("\""), TEXT("\\\"")),
NumEmoji[i]);
}

// Assemble embed payload.
const FString EmbedJson = FString::Printf(
TEXT("{\"embeds\":[{\"title\":\"📊 %s\",\"color\":5793266,\"fields\":[%s],"
 "\"footer\":{\"text\":\"React to vote! Results visible on the reactions.\"},"
 "\"timestamp\":\"%s\"}]}"),
*Question.Replace(TEXT("\""), TEXT("\\\"")),
*FieldsJson,
*FDateTime::UtcNow().ToIso8601());

// Send via the provider's body helper (bypasses text escaping).
Respond(ChannelId, EmbedJson);
}

// ─────────────────────────────────────────────────────────────────────────────
// Thread-per-player moderation log
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::PostToPlayerModerationThread(const FString& PlayerName,
                                                         const FString& Uid,
                                                         const FString& Message)
{
if (!CachedProvider) return;
if (Config.ModerationLogChannelId.IsEmpty()) return;
if (Config.ModerationLogChannelId == TEXT("0")) return;

// Check if we already have a cached thread ID for this player.
if (const FString* CachedId = PlayerThreadIdCache.Find(Uid))
{
// Post directly into the existing thread.
CachedProvider->SendDiscordChannelMessage(*CachedId, Message);
return;
}

// Derive the Config.BotToken from the provider's interface.
// We need to create a new thread via the Discord API.
// Build the thread name: "PlayerName (EOS:xxx)" truncated to 100 chars.
const FString ThreadName = BanDiscordHelpers::Truncate(
FString::Printf(TEXT("%s [%s]"), *PlayerName, *Uid), 100);

// POST /channels/{channel_id}/threads to create a public thread.
const FString Url = FString::Printf(
TEXT("https://discord.com/api/v10/channels/%s/threads"),
*Config.ModerationLogChannelId);

const FString BodyStr = FString::Printf(
TEXT("{\"name\":\"%s\",\"type\":11,\"auto_archive_duration\":10080}"),
*ThreadName.Replace(TEXT("\""), TEXT("\\\"")));

TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
FHttpModule::Get().CreateRequest();
Request->SetURL(Url);
Request->SetVerb(TEXT("POST"));
// We derive the token from the first connected provider config.
// Since we don't have direct access to the token here, we post a plain
// message to the log channel instead (falls back to flat log).
// Full thread creation is supported when BotToken is accessible.

// Fallback: post a prefixed message to the main mod-log channel.
const FString Prefixed = FString::Printf(
TEXT("**[%s]** %s"), *BanDiscordHelpers::EscapeMarkdown(ThreadName), *Message);
CachedProvider->SendDiscordChannelMessage(Config.ModerationLogChannelId, Prefixed);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban schedule <player|PUID> <delay> [banDuration] [reason...]
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleScheduleBanCommand(const TArray<FString>& Args,
                                                     const FString& ChannelId,
                                                     const FString& SenderName)
{
if (!CachedProvider) return;

if (Args.Num() < 2)
{
Respond(ChannelId,
TEXT("Usage: `!scheduleban <player|PUID> <delay> [banDuration] [reason...]`\n"
     "Example: `!scheduleban BadPlayer 2h 1d Griefing`"));
return;
}

UGameInstance* GI = GetGameInstance();
UScheduledBanRegistry* SchReg = GI ? GI->GetSubsystem<UScheduledBanRegistry>() : nullptr;
if (!SchReg)
{
Respond(ChannelId,
TEXT(":x: ScheduledBanRegistry unavailable."));
return;
}

FString Uid, DisplayName, ErrorMsg;
if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
{
Uid         = UBanDatabase::MakeUid(TEXT("EOS"), Args[0].ToLower());
DisplayName = Args[0];
}

// Parse delay.
const int32 DelayMinutes = UBanDiscordSubsystem::ParseDurationMinutes(Args[1]);
if (DelayMinutes <= 0)
{
Respond(ChannelId,
FString::Printf(TEXT(":x: Invalid delay `%s`. Use format like `30m`, `2h`, `1d`."), *Args[1]));
return;
}

// Optional ban duration.
int32 BanDurationMinutes = 0;
int32 ReasonIdx = 2;
if (Args.Num() > 2)
{
const int32 Parsed = UBanDiscordSubsystem::ParseDurationMinutes(Args[2]);
if (Parsed > 0) { BanDurationMinutes = Parsed; ReasonIdx = 3; }
else if (Args[2] == TEXT("perm") || Args[2] == TEXT("permanent")) { BanDurationMinutes = 0; ReasonIdx = 3; }
}

FString Reason = TEXT("Scheduled ban");
if (ReasonIdx < Args.Num())
{
Reason.Empty();
for (int32 i = ReasonIdx; i < Args.Num(); ++i)
{
if (i > ReasonIdx) Reason += TEXT(" ");
Reason += Args[i];
}
}

const FDateTime EffectiveAt = FDateTime::UtcNow() + FTimespan::FromMinutes(DelayMinutes);
FScheduledBanEntry Entry = SchReg->AddScheduled(Uid, DisplayName, Reason, SenderName, EffectiveAt, BanDurationMinutes);

const FString DurStr = BanDurationMinutes == 0 ? TEXT("permanent") : FString::Printf(TEXT("%d min"), BanDurationMinutes);
Respond(ChannelId,
FString::Printf(TEXT(":calendar: Scheduled ban **#%lld** for `%s` in **%d min** (effective %s). Duration: %s. Reason: %s"),
Entry.Id, *DisplayName, DelayMinutes, *EffectiveAt.ToString(TEXT("%Y-%m-%d %H:%M:%S")), *DurStr, *Reason));
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban quick <templateSlug> <player|PUID>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleQBanCommand(const TArray<FString>& Args,
                                              const FString& ChannelId,
                                              const FString& SenderName)
{
if (!CachedProvider) return;

const UBanSystemConfig* SysCfg = UBanSystemConfig::Get();
if (!SysCfg || SysCfg->BanTemplates.IsEmpty())
{
Respond(ChannelId,
TEXT(":x: No ban templates configured (`BanTemplates=` in DefaultBanSystem.ini)."));
return;
}

// Parse pipe-delimited template strings into FBanTemplate structs.
const TArray<FBanTemplate> Templates = FBanTemplate::ParseTemplates(SysCfg->BanTemplates);

if (Args.IsEmpty())
{
FString List = TEXT("**Available ban templates:**\n");
for (const FBanTemplate& T : Templates)
{
const FString DurStr = T.DurationMinutes == 0 ? TEXT("permanent") : FString::Printf(TEXT("%dmin"), T.DurationMinutes);
List += FString::Printf(TEXT("`%s` — %s — %s\n"), *T.Slug, *DurStr, *T.Reason);
}
Respond(ChannelId, List);
return;
}

if (Args.Num() < 2)
{
Respond(ChannelId,
TEXT("Usage: `!qban <templateSlug> <player|PUID>`"));
return;
}

const FString Slug = Args[0].ToLower();
const FBanTemplate* Template = nullptr;
for (const FBanTemplate& T : Templates)
{
if (T.Slug.ToLower() == Slug) { Template = &T; break; }
}
if (!Template)
{
Respond(ChannelId,
FString::Printf(TEXT(":x: Unknown template `%s`. Use `!qban` to list templates."), *Slug));
return;
}

FString Uid, DisplayName, ErrorMsg;
if (!ResolveTarget(Args[1], Uid, DisplayName, ErrorMsg))
{
Uid         = UBanDatabase::MakeUid(TEXT("EOS"), Args[1].ToLower());
DisplayName = Args[1];
}

UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
if (!DB)
{
Respond(ChannelId, TEXT(":x: Database unavailable."));
return;
}

FBanEntry Ban;
Ban.Uid        = Uid;
UBanDatabase::ParseUid(Uid, Ban.Platform, Ban.PlayerUID);
Ban.PlayerName      = DisplayName;
Ban.Reason          = Template->Reason;
Ban.BannedBy        = SenderName;
Ban.BanDate         = FDateTime::UtcNow();
Ban.Category        = Template->Category;
Ban.bIsPermanent    = (Template->DurationMinutes <= 0);
Ban.ExpireDate      = Ban.bIsPermanent
? FDateTime(0)
: FDateTime::UtcNow() + FTimespan::FromMinutes(Template->DurationMinutes);

if (!DB->AddBan(Ban))
{
Respond(ChannelId, TEXT(":x: Failed to apply ban."));
return;
}

UGameInstance* GI = GetGameInstance();
if (UWorld* W = GI ? GI->GetWorld() : nullptr)
UBanEnforcer::KickConnectedPlayer(W, Uid, Ban.GetKickMessage());

// Also ban counterpart identifiers (IP↔EOS).
BanDiscordHelpers::AddCounterpartBans(this, DB, Uid, DisplayName,
	Template->Reason, SenderName, Ban.bIsPermanent, Ban.ExpireDate);

FBanDiscordNotifier::NotifyBanCreated(Ban);

const FString DurStr = Ban.bIsPermanent ? TEXT("permanent") : FString::Printf(TEXT("%dmin"), Template->DurationMinutes);
Respond(ChannelId,
FString::Printf(TEXT(":hammer: [%s] Banned **%s** (%s). Reason: %s. Duration: %s."),
*Slug, *DisplayName, *Uid, *Template->Reason, *DurStr));
}

// ─────────────────────────────────────────────────────────────────────────────
// /player reputation <player|PUID>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleReputationCommand(const TArray<FString>& Args,
                                                    const FString& ChannelId)
{
if (!CachedProvider) return;

if (Args.IsEmpty())
{
Respond(ChannelId,
TEXT("Usage: `!reputation <player|PUID>`"));
return;
}

FString Uid, DisplayName, ErrorMsg;
if (!ResolveTarget(Args[0], Uid, DisplayName, ErrorMsg))
{
Uid         = UBanDatabase::MakeUid(TEXT("EOS"), Args[0].ToLower());
DisplayName = Args[0];
}

UGameInstance* GI = GetGameInstance();
UBanDatabase*           DB        = GI ? GI->GetSubsystem<UBanDatabase>() : nullptr;
UPlayerWarningRegistry* WarnReg   = GI ? GI->GetSubsystem<UPlayerWarningRegistry>() : nullptr;
UPlayerSessionRegistry* SessionReg= GI ? GI->GetSubsystem<UPlayerSessionRegistry>() : nullptr;
UBanAuditLog*           AuditLog  = GI ? GI->GetSubsystem<UBanAuditLog>() : nullptr;

const int32 WarnCount  = WarnReg  ? WarnReg->GetWarningCount(Uid)  : 0;
const int32 WarnPoints = WarnReg  ? WarnReg->GetWarningPoints(Uid) : 0;

int32 TotalBans = 0;
bool  bCurrentlyBanned = false;
if (DB)
{
for (const FBanEntry& B : DB->GetAllBans())
{
if (B.Uid.Equals(Uid, ESearchCase::IgnoreCase))
{
++TotalBans;
if (!B.IsExpired()) bCurrentlyBanned = true;
}
}
}

int32 KickCount = 0;
if (AuditLog)
{
for (const FAuditEntry& E : AuditLog->GetEntriesForTarget(Uid))
{
if (E.Action == TEXT("kick")) ++KickCount;
}
}

FString LastSeen = TEXT("unknown");
if (SessionReg)
{
FPlayerSessionRecord Rec;
if (SessionReg->FindByUid(Uid, Rec))
{
LastSeen    = Rec.LastSeen;
if (!Rec.DisplayName.IsEmpty()) DisplayName = Rec.DisplayName;
}
}

const int32 Score = FMath::Max(0,
100 - (WarnPoints * 5) - (TotalBans * 15) - (KickCount * 3));

const int32 Color = Score >= 70 ? 3066993 : (Score >= 40 ? 16776960 : 15158332);

const FString Fields = FString::Printf(
TEXT("{\"name\":\"Score\",\"value\":\"%d/100\",\"inline\":true},"
     "{\"name\":\"Currently Banned\",\"value\":\"%s\",\"inline\":true},"
     "{\"name\":\"Warnings\",\"value\":\"%d (pts: %d)\",\"inline\":true},"
     "{\"name\":\"Total Bans\",\"value\":\"%d\",\"inline\":true},"
     "{\"name\":\"Kicks\",\"value\":\"%d\",\"inline\":true},"
     "{\"name\":\"Last Seen\",\"value\":\"%s\",\"inline\":false}"),
Score,
bCurrentlyBanned ? TEXT("YES") : TEXT("No"),
WarnCount, WarnPoints, TotalBans, KickCount,
*BanDiscordHelpers::EscapeMarkdown(LastSeen));

const FString EmbedJson = FString::Printf(
TEXT("{\"embeds\":[{\"title\":\"🔍 Reputation: %s\",\"description\":\"`%s`\",\"color\":%d,\"fields\":[%s],\"timestamp\":\"%s\"}]}"),
*BanDiscordHelpers::EscapeMarkdown(DisplayName),
*BanDiscordHelpers::EscapeMarkdown(Uid),
Color,
*Fields,
*FDateTime::UtcNow().ToIso8601());

Respond(ChannelId, EmbedJson);
}

// ─────────────────────────────────────────────────────────────────────────────
// /ban bulk <PUID1> <PUID2> ... -- <reason>
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBulkBanCommand(const TArray<FString>& Args,
                                                 const FString& ChannelId,
                                                 const FString& SenderName)
{
if (!CachedProvider) return;

if (Args.IsEmpty())
{
Respond(ChannelId,
TEXT("Usage: `!bulkban <PUID1> <PUID2> ... -- <reason>`"));
return;
}

UBanDatabase* DB = BanDiscordHelpers::GetDB(this);
if (!DB)
{
Respond(ChannelId, TEXT(":x: Database unavailable."));
return;
}

// Split on "--".
TArray<FString> Uids;
FString Reason = TEXT("Bulk ban");
int32 SepIdx = -1;
for (int32 i = 0; i < Args.Num(); ++i)
{
if (Args[i] == TEXT("--")) { SepIdx = i; break; }
}

if (SepIdx > 0 && SepIdx < Args.Num() - 1)
{
for (int32 i = 0; i < SepIdx; ++i)
Uids.Add(Args[i]);

Reason.Empty();
for (int32 i = SepIdx + 1; i < Args.Num(); ++i)
{
if (i > SepIdx + 1) Reason += TEXT(" ");
Reason += Args[i];
}
}
else
{
Uids = Args;
}

if (Uids.IsEmpty())
{
Respond(ChannelId,
TEXT(":x: No UIDs provided."));
return;
}

UGameInstance* GI = GetGameInstance();
int32 BannedCount = 0;

for (const FString& RawUid : Uids)
{
const FString Uid = UBanDatabase::MakeUid(TEXT("EOS"), RawUid.ToLower());

FBanEntry Ban;
Ban.Uid        = Uid;
UBanDatabase::ParseUid(Uid, Ban.Platform, Ban.PlayerUID);
Ban.PlayerName      = RawUid;
Ban.Reason          = Reason;
Ban.BannedBy        = SenderName;
Ban.BanDate         = FDateTime::UtcNow();
Ban.bIsPermanent    = true;
Ban.ExpireDate      = FDateTime(0);

if (DB->AddBan(Ban))
{
if (UWorld* W = GI ? GI->GetWorld() : nullptr)
UBanEnforcer::KickConnectedPlayer(W, Uid, Ban.GetKickMessage());
FBanDiscordNotifier::NotifyBanCreated(Ban);
// Also ban counterpart identifiers (IP↔EOS).
BanDiscordHelpers::AddCounterpartBans(this, DB, Uid, RawUid,
	Reason, SenderName, true, FDateTime(0));
++BannedCount;
}
}

Respond(ChannelId,
FString::Printf(TEXT(":hammer: Bulk ban complete: **%d/%d** players banned. Reason: %s"),
BannedCount, Uids.Num(), *Reason));
PostModerationLog(FString::Printf(TEXT("%s bulk-banned %d player(s). Reason: %s"), *SenderName, BannedCount, *Reason));
}

// ─────────────────────────────────────────────────────────────────────────────
// Slash command interaction handler
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::OnDiscordInteraction(const TSharedPtr<FJsonObject>& InteractionObj)
{
if (!InteractionObj.IsValid() || !CachedProvider)
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: OnDiscordInteraction skipped — "
            "InteractionObj is %s, CachedProvider is %s."),
       InteractionObj.IsValid() ? TEXT("valid") : TEXT("null"),
       CachedProvider ? TEXT("set") : TEXT("null"));
return;
}

// Only handle APPLICATION_COMMAND interactions (type 2).
int32 InteractionType = 0;
InteractionObj->TryGetNumberField(TEXT("type"), InteractionType);
if (InteractionType != 2) return;

// Extract the interaction token and set up a guard that clears
// PendingInteractionToken when OnDiscordInteraction returns, ensuring
// the member field is never stale between calls.
{
FString ExtractedToken;
InteractionObj->TryGetStringField(TEXT("token"), ExtractedToken);
PendingInteractionToken = ExtractedToken;
}

// RAII guard: clears PendingInteractionToken on any return path.
struct FInteractionTokenGuard
{
    FString& TokenRef;
    explicit FInteractionTokenGuard(FString& T) : TokenRef(T) {}
    ~FInteractionTokenGuard() { TokenRef = FString(); }
} TokenGuard(PendingInteractionToken);

// Extract channel_id early so error messages can be posted.
FString ChannelId;
InteractionObj->TryGetStringField(TEXT("channel_id"), ChannelId);

// Commands are disabled when neither role is configured.
if (Config.AdminRoleId.IsEmpty() && Config.ModeratorRoleId.IsEmpty())
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: Slash command ignored — "
            "neither AdminRoleId nor ModeratorRoleId is set in DefaultBanBridge.ini."));
if (!ChannelId.IsEmpty())
{
Respond(ChannelId,
    TEXT("❌ Ban commands are disabled — `AdminRoleId` is not configured.\n"
         "Set `AdminRoleId` in `DefaultBanBridge.ini` and restart the server."));
}
return;
}

// Extract command data.
const TSharedPtr<FJsonObject>* CmdDataPtr = nullptr;
if (!InteractionObj->TryGetObjectField(TEXT("data"), CmdDataPtr) || !CmdDataPtr)
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: Slash command ignored — "
            "interaction payload has no 'data' field."));
return;
}

FString CmdGroupName;
(*CmdDataPtr)->TryGetStringField(TEXT("name"), CmdGroupName);
CmdGroupName = CmdGroupName.ToLower();

// Only handle groups that belong to BanDiscordSubsystem.
static const TArray<FString> HandledGroups = {
TEXT("ban"), TEXT("warn"), TEXT("mod"),
TEXT("player"), TEXT("appeal"), TEXT("admin"),
};
if (!HandledGroups.Contains(CmdGroupName)) return;

// Extract the subcommand and its option list.
const TArray<TSharedPtr<FJsonValue>>* TopOpts = nullptr;
(*CmdDataPtr)->TryGetArrayField(TEXT("options"), TopOpts);
if (!TopOpts || TopOpts->IsEmpty())
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: /%s interaction has no subcommand options."),
       *CmdGroupName);
if (!ChannelId.IsEmpty())
Respond(ChannelId,
    TEXT("❌ Could not parse the subcommand. Please try the command again."));
return;
}

const TSharedPtr<FJsonObject>* SubCmdPtr = nullptr;
if (!(*TopOpts)[0]->TryGetObject(SubCmdPtr) || !SubCmdPtr)
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: /%s interaction subcommand object is malformed."),
       *CmdGroupName);
if (!ChannelId.IsEmpty())
Respond(ChannelId,
    TEXT("❌ Could not parse the subcommand. Please try the command again."));
return;
}

FString SubCmdName;
(*SubCmdPtr)->TryGetStringField(TEXT("name"), SubCmdName);
SubCmdName = SubCmdName.ToLower();

const TArray<TSharedPtr<FJsonValue>>* SubOpts = nullptr;
(*SubCmdPtr)->TryGetArrayField(TEXT("options"), SubOpts);

// Extract sender identity, channel, and roles.
FString SenderName, AuthorId;
TArray<FString> MemberRoles;

const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
if (InteractionObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
{
(*MemberPtr)->TryGetStringField(TEXT("nick"), SenderName);
const TSharedPtr<FJsonObject>* UserPtr = nullptr;
if ((*MemberPtr)->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
{
(*UserPtr)->TryGetStringField(TEXT("id"), AuthorId);
if (SenderName.IsEmpty())
(*UserPtr)->TryGetStringField(TEXT("global_name"), SenderName);
if (SenderName.IsEmpty())
(*UserPtr)->TryGetStringField(TEXT("username"), SenderName);
}
const TArray<TSharedPtr<FJsonValue>>* RolesArr = nullptr;
if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), RolesArr) && RolesArr)
{
for (const TSharedPtr<FJsonValue>& RV : *RolesArr)
{
FString RId;
if (RV->TryGetString(RId)) MemberRoles.Add(RId);
}
}
}
if (SenderName.IsEmpty()) SenderName = TEXT("Discord Admin");
if (ChannelId.IsEmpty())
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: /%s %s — channel_id is empty, cannot respond."),
       *CmdGroupName, *SubCmdName);
return;
}

// Helper: extract a named option value as a string (handles STRING and INTEGER types).
auto GetOpt = [&](const FString& Name) -> FString
{
if (!SubOpts) return FString();
for (const TSharedPtr<FJsonValue>& Opt : *SubOpts)
{
const TSharedPtr<FJsonObject>* ObjPtr = nullptr;
if (!Opt->TryGetObject(ObjPtr) || !ObjPtr) continue;
FString OptName;
if (!(*ObjPtr)->TryGetStringField(TEXT("name"), OptName) ||
    !OptName.Equals(Name, ESearchCase::IgnoreCase)) continue;
FString StrVal;
if ((*ObjPtr)->TryGetStringField(TEXT("value"), StrVal)) return StrVal;
double NumVal = 0.0;
if ((*ObjPtr)->TryGetNumberField(TEXT("value"), NumVal))
return FString::Printf(TEXT("%lld"), static_cast<int64>(NumVal));
return FString();
}
return FString();
};

// Determine required permission level.
// /mod subcommands are accessible to moderators; all others require admin.
static const TArray<FString> ModOnlyCmds = {
TEXT("kick"), TEXT("modban"), TEXT("mute"), TEXT("unmute"),
TEXT("tempmute"), TEXT("tempunmute"), TEXT("mutecheck"), TEXT("mutelist"),
TEXT("mutereason"), TEXT("announce"), TEXT("stafflist"), TEXT("staffchat"),
};
const bool bNeedsAdminOnly = (CmdGroupName != TEXT("mod")) || !ModOnlyCmds.Contains(SubCmdName);
const bool bAuthorised     = bNeedsAdminOnly
? IsAdminMember(MemberRoles)
: IsModeratorMember(MemberRoles);

if (!bAuthorised)
{
Respond(ChannelId,
bNeedsAdminOnly
? TEXT("❌ Admin role required for that command.")
: TEXT("❌ Moderator role required for that command."));
return;
}

// ── Route to the appropriate command handler ──────────────────────────────
TArray<FString> Args;
bool bHandled = true;

if (CmdGroupName == TEXT("ban"))
{
if (SubCmdName == TEXT("add"))
{
Args.Add(GetOpt(TEXT("player")));
const FString R = GetOpt(TEXT("reason")); if (!R.IsEmpty()) Args.Add(R);
HandleBanCommand(Args, ChannelId, SenderName, false);
}
else if (SubCmdName == TEXT("temp"))
{
// Convert duration string (e.g. "2h") to minutes.
const FString Player = GetOpt(TEXT("player"));
const FString DurStr = GetOpt(TEXT("duration"));
const int32 Minutes  = ParseDurationMinutes(DurStr);
const FString Reason = GetOpt(TEXT("reason"));
Args.Add(Player);
Args.Add(FString::FromInt(FMath::Max(1, Minutes)));
if (!Reason.IsEmpty()) Args.Add(Reason);
HandleBanCommand(Args, ChannelId, SenderName, true);
}
else if (SubCmdName == TEXT("remove"))
{
Args.Add(GetOpt(TEXT("uid")));
HandleUnbanCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("removename"))
{
Args.Add(GetOpt(TEXT("name")));
HandleUnbanNameCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("byname"))
{
Args.Add(GetOpt(TEXT("name")));
const FString R = GetOpt(TEXT("reason")); if (!R.IsEmpty()) Args.Add(R);
HandleBanNameCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("check"))
{
Args.Add(GetOpt(TEXT("player")));
HandleBanCheckCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("reason"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("new_reason")));
HandleBanReasonCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("list"))
{
const FString P = GetOpt(TEXT("page")); if (!P.IsEmpty()) Args.Add(P);
HandleBanListCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("extend"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("duration")));
HandleExtendBanCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("duration"))
{
Args.Add(GetOpt(TEXT("player")));
HandleDurationCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("link"))
{
Args.Add(GetOpt(TEXT("uid1")));
Args.Add(GetOpt(TEXT("uid2")));
HandleLinkBansCommand(Args, ChannelId, SenderName, true);
}
else if (SubCmdName == TEXT("unlink"))
{
Args.Add(GetOpt(TEXT("uid1")));
Args.Add(GetOpt(TEXT("uid2")));
HandleLinkBansCommand(Args, ChannelId, SenderName, false);
}
else if (SubCmdName == TEXT("schedule"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("delay")));
const FString BD = GetOpt(TEXT("ban_duration")); if (!BD.IsEmpty()) Args.Add(BD);
const FString R  = GetOpt(TEXT("reason"));       if (!R.IsEmpty())  Args.Add(R);
HandleScheduleBanCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("quick"))
{
Args.Add(GetOpt(TEXT("template")));
Args.Add(GetOpt(TEXT("player")));
HandleQBanCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("bulk"))
{
// Split the space-separated player list into individual UID args.
const FString PlayersStr = GetOpt(TEXT("players"));
const FString Reason     = GetOpt(TEXT("reason"));
TArray<FString> PlayerTokens;
PlayersStr.ParseIntoArrayWS(PlayerTokens);
Args.Append(PlayerTokens);
Args.Add(TEXT("--"));
if (!Reason.IsEmpty()) Args.Add(Reason);
HandleBulkBanCommand(Args, ChannelId, SenderName);
}
else { bHandled = false; }
}
else if (CmdGroupName == TEXT("warn"))
{
if (SubCmdName == TEXT("add"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("reason")));
HandleWarnCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("list"))
{
Args.Add(GetOpt(TEXT("player")));
HandleWarningsCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("clearall"))
{
Args.Add(GetOpt(TEXT("player")));
HandleClearWarnsCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("clearone"))
{
Args.Add(GetOpt(TEXT("warning_id")));
HandleClearWarnByIdCommand(Args, ChannelId, SenderName);
}
else { bHandled = false; }
}
else if (CmdGroupName == TEXT("mod"))
{
if (SubCmdName == TEXT("kick"))
{
Args.Add(GetOpt(TEXT("player")));
const FString R = GetOpt(TEXT("reason")); if (!R.IsEmpty()) Args.Add(R);
HandleKickCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("modban"))
{
Args.Add(GetOpt(TEXT("player")));
const FString R = GetOpt(TEXT("reason")); if (!R.IsEmpty()) Args.Add(R);
HandleModBanCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("mute"))
{
Args.Add(GetOpt(TEXT("player")));
const FString Mins = GetOpt(TEXT("minutes")); if (!Mins.IsEmpty()) Args.Add(Mins);
const FString R    = GetOpt(TEXT("reason"));  if (!R.IsEmpty())    Args.Add(R);
HandleMuteCommand(Args, ChannelId, SenderName, true);
}
else if (SubCmdName == TEXT("unmute"))
{
Args.Add(GetOpt(TEXT("player")));
HandleMuteCommand(Args, ChannelId, SenderName, false);
}
else if (SubCmdName == TEXT("tempmute"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("minutes")));
HandleTempMuteCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("tempunmute"))
{
Args.Add(GetOpt(TEXT("player")));
HandleTempUnmuteCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("mutecheck"))
{
Args.Add(GetOpt(TEXT("player")));
HandleMuteCheckCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("mutelist"))
{
HandleMuteListCommand(ChannelId);
}
else if (SubCmdName == TEXT("mutereason"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("new_reason")));
HandleMuteReasonCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("announce"))
{
Args.Add(GetOpt(TEXT("message")));
HandleAnnounceCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("stafflist"))
{
HandleStaffListCommand(ChannelId);
}
else if (SubCmdName == TEXT("staffchat"))
{
Args.Add(GetOpt(TEXT("message")));
HandleStaffChatCommand(Args, ChannelId, SenderName);
}
else { bHandled = false; }
}
else if (CmdGroupName == TEXT("player"))
{
// Note: /player stats is handled by DiscordBridgeSubsystem (not BanDiscordSubsystem).
if (SubCmdName == TEXT("history"))
{
Args.Add(GetOpt(TEXT("query")));
HandlePlayerHistoryCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("note"))
{
Args.Add(GetOpt(TEXT("player")));
Args.Add(GetOpt(TEXT("text"))); // JoinArgs(Args, 1) in handler reassembles the text
HandleNoteCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("notes"))
{
Args.Add(GetOpt(TEXT("player")));
HandleNotesCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("reason"))
{
Args.Add(GetOpt(TEXT("uid")));
HandleReasonCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("playtime"))
{
Args.Add(GetOpt(TEXT("player")));
HandlePlaytimeCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("reputation"))
{
Args.Add(GetOpt(TEXT("player")));
HandleReputationCommand(Args, ChannelId);
}
// "stats" is intentionally absent — handled in DiscordBridgeSubsystem.
else { bHandled = false; }
}
else if (CmdGroupName == TEXT("appeal"))
{
if (SubCmdName == TEXT("list"))
{
HandleAppealsCommand(ChannelId);
}
else if (SubCmdName == TEXT("dismiss"))
{
Args.Add(GetOpt(TEXT("id")));
HandleDismissAppealCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("approve"))
{
Args.Add(GetOpt(TEXT("id")));
HandleAppealApproveCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("deny"))
{
Args.Add(GetOpt(TEXT("id")));
HandleAppealDenyCommand(Args, ChannelId, SenderName);
}
else { bHandled = false; }
}
else if (CmdGroupName == TEXT("admin"))
{
if (SubCmdName == TEXT("say"))
{
Args.Add(GetOpt(TEXT("message")));
HandleSayCommand(Args, ChannelId, SenderName);
}
else if (SubCmdName == TEXT("poll"))
{
// Reconstruct the pipe-delimited string that HandlePollCommand expects
// (JoinArgs(Args, 0) then split on "|").
const FString Question = GetOpt(TEXT("question"));
const FString Options  = GetOpt(TEXT("options")); // "Yes|No|Maybe"
TArray<FString> OptTokens;
Options.ParseIntoArray(OptTokens, TEXT("|"), true);
FString PollText = Question;
for (const FString& O : OptTokens)
PollText += TEXT(" | ") + O.TrimStartAndEnd();
Args.Add(PollText);
HandlePollCommand(Args, ChannelId);
}
else if (SubCmdName == TEXT("reloadconfig"))
{
HandleReloadConfigCommand(ChannelId, SenderName);
}
else { bHandled = false; }
}
else { bHandled = false; }

if (!bHandled)
{
UE_LOG(LogBanDiscord, Warning,
       TEXT("BanDiscordSubsystem: Unrecognised slash command /%s %s — no handler matched."),
       *CmdGroupName, *SubCmdName);
Respond(ChannelId,
    FString::Printf(TEXT("❌ Unknown subcommand `/%s %s`."), *CmdGroupName, *SubCmdName));
}
}
