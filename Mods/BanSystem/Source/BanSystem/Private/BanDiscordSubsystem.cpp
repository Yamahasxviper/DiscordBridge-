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
// Discord command handling — ban by player name
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleDiscordBanByNameCommand(const FString& NameAndArgs,
                                                          const FString& AdminName,
                                                          const FString& ChannelId)
{
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: ban-by-name from '%s': '%s'"), *AdminName, *NameAndArgs);

	FString PlayerNameQuery;
	int32   DurationMinutes = 0;
	FString Reason;
	ParseNameDurationReason(NameAndArgs, PlayerNameQuery, DurationMinutes, Reason);

	if (PlayerNameQuery.IsEmpty())
	{
		Reply(ChannelId, TEXT(":warning: Usage: `!ban add <PlayerName> [duration_minutes] [reason...]`\n"
		                      "Example: `!ban add SomePlayer 60 Spamming`\n"
		                      "Use duration 0 (or omit) for a permanent ban."));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: No world context available. Try again after the server has fully loaded."));
		return;
	}

	FResolvedBanId   Ids;
	FString          ExactName;
	TArray<FString>  Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, PlayerNameQuery, Ids, ExactName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			const FString AmbigList = FString::Join(Ambiguous, TEXT(", "));
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":warning: Multiple players match **%s**: %s\n"
			               "Please use a more specific name."),
			          *PlayerNameQuery, *AmbigList));
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

	const FString DurStr  = (DurationMinutes > 0)
	                            ? FString::Printf(TEXT("for %d minute(s)"), DurationMinutes)
	                            : TEXT("permanently");
	int32 BanCount = 0;

	UGameInstance* GI = GetGameInstance();

	// ── Steam ban ─────────────────────────────────────────────────────────────
	if (Ids.HasSteamId())
	{
		USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
		if (SteamBans)
		{
			SteamBans->BanPlayer(Ids.Steam64Id, Reason, DurationMinutes, AdminName);
			++BanCount;
		}
	}

	// ── EOS ban ───────────────────────────────────────────────────────────────
	if (Ids.HasEOSPuid())
	{
		UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
		if (EOSBans)
		{
			EOSBans->BanPlayer(Ids.EOSProductUserId, Reason, DurationMinutes, AdminName);
			++BanCount;
		}
	}

	if (BanCount == 0)
	{
		Reply(ChannelId,
		      FString::Printf(
		          TEXT(":x: Player **%s** was found but could not be banned "
		               "(no ban subsystem available)."),
		          *ExactName));
		return;
	}

	const FString ReasonNote = Reason.IsEmpty()
	    ? TEXT("")
	    : FString::Printf(TEXT(" Reason: %s"), *Reason);

	Reply(ChannelId,
	      FString::Printf(
	          TEXT(":hammer: **%s** has been banned %s on %d platform(s).%s"),
	          *ExactName, *DurStr, BanCount, *ReasonNote));
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord command handling — unban by player name
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleDiscordUnbanByNameCommand(const FString& PlayerName,
                                                            const FString& AdminName,
                                                            const FString& ChannelId)
{
	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: unban-by-name from '%s': '%s'"), *AdminName, *PlayerName);

	if (PlayerName.IsEmpty())
	{
		Reply(ChannelId, TEXT(":warning: Usage: `!ban remove <PlayerName>`\n"
		                      "Note: the player must be currently online for name-based unban. "
		                      "For offline unban use `/steamunban <id>` or `/eosunban <id>` in-game."));
		return;
	}

	UWorld* World = GetGameInstance() ? GetGameInstance()->GetWorld() : nullptr;
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: No world context available. Try again after the server has fully loaded."));
		return;
	}

	FResolvedBanId   Ids;
	FString          ExactName;
	TArray<FString>  Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, PlayerName, Ids, ExactName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			const FString AmbigList = FString::Join(Ambiguous, TEXT(", "));
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":warning: Multiple online players match **%s**: %s\n"
			               "Please use a more specific name."),
			          *PlayerName, *AmbigList));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(
			          TEXT(":x: No online player found matching **%s**.\n"
			               "Unban by name requires the player to be currently connected.\n"
			               "For offline unban use `/steamunban <Steam64Id>` or `/eosunban <EOSPuid>` in-game.\n"
			               "Use `/playerids` in-game to look up a player's raw ID."),
			          *PlayerName));
		}
		return;
	}

	UGameInstance* GI = GetGameInstance();
	int32 UnbanCount = 0;

	// ── Steam unban ───────────────────────────────────────────────────────────
	if (Ids.HasSteamId())
	{
		USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
		if (SteamBans && SteamBans->UnbanPlayer(Ids.Steam64Id))
		{
			++UnbanCount;
		}
	}

	// ── EOS unban ─────────────────────────────────────────────────────────────
	if (Ids.HasEOSPuid())
	{
		UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
		if (EOSBans && EOSBans->UnbanPlayer(Ids.EOSProductUserId))
		{
			++UnbanCount;
		}
	}

	if (UnbanCount > 0)
	{
		Reply(ChannelId,
		      FString::Printf(
		          TEXT(":white_check_mark: **%s** has been unbanned on %d platform(s)."),
		          *ExactName, UnbanCount));
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(
		          TEXT(":yellow_circle: **%s** is online but was not found in any ban list."),
		          *ExactName));
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

	// Split on first space to get the player name.
	FString Rest;
	if (!Trimmed.Split(TEXT(" "), &OutName, &Rest, ESearchCase::IgnoreCase))
	{
		// Only a name, no extra args.
		OutName = Trimmed;
		return;
	}
	OutName = OutName.TrimStartAndEnd();
	Rest    = Rest.TrimStartAndEnd();

	if (Rest.IsEmpty())
	{
		return;
	}

	// Try to parse the second token as a duration (integer).
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
		// Not numeric — treat the entire Rest as the reason (no explicit duration).
		OutDurationMinutes = 0;
		OutReason          = Rest;
	}
}
