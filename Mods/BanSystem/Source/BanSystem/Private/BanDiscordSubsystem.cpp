// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordSubsystem.h"
#include "BanPlayerLookup.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanDiscord, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem initialised."));
}

void UBanDiscordSubsystem::Deinitialize()
{
	Provider = nullptr;
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Provider registration
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::SetProvider(IBanDiscordCommandProvider* InProvider)
{
	Provider = InProvider;
	if (Provider)
	{
		UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: Discord command provider registered."));
	}
	else
	{
		UE_LOG(LogBanDiscord, Log, TEXT("BanDiscordSubsystem: Discord command provider cleared."));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Main command dispatcher
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleDiscordCommand(const FString& SubCommand,
                                                 const FString& AdminName,
                                                 const FString& ChannelId)
{
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: command from '%s': '%s'"), *AdminName, *SubCommand);

	FString Verb, Arg;
	if (!SubCommand.TrimStartAndEnd().Split(TEXT(" "), &Verb, &Arg, ESearchCase::IgnoreCase))
	{
		Verb = SubCommand.TrimStartAndEnd();
		Arg  = TEXT("");
	}
	Verb = Verb.TrimStartAndEnd().ToLower();
	Arg  = Arg.TrimStartAndEnd();

	if (Verb == TEXT("add"))
	{
		HandleBanAdd(Arg, AdminName, ChannelId);
	}
	else if (Verb == TEXT("remove"))
	{
		HandleBanRemove(Arg, AdminName, ChannelId);
	}
	else if (Verb == TEXT("list"))
	{
		HandleBanList(ChannelId);
	}
	else if (Verb == TEXT("status"))
	{
		HandleBanStatus(ChannelId);
	}
	else if (Verb == TEXT("role"))
	{
		HandleBanRole(Arg, ChannelId);
	}
	else
	{
		Reply(ChannelId,
		      TEXT(":question: Unknown ban command.\n"
		           "**Available commands:**\n"
		           "• `!ban add <PlayerName> [duration_minutes] [reason]` — ban a connected player\n"
		           "• `!ban remove <PlayerName>` — unban an online player by name\n"
		           "• `!ban list` — show all active Steam + EOS bans\n"
		           "• `!ban status` — show ban count summary\n"
		           "• `!ban role add <discord_user_id>` — grant ban-admin role\n"
		           "• `!ban role remove <discord_user_id>` — revoke ban-admin role\n"
		           "\n"
		           ":information_source: For offline unban use `/steamunban <id>` or `/eosunban <id>` in-game."));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban add
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanAdd(const FString& Arg,
                                         const FString& AdminName,
                                         const FString& ChannelId)
{
	FString PlayerNameQuery;
	int32   DurationMinutes = 0;
	FString Reason;
	ParseNameDurationReason(Arg, PlayerNameQuery, DurationMinutes, Reason);

	if (PlayerNameQuery.IsEmpty())
	{
		Reply(ChannelId, TEXT(":warning: Usage: `!ban add <PlayerName> [duration_minutes] [reason...]`\n"
		                      "Example: `!ban add SomePlayer 60 Spamming`\n"
		                      "Use duration `0` (or omit) for a permanent ban."));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: No world context available. Try again after the server has fully loaded."));
		return;
	}

	FResolvedBanId  Ids;
	FString         ExactName;
	TArray<FString> Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, PlayerNameQuery, Ids, ExactName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":warning: Multiple online players match **%s**: %s\n"
			               "Please use a more specific name."),
			          *PlayerNameQuery, *FString::Join(Ambiguous, TEXT(", "))));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":x: No online player found matching **%s**.\n"
			               "The player must be connected to ban by name. "
			               "For offline bans use `/steamban <id>` or `/eosban <id>` in-game."),
			          *PlayerNameQuery));
		}
		return;
	}

	const FString DurStr = (DurationMinutes > 0)
	                           ? FString::Printf(TEXT("for %d minute(s)"), DurationMinutes)
	                           : TEXT("permanently");
	int32 BanCount = 0;
	UGameInstance* GI = GetGameInstance();

	if (Ids.HasSteamId())
	{
		if (USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr)
		{
			SteamBans->BanPlayer(Ids.Steam64Id, Reason, DurationMinutes, AdminName);
			++BanCount;
		}
	}

	if (Ids.HasEOSPuid())
	{
		if (UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr)
		{
			EOSBans->BanPlayer(Ids.EOSProductUserId, Reason, DurationMinutes, AdminName);
			++BanCount;
		}
	}

	if (BanCount == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Player **%s** was found but no ban subsystem is available."),
		                      *ExactName));
		return;
	}

	const FString ReasonNote = Reason.IsEmpty()
	    ? TEXT("")
	    : FString::Printf(TEXT(" Reason: %s"), *Reason);

	Reply(ChannelId,
	      FString::Printf(TEXT(":hammer: **%s** has been banned %s on %d platform(s).%s"),
	                      *ExactName, *DurStr, BanCount, *ReasonNote));
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban remove
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanRemove(const FString& Arg,
                                            const FString& AdminName,
                                            const FString& ChannelId)
{
	if (Arg.IsEmpty())
	{
		Reply(ChannelId, TEXT(":warning: Usage: `!ban remove <PlayerName>`\n"
		                      ":information_source: The player must be currently online for name-based unban. "
		                      "For offline unban use `/steamunban <id>` or `/eosunban <id>` in-game."));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: No world context available. Try again after the server has fully loaded."));
		return;
	}

	FResolvedBanId  Ids;
	FString         ExactName;
	TArray<FString> Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, Arg, Ids, ExactName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":warning: Multiple online players match **%s**: %s\n"
			               "Please use a more specific name."),
			          *Arg, *FString::Join(Ambiguous, TEXT(", "))));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":x: No online player found matching **%s**.\n"
			               "Unban by name requires the player to be currently connected.\n"
			               "For offline unban use `/steamunban <Steam64Id>` or `/eosunban <EOSPuid>` in-game.\n"
			               "Use `/playerids` in-game to look up a player's raw ID."),
			          *Arg));
		}
		return;
	}

	UGameInstance* GI = GetGameInstance();
	int32 UnbanCount = 0;

	if (Ids.HasSteamId())
	{
		if (USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr)
		{
			if (SteamBans->UnbanPlayer(Ids.Steam64Id)) { ++UnbanCount; }
		}
	}

	if (Ids.HasEOSPuid())
	{
		if (UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr)
		{
			if (EOSBans->UnbanPlayer(Ids.EOSProductUserId)) { ++UnbanCount; }
		}
	}

	if (UnbanCount > 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":white_check_mark: **%s** has been unbanned on %d platform(s)."),
		                      *ExactName, UnbanCount));
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: **%s** is online but was not found in any active ban list."),
		                      *ExactName));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban list
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanList(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();

	TArray<FBanEntry> SteamBans;
	if (USteamBanSubsystem* Sub = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr)
	{
		SteamBans = Sub->GetAllBans();
	}

	TArray<FBanEntry> EOSBans;
	if (UEOSBanSubsystem* Sub = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr)
	{
		EOSBans = Sub->GetAllBans();
	}

	if (SteamBans.Num() == 0 && EOSBans.Num() == 0)
	{
		Reply(ChannelId, TEXT(":scroll: No active bans."));
		return;
	}

	FString Lines;

	if (SteamBans.Num() > 0)
	{
		Lines += FString::Printf(TEXT("**Steam bans (%d):**\n"), SteamBans.Num());
		for (const FBanEntry& E : SteamBans)
		{
			Lines += FString::Printf(TEXT("• `%s` — %s | Expires: %s | By: %s\n"),
			                         *E.PlayerId, *E.Reason,
			                         *E.GetExpiryString(), *E.BannedBy);
		}
	}

	if (EOSBans.Num() > 0)
	{
		if (!Lines.IsEmpty()) { Lines += TEXT("\n"); }
		Lines += FString::Printf(TEXT("**EOS bans (%d):**\n"), EOSBans.Num());
		for (const FBanEntry& E : EOSBans)
		{
			Lines += FString::Printf(TEXT("• `%s` — %s | Expires: %s | By: %s\n"),
			                         *E.PlayerId, *E.Reason,
			                         *E.GetExpiryString(), *E.BannedBy);
		}
	}

	// Discord messages have a 2000-character limit; truncate if needed.
	if (Lines.Len() > 1900)
	{
		Lines = Lines.Left(1900) + TEXT("\n*(list truncated — use in-game /steambanlist or /eosbanlist for the full list)*");
	}

	Reply(ChannelId, Lines.TrimEnd());
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban status
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanStatus(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();

	const int32 SteamCount = (GI && GI->GetSubsystem<USteamBanSubsystem>())
	                             ? GI->GetSubsystem<USteamBanSubsystem>()->GetBanCount()
	                             : 0;
	const int32 EOSCount   = (GI && GI->GetSubsystem<UEOSBanSubsystem>())
	                             ? GI->GetSubsystem<UEOSBanSubsystem>()->GetBanCount()
	                             : 0;

	Reply(ChannelId,
	      FString::Printf(
	          TEXT(":hammer: **BanSystem status:**\n"
	               "• Steam bans: **%d**\n"
	               "• EOS bans:   **%d**\n"
	               "\n"
	               ":information_source: Ban enforcement is always active. "
	               "Use `!ban list` to see full details."),
	          SteamCount, EOSCount));
}

// ─────────────────────────────────────────────────────────────────────────────
// !ban role
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleBanRole(const FString& Arg, const FString& ChannelId)
{
	FString RoleVerb, TargetUserId;
	if (!Arg.Split(TEXT(" "), &RoleVerb, &TargetUserId, ESearchCase::IgnoreCase))
	{
		RoleVerb     = Arg.TrimStartAndEnd();
		TargetUserId = TEXT("");
	}
	RoleVerb     = RoleVerb.TrimStartAndEnd().ToLower();
	TargetUserId = TargetUserId.TrimStartAndEnd();

	if (TargetUserId.IsEmpty())
	{
		Reply(ChannelId, TEXT(":warning: Usage: `!ban role add <discord_user_id>` "
		                      "or `!ban role remove <discord_user_id>`"));
		return;
	}

	if (!Provider)
	{
		Reply(ChannelId, TEXT(":x: Discord provider is not available."));
		return;
	}

	const bool bGrant = (RoleVerb == TEXT("add"));
	if (RoleVerb != TEXT("add") && RoleVerb != TEXT("remove"))
	{
		Reply(ChannelId, TEXT(":question: Usage: `!ban role add <discord_user_id>` "
		                      "or `!ban role remove <discord_user_id>`"));
		return;
	}

	if (!Provider->ManageBanDiscordRole(TargetUserId, bGrant))
	{
		Reply(ChannelId,
		      TEXT(":warning: `BanCommandRoleId` is not configured in `DefaultDiscordBridge.ini`. "
		           "Set it to the snowflake ID of the ban-admin Discord role you want to grant/revoke."));
		return;
	}

	if (bGrant)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":green_circle: Granting ban-admin role to Discord user `%s`…"),
		                      *TargetUserId));
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":red_circle: Revoking ban-admin role from Discord user `%s`…"),
		                      *TargetUserId));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Private helpers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Reply(const FString& ChannelId, const FString& Message) const
{
	if (Provider)
	{
		Provider->SendBanDiscordMessage(ChannelId, Message);
	}
}

void UBanDiscordSubsystem::ParseNameDurationReason(const FString& Input,
                                                    FString& OutName,
                                                    int32&   OutDurationMinutes,
                                                    FString& OutReason)
{
	OutDurationMinutes = 0;
	OutReason.Empty();

	const FString Trimmed = Input.TrimStartAndEnd();
	if (Trimmed.IsEmpty())
	{
		OutName.Empty();
		return;
	}

	FString Rest;
	if (!Trimmed.Split(TEXT(" "), &OutName, &Rest, ESearchCase::IgnoreCase))
	{
		OutName = Trimmed;
		return;
	}
	OutName = OutName.TrimStartAndEnd();
	Rest    = Rest.TrimStartAndEnd();

	if (Rest.IsEmpty()) { return; }

	FString DurToken, ReasonRest;
	if (!Rest.Split(TEXT(" "), &DurToken, &ReasonRest, ESearchCase::IgnoreCase))
	{
		DurToken   = Rest;
		ReasonRest = TEXT("");
	}
	DurToken = DurToken.TrimStartAndEnd();

	if (DurToken.IsNumeric())
	{
		OutDurationMinutes = FCString::Atoi(*DurToken);
		OutReason          = ReasonRest.TrimStartAndEnd();
	}
	else
	{
		OutDurationMinutes = 0;
		OutReason          = Rest;
	}
}
