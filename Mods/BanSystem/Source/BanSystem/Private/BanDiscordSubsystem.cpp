// Copyright Yamahasxviper. All Rights Reserved.

#include "BanDiscordSubsystem.h"
#include "Steam/SteamBanSubsystem.h"
#include "EOS/EOSBanSubsystem.h"
#include "BanPlayerLookup.h"
#include "BanIdResolver.h"
#include "Engine/GameInstance.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanDiscord, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load (or auto-create) the config from DefaultBanSystem.ini.
	Config = FBanDiscordConfig::LoadOrCreate();

	if (Config.DiscordChannelId.IsEmpty())
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: DiscordChannelId not configured — "
		            "Discord ban commands disabled until a channel ID is set in "
		            "Mods/BanSystem/Config/DefaultBanSystem.ini."));
	}
}

void UBanDiscordSubsystem::Deinitialize()
{
	// Remove any active provider (and its message subscription) before shutdown.
	SetCommandProvider(nullptr);
	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Provider registration
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::SetCommandProvider(IBanDiscordCommandProvider* InProvider)
{
	// Unsubscribe from the old provider if there is one.
	if (CommandProvider && CommandProvider != InProvider)
	{
		CommandProvider->UnsubscribeDiscordMessages(MessageSubscriptionHandle);
		MessageSubscriptionHandle.Reset();
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Disconnected from previous Discord provider."));
	}

	CommandProvider = InProvider;

	if (!CommandProvider)
	{
		UE_LOG(LogBanDiscord, Log,
		       TEXT("BanDiscordSubsystem: Discord command provider cleared."));
		return;
	}

	// Subscribe to Discord messages via the new provider.
	TWeakObjectPtr<UBanDiscordSubsystem> WeakThis(this);
	MessageSubscriptionHandle = CommandProvider->SubscribeDiscordMessages(
		[WeakThis](const TSharedPtr<FJsonObject>& MsgObj)
		{
			if (UBanDiscordSubsystem* Self = WeakThis.Get())
			{
				Self->OnDiscordMessageReceived(MsgObj);
			}
		});

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Discord command provider registered. "
	            "Listening on channel '%s'."),
	       *Config.DiscordChannelId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Discord message handling
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::OnDiscordMessageReceived(const TSharedPtr<FJsonObject>& MessageObj)
{
	if (!MessageObj.IsValid())
	{
		return;
	}

	// Only process messages from the configured ban commands channel.
	if (Config.DiscordChannelId.IsEmpty())
	{
		return;
	}

	FString MsgChannelId;
	if (!MessageObj->TryGetStringField(TEXT("channel_id"), MsgChannelId))
	{
		return;
	}
	if (MsgChannelId != Config.DiscordChannelId)
	{
		return;
	}

	// Extract author info.
	const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("author"), AuthorPtr) || !AuthorPtr)
	{
		return;
	}

	FString AuthorId;
	(*AuthorPtr)->TryGetStringField(TEXT("id"), AuthorId);

	// Ignore messages from bots (including our own bot).
	bool bIsBot = false;
	(*AuthorPtr)->TryGetBoolField(TEXT("bot"), bIsBot);
	if (bIsBot)
	{
		return;
	}

	// Get the display name (server nickname > global name > username).
	FString DisplayName;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
	{
		(*MemberPtr)->TryGetStringField(TEXT("nick"), DisplayName);
	}
	if (DisplayName.IsEmpty())
	{
		if (!(*AuthorPtr)->TryGetStringField(TEXT("global_name"), DisplayName) ||
		    DisplayName.IsEmpty())
		{
			(*AuthorPtr)->TryGetStringField(TEXT("username"), DisplayName);
		}
	}
	if (DisplayName.IsEmpty())
	{
		DisplayName = TEXT("Discord User");
	}

	// Extract message content.
	FString Content;
	MessageObj->TryGetStringField(TEXT("content"), Content);
	Content = Content.TrimStartAndEnd();
	if (Content.IsEmpty())
	{
		return;
	}

	// ── Check for permission-gated commands ──────────────────────────────────
	// Compute permission once; used by multiple branches below.
	bool bHasPermission = HasCommandPermission(MessageObj, AuthorId);

	// ── Command routing ───────────────────────────────────────────────────────
	// Check prefixes from longest to shortest to avoid ambiguous prefix matches
	// (e.g. "!steambanlist" must be checked before "!steamban").

	// Helper: check prefix and split rest of content.
	auto TryCommand = [&](const FString& Prefix, bool bRequiresAuth,
	                      TFunction<void(const FString&)> Handler) -> bool
	{
		if (Prefix.IsEmpty())
		{
			return false; // Command disabled via config.
		}
		if (!Content.StartsWith(Prefix, ESearchCase::IgnoreCase))
		{
			return false;
		}
		// Ensure the prefix is followed by whitespace or end of string
		// so that "!steamban" doesn't match "!steambanlist".
		const int32 PrefixLen = Prefix.Len();
		if (PrefixLen < Content.Len() && !FChar::IsWhitespace(Content[PrefixLen]))
		{
			return false;
		}
		if (bRequiresAuth && !bHasPermission)
		{
			Reply(MsgChannelId,
			      TEXT(":no_entry: You do not have permission to use BanSystem commands."));
			return true; // Consumed (even though we rejected it).
		}
		const FString Args = Content.Mid(PrefixLen).TrimStartAndEnd();
		Handler(Args);
		return true;
	};

	// List commands (no destructive action, still require permission).
	if (TryCommand(Config.SteamBanListCommandPrefix, true, [&](const FString&)
	               { HandleSteamBanListCommand(MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSBanListCommandPrefix, true, [&](const FString&)
	               { HandleEOSBanListCommand(MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.PlayerIdsCommandPrefix, true, [&](const FString& Args)
	               { HandlePlayerIdsCommand(Args, MsgChannelId); }))
	{
		return;
	}

	// Unban commands.
	if (TryCommand(Config.SteamUnbanCommandPrefix, true, [&](const FString& Args)
	               { HandleSteamUnbanCommand(Args, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSUnbanCommandPrefix, true, [&](const FString& Args)
	               { HandleEOSUnbanCommand(Args, MsgChannelId); }))
	{
		return;
	}

	// Ban commands (checked after unban so "!steamunban" prefix wins over "!steamban").
	if (TryCommand(Config.SteamBanCommandPrefix, true, [&](const FString& Args)
	               { HandleSteamBanCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.EOSBanCommandPrefix, true, [&](const FString& Args)
	               { HandleEOSBanCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
	if (TryCommand(Config.BanByNameCommandPrefix, true, [&](const FString& Args)
	               { HandleBanByNameCommand(Args, DisplayName, MsgChannelId); }))
	{
		return;
	}
}

bool UBanDiscordSubsystem::HasCommandPermission(const TSharedPtr<FJsonObject>& MessageObj,
                                                 const FString&                  AuthorId) const
{
	if (!CommandProvider)
	{
		return false;
	}

	// Guild owner always has permission.
	const FString& OwnerId = CommandProvider->GetGuildOwnerId();
	if (!OwnerId.IsEmpty() && AuthorId == OwnerId)
	{
		return true;
	}

	// When no role is configured, only the guild owner may run commands.
	if (Config.DiscordCommandRoleId.IsEmpty())
	{
		return false;
	}

	// Check whether the member holds the required role.
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) || !MemberPtr)
	{
		return false;
	}

	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
	{
		for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
		{
			FString RoleId;
			if (RoleVal->TryGetString(RoleId) && RoleId == Config.DiscordCommandRoleId)
			{
				return true;
			}
		}
	}

	return false;
}

// ─────────────────────────────────────────────────────────────────────────────
// Command handlers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::HandleSteamBanCommand(const FString& Args,
                                                  const FString& IssuedBy,
                                                  const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id|PlayerName> [duration_minutes] [reason]`"),
		                      *Config.SteamBanCommandPrefix));
		return;
	}

	const FString Input = Parts[0];

	// Resolve Steam64 ID.
	FString Steam64Id;
	if (USteamBanSubsystem::IsValidSteam64Id(Input))
	{
		Steam64Id = Input;
	}
	else
	{
		// Attempt name lookup.
		UWorld* World = GetWorld();
		if (!World)
		{
			Reply(ChannelId, TEXT(":x: World not available — cannot resolve player name."));
			return;
		}
		FResolvedBanId Ids;
		FString PlayerName;
		TArray<FString> Ambiguous;
		if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
		{
			if (Ambiguous.Num() > 1)
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
				                      *Input, *FString::Join(Ambiguous, TEXT(", "))));
			}
			else
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":x: `%s` is not a valid Steam64 ID and no online player "
				                          "with that name was found."),
				                      *Input));
			}
			return;
		}
		if (!Ids.HasSteamId())
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":yellow_circle: Player `%s` has no Steam ID. "
			                          "They may be an Epic-only player — use `%s` instead."),
			                      *PlayerName, *Config.EOSBanCommandPrefix));
			return;
		}
		Reply(ChannelId,
		      FString::Printf(TEXT(":mag: Resolved `%s` → Steam64 ID: `%s`"),
		                      *PlayerName, *Ids.Steam64Id));
		Steam64Id = Ids.Steam64Id;
	}

	// Parse optional duration and reason from the remaining arguments.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (Parts.Num() > 1)
	{
		if (Parts[1].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[1]);
			if (Parts.Num() > 2)
			{
				TArray<FString> ReasonParts;
				for (int32 i = 2; i < Parts.Num(); ++i)
				{
					ReasonParts.Add(Parts[i]);
				}
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				ReasonParts.Add(Parts[i]);
			}
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	Bans->BanPlayer(Steam64Id, Reason, Duration, IssuedBy);

	FString Msg = Config.SteamBanResponseMessage;
	Msg.ReplaceInline(TEXT("%PlayerId%"),  *Steam64Id, ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Reason%"),    *Reason,    ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%BannedBy%"),  *IssuedBy,  ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Duration%"),  *FormatDuration(Duration), ESearchCase::CaseSensitive);
	Reply(ChannelId, Msg);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: Steam ban issued by '%s' — ID: %s, Duration: %d min, Reason: %s"),
	       *IssuedBy, *Steam64Id, Duration, *Reason);
}

void UBanDiscordSubsystem::HandleSteamUnbanCommand(const FString& Args,
                                                    const FString& ChannelId)
{
	const FString Steam64Id = Args.TrimStartAndEnd();
	if (Steam64Id.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <Steam64Id>`"),
		                      *Config.SteamUnbanCommandPrefix));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	if (Bans->UnbanPlayer(Steam64Id))
	{
		FString Msg = Config.SteamUnbanResponseMessage;
		Msg.ReplaceInline(TEXT("%PlayerId%"), *Steam64Id, ESearchCase::CaseSensitive);
		Reply(ChannelId, Msg);
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: Steam64 ID `%s` was not on the ban list."),
		                      *Steam64Id));
	}
}

void UBanDiscordSubsystem::HandleSteamBanListCommand(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();
	USteamBanSubsystem* Bans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: Steam ban subsystem is not available."));
		return;
	}

	const TArray<FBanEntry> AllBans = Bans->GetAllBans();
	if (AllBans.Num() == 0)
	{
		Reply(ChannelId, TEXT(":scroll: **Steam Ban List** — No active Steam bans."));
		return;
	}

	FString ListMsg = FString::Printf(
		TEXT(":scroll: **Steam Ban List** — %d active ban(s):\n"), AllBans.Num());

	for (const FBanEntry& Entry : AllBans)
	{
		ListMsg += FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);
	}

	// Discord message length limit is 2000 chars; truncate with a notice if needed.
	if (ListMsg.Len() > 1900)
	{
		ListMsg = ListMsg.Left(1900) + TEXT("\n… (list truncated — too many bans to display)");
	}

	Reply(ChannelId, ListMsg);
}

void UBanDiscordSubsystem::HandleEOSBanCommand(const FString& Args,
                                                const FString& IssuedBy,
                                                const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <EOSProductUserId|PlayerName> [duration_minutes] [reason]`"),
		                      *Config.EOSBanCommandPrefix));
		return;
	}

	const FString Input = Parts[0];

	// Resolve EOS Product User ID.
	FString EOSPUID;
	if (UEOSBanSubsystem::IsValidEOSProductUserId(Input))
	{
		EOSPUID = Input;
	}
	else
	{
		UWorld* World = GetWorld();
		if (!World)
		{
			Reply(ChannelId, TEXT(":x: World not available — cannot resolve player name."));
			return;
		}
		FResolvedBanId Ids;
		FString PlayerName;
		TArray<FString> Ambiguous;
		if (!FBanPlayerLookup::FindPlayerByName(World, Input, Ids, PlayerName, Ambiguous))
		{
			if (Ambiguous.Num() > 1)
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
				                      *Input, *FString::Join(Ambiguous, TEXT(", "))));
			}
			else
			{
				Reply(ChannelId,
				      FString::Printf(TEXT(":x: `%s` is not a valid EOS Product User ID and no "
				                          "online player with that name was found."),
				                      *Input));
			}
			return;
		}
		if (!Ids.HasEOSPuid())
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":yellow_circle: Player `%s` has no EOS Product User ID. "
			                          "They may be a Steam-only player — use `%s` instead."),
			                      *PlayerName, *Config.SteamBanCommandPrefix));
			return;
		}
		Reply(ChannelId,
		      FString::Printf(TEXT(":mag: Resolved `%s` → EOS PUID: `%s`"),
		                      *PlayerName, *Ids.EOSProductUserId));
		EOSPUID = Ids.EOSProductUserId;
	}

	// Parse optional duration and reason.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (Parts.Num() > 1)
	{
		if (Parts[1].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[1]);
			if (Parts.Num() > 2)
			{
				TArray<FString> ReasonParts;
				for (int32 i = 2; i < Parts.Num(); ++i)
				{
					ReasonParts.Add(Parts[i]);
				}
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				ReasonParts.Add(Parts[i]);
			}
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	Bans->BanPlayer(EOSPUID, Reason, Duration, IssuedBy);

	FString Msg = Config.EOSBanResponseMessage;
	Msg.ReplaceInline(TEXT("%PlayerId%"), *EOSPUID,            ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Reason%"),   *Reason,             ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%BannedBy%"), *IssuedBy,           ESearchCase::CaseSensitive);
	Msg.ReplaceInline(TEXT("%Duration%"), *FormatDuration(Duration), ESearchCase::CaseSensitive);
	Reply(ChannelId, Msg);

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: EOS ban issued by '%s' — ID: %s, Duration: %d min, Reason: %s"),
	       *IssuedBy, *EOSPUID, Duration, *Reason);
}

void UBanDiscordSubsystem::HandleEOSUnbanCommand(const FString& Args,
                                                  const FString& ChannelId)
{
	const FString EOSPUID = Args.TrimStartAndEnd();
	if (EOSPUID.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <EOSProductUserId>`"),
		                      *Config.EOSUnbanCommandPrefix));
		return;
	}

	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	if (Bans->UnbanPlayer(EOSPUID))
	{
		FString Msg = Config.EOSUnbanResponseMessage;
		Msg.ReplaceInline(TEXT("%PlayerId%"), *EOSPUID, ESearchCase::CaseSensitive);
		Reply(ChannelId, Msg);
	}
	else
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: EOS Product User ID `%s` was not on the ban list."),
		                      *EOSPUID));
	}
}

void UBanDiscordSubsystem::HandleEOSBanListCommand(const FString& ChannelId)
{
	UGameInstance* GI = GetGameInstance();
	UEOSBanSubsystem* Bans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
	if (!Bans)
	{
		Reply(ChannelId, TEXT(":x: EOS ban subsystem is not available."));
		return;
	}

	const TArray<FBanEntry> AllBans = Bans->GetAllBans();
	if (AllBans.Num() == 0)
	{
		Reply(ChannelId, TEXT(":scroll: **EOS Ban List** — No active EOS bans."));
		return;
	}

	FString ListMsg = FString::Printf(
		TEXT(":scroll: **EOS Ban List** — %d active ban(s):\n"), AllBans.Num());

	for (const FBanEntry& Entry : AllBans)
	{
		ListMsg += FString::Printf(
			TEXT("• `%s` — Reason: %s | Expires: %s | Banned by: %s\n"),
			*Entry.PlayerId,
			*Entry.Reason,
			*Entry.GetExpiryString(),
			*Entry.BannedBy);
	}

	if (ListMsg.Len() > 1900)
	{
		ListMsg = ListMsg.Left(1900) + TEXT("\n… (list truncated — too many bans to display)");
	}

	Reply(ChannelId, ListMsg);
}

void UBanDiscordSubsystem::HandleBanByNameCommand(const FString& Args,
                                                   const FString& IssuedBy,
                                                   const FString& ChannelId)
{
	TArray<FString> Parts = SplitArgs(Args);
	if (Parts.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":warning: Usage: `%s <PlayerName> [duration_minutes] [reason]`"),
		                      *Config.BanByNameCommandPrefix));
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: World not available — cannot look up player."));
		return;
	}

	const FString NameQuery = Parts[0];
	FResolvedBanId Ids;
	FString PlayerName;
	TArray<FString> Ambiguous;

	if (!FBanPlayerLookup::FindPlayerByName(World, NameQuery, Ids, PlayerName, Ambiguous))
	{
		if (Ambiguous.Num() > 1)
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":warning: Ambiguous name `%s`. Matching players: %s"),
			                      *NameQuery, *FString::Join(Ambiguous, TEXT(", "))));
		}
		else
		{
			Reply(ChannelId,
			      FString::Printf(TEXT(":x: No online player found matching `%s`."), *NameQuery));
		}
		return;
	}

	if (!Ids.IsValid())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":x: Player `%s` has no resolvable platform IDs."), *PlayerName));
		return;
	}

	// Parse optional duration and reason.
	int32   Duration = 0;
	FString Reason   = TEXT("Banned by server administrator");
	if (Parts.Num() > 1)
	{
		if (Parts[1].IsNumeric())
		{
			Duration = FCString::Atoi(*Parts[1]);
			if (Parts.Num() > 2)
			{
				TArray<FString> ReasonParts;
				for (int32 i = 2; i < Parts.Num(); ++i)
				{
					ReasonParts.Add(Parts[i]);
				}
				Reason = FString::Join(ReasonParts, TEXT(" "));
			}
		}
		else
		{
			TArray<FString> ReasonParts;
			for (int32 i = 1; i < Parts.Num(); ++i)
			{
				ReasonParts.Add(Parts[i]);
			}
			Reason = FString::Join(ReasonParts, TEXT(" "));
		}
	}

	UGameInstance* GI = GetGameInstance();
	TArray<FString> BannedOn;

	if (Ids.HasSteamId())
	{
		USteamBanSubsystem* SteamBans = GI ? GI->GetSubsystem<USteamBanSubsystem>() : nullptr;
		if (SteamBans && SteamBans->BanPlayer(Ids.Steam64Id, Reason, Duration, IssuedBy))
		{
			BannedOn.Add(FString::Printf(TEXT("Steam (`%s`)"), *Ids.Steam64Id));
		}
	}

	if (Ids.HasEOSPuid())
	{
		UEOSBanSubsystem* EOSBans = GI ? GI->GetSubsystem<UEOSBanSubsystem>() : nullptr;
		if (EOSBans && EOSBans->BanPlayer(Ids.EOSProductUserId, Reason, Duration, IssuedBy))
		{
			BannedOn.Add(FString::Printf(TEXT("EOS (`%s`)"), *Ids.EOSProductUserId));
		}
	}

	if (BannedOn.Num() == 0)
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":yellow_circle: Player `%s` is already banned on all "
		                          "available platforms."),
		                      *PlayerName));
		return;
	}

	Reply(ChannelId,
	      FString::Printf(TEXT(":hammer: **BanSystem** — `%s` banned by **%s** on %s. "
	                           "Duration: %s. Reason: %s"),
	                      *PlayerName,
	                      *IssuedBy,
	                      *FString::Join(BannedOn, TEXT(" and ")),
	                      *FormatDuration(Duration),
	                      *Reason));

	UE_LOG(LogBanDiscord, Log,
	       TEXT("BanDiscordSubsystem: banbyname '%s' issued by '%s' — platforms: %s, "
	            "Duration: %d min, Reason: %s"),
	       *PlayerName, *IssuedBy, *FString::Join(BannedOn, TEXT(", ")), Duration, *Reason);
}

void UBanDiscordSubsystem::HandlePlayerIdsCommand(const FString& Args,
                                                   const FString& ChannelId)
{
	UWorld* World = GetWorld();
	if (!World)
	{
		Reply(ChannelId, TEXT(":x: World not available."));
		return;
	}

	const FString NameFilter = Args.TrimStartAndEnd();
	const TArray<TPair<FString, FResolvedBanId>> AllPlayers =
		FBanPlayerLookup::GetAllConnectedPlayers(World);

	if (AllPlayers.Num() == 0)
	{
		Reply(ChannelId, TEXT(":busts_in_silhouette: No players are currently connected."));
		return;
	}

	FString ListMsg;

	for (const TPair<FString, FResolvedBanId>& KV : AllPlayers)
	{
		const FString& Name = KV.Key;
		const FResolvedBanId& Ids = KV.Value;

		// When a name filter is provided, skip non-matching players.
		if (!NameFilter.IsEmpty() &&
		    !Name.Contains(NameFilter, ESearchCase::IgnoreCase))
		{
			continue;
		}

		FString IdLine = FString::Printf(TEXT("• **%s**"), *Name);
		if (Ids.HasSteamId())
		{
			IdLine += FString::Printf(TEXT(" — Steam: `%s`"), *Ids.Steam64Id);
		}
		if (Ids.HasEOSPuid())
		{
			IdLine += FString::Printf(TEXT(" — EOS: `%s`"), *Ids.EOSProductUserId);
		}
		if (!Ids.IsValid())
		{
			IdLine += TEXT(" — *no platform IDs resolved*");
		}
		ListMsg += IdLine + TEXT("\n");
	}

	if (ListMsg.IsEmpty())
	{
		Reply(ChannelId,
		      FString::Printf(TEXT(":busts_in_silhouette: No connected players match `%s`."),
		                      *NameFilter));
		return;
	}

	const FString Header = NameFilter.IsEmpty()
		? FString::Printf(TEXT(":id: **Connected Players (%d):**\n"), AllPlayers.Num())
		: FString::Printf(TEXT(":id: **Players matching `%s`:**\n"), *NameFilter);

	FString FullMsg = Header + ListMsg;
	if (FullMsg.Len() > 1900)
	{
		FullMsg = FullMsg.Left(1900) + TEXT("\n… (list truncated)");
	}

	Reply(ChannelId, FullMsg);
}

// ─────────────────────────────────────────────────────────────────────────────
// Helpers
// ─────────────────────────────────────────────────────────────────────────────

void UBanDiscordSubsystem::Reply(const FString& ChannelId, const FString& Message) const
{
	if (CommandProvider && !ChannelId.IsEmpty())
	{
		CommandProvider->SendDiscordChannelMessage(ChannelId, Message);
	}
}

FString UBanDiscordSubsystem::FormatDuration(int32 DurationMinutes)
{
	return (DurationMinutes <= 0)
		? TEXT("permanently")
		: FString::Printf(TEXT("%d minute(s)"), DurationMinutes);
}

TArray<FString> UBanDiscordSubsystem::SplitArgs(const FString& Input)
{
	TArray<FString> Parts;
	Input.TrimStartAndEnd().ParseIntoArrayWS(Parts);
	return Parts;
}
