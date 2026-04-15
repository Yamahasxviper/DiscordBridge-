// Copyright Yamahasxviper. All Rights Reserved.

#include "TicketSubsystem.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "Engine/GameInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "HAL/PlatformFileManager.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "BanAppealRegistry.h"
#include "WhitelistManager.h"
#include "Containers/Ticker.h"

DEFINE_LOG_CATEGORY(LogTicketSystem);

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UTicketSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// Only create on dedicated servers.
	return IsRunningDedicatedServer();
}

void UTicketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Load ticket configuration.
	FTicketConfig::RestoreDefaultConfigIfNeeded();
	Config = FTicketConfig::Load();

	// Restore any active tickets from the previous session so the duplicate-ticket
	// check works correctly immediately after a server restart.
	LoadTicketState();

	// Start the reopen grace period expiry ticker when configured.
	if (Config.TicketReopenGracePeriodMinutes > 0)
	{
		ReopenExpiryTickHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				const FDateTime Now = FDateTime::UtcNow();
				TArray<FString> Expired;
				for (const auto& Pair : PendingReopenExpiry)
				{
					if (Now >= Pair.Value)
					{
						Expired.Add(Pair.Key);
					}
				}
				for (const FString& ChanId : Expired)
				{
					PendingReopenExpiry.Remove(ChanId);
					FString OpenerId;
					PendingReopenOpener.RemoveAndCopyValue(ChanId, OpenerId);
					IDiscordBridgeProvider* B = GetBridge();
					if (B)
					{
						B->DeleteDiscordChannel(ChanId);
					}
				}
				return true;
			}),
			30.0f);
	}

	// Start the inactive-ticket check ticker when enabled.
	if (Config.InactiveTicketTimeoutHours > 0.0f)
	{
		// Check every 5 minutes; the real threshold is InactiveTicketTimeoutHours.
		constexpr float CheckIntervalSeconds = 5.0f * 60.0f;

		TWeakObjectPtr<UTicketSubsystem> WeakThis(this);
		InactiveTicketCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				const FTimespan Threshold = FTimespan::FromHours(
					static_cast<double>(Config.InactiveTicketTimeoutHours));
				const FDateTime Now = FDateTime::UtcNow();

				// Collect channels that have timed out; avoid modifying the map while iterating.
				TArray<FString> TimedOut;
				for (const auto& Pair : TicketChannelToOpener)
				{
					const FString& ChanId = Pair.Key;
					const FDateTime* LastActivity = TicketChannelToLastActivity.Find(ChanId);
					if (!LastActivity)
					{
						// No activity tracked yet — use now as the baseline so a fresh
						// ticket is not immediately closed after a restart.
						TicketChannelToLastActivity.Add(ChanId, Now);
						continue;
					}
					if ((Now - *LastActivity) >= Threshold)
					{
						TimedOut.Add(ChanId);
					}
				}

				for (const FString& ChanId : TimedOut)
				{
					UE_LOG(LogTicketSystem, Log,
					       TEXT("TicketSystem: Closing inactive ticket channel %s (no activity for %.1f h)."),
					       *ChanId, Config.InactiveTicketTimeoutHours);
					CloseTicketChannelInactive(ChanId);
				}

				return true; // keep ticking
			}),
			CheckIntervalSeconds);

		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Inactive-ticket timeout enabled (%.1f h, checked every 5 min)."),
		       Config.InactiveTicketTimeoutHours);
	}

	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Initialized. Waiting for Discord provider to be injected via SetProvider()."));

	// Start the SLA warning ticker when enabled.
	if (Config.TicketSlaWarningMinutes > 0)
	{
		SlaCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
			FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
			{
				if (Config.TicketSlaWarningMinutes <= 0) return true;
				const FDateTime Now = FDateTime::UtcNow();
				const FTimespan SlaThreshold = FTimespan::FromMinutes(
					static_cast<double>(Config.TicketSlaWarningMinutes));
				IDiscordBridgeProvider* SlaB = GetBridge();
				if (!SlaB) return true;

				for (const TPair<FString, FString>& SLAPair : TicketChannelToOpener)
				{
					const FString& SlaChanId = SLAPair.Key;
					const bool* bReplied = TicketChannelStaffReplied.Find(SlaChanId);
					if (bReplied && *bReplied) continue;

					const FDateTime* OpenTime = TicketChannelToOpenTime.Find(SlaChanId);
					if (!OpenTime) continue;

					if ((Now - *OpenTime) >= SlaThreshold)
					{
						const FString WarnMsg = FString::Printf(
							TEXT(":alarm_clock: SLA WARNING: Ticket <#%s> opened by <@%s> "
							     "has not received a staff response after %d minutes!"),
							*SlaChanId, *SLAPair.Value, Config.TicketSlaWarningMinutes);
						const TArray<FString> SlaChans =
							FTicketConfig::ParseChannelIds(Config.TicketChannelId);
						for (const FString& NChan : SlaChans)
						{
							if (!NChan.IsEmpty())
								SlaB->SendDiscordChannelMessage(NChan, WarnMsg);
						}
						// Mark as warned to prevent repeated SLA warnings for this ticket.
						TicketChannelStaffReplied.Add(SlaChanId, true);
					}
				}
				return true;
			}),
			60.0f);

		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: SLA warning ticker enabled (%d min threshold)."),
		       Config.TicketSlaWarningMinutes);
	}

	// Start the follow-up reminder check ticker (always active; no-op when map is empty).
	ReminderCheckHandle = FTSTicker::GetCoreTicker().AddTicker(
		FTickerDelegate::CreateWeakLambda(this, [this](float) -> bool
		{
			if (TicketChannelToReminder.Num() == 0) return true;
			const FDateTime RNow = FDateTime::UtcNow();
			TArray<FString> FiredReminders;
			for (const TPair<FString, FDateTime>& RPair : TicketChannelToReminder)
			{
				if (RNow >= RPair.Value)
					FiredReminders.Add(RPair.Key);
			}
			IDiscordBridgeProvider* RemB = GetBridge();
			for (const FString& RemChanId : FiredReminders)
			{
				TicketChannelToReminder.Remove(RemChanId);
				if (RemB)
				{
					RemB->SendDiscordChannelMessage(RemChanId,
						TEXT(":alarm_clock: **Reminder:** This ticket needs follow-up!"));
				}
			}
			if (FiredReminders.Num() > 0)
				SaveTicketState();
			return true;
		}),
		60.0f);

	// Load the ticket blacklist.
	LoadTicketBlacklist();
}

void UTicketSubsystem::Deinitialize()
{
	// Stop the inactive-ticket check ticker.
	if (InactiveTicketCheckHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(InactiveTicketCheckHandle);
		InactiveTicketCheckHandle.Reset();
	}

	// Stop the SLA warning ticker.
	if (SlaCheckHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(SlaCheckHandle);
		SlaCheckHandle.Reset();
	}

	// Stop the follow-up reminder ticker.
	if (ReminderCheckHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReminderCheckHandle);
		ReminderCheckHandle.Reset();
	}

	// Detach from the provider (unsubscribes delegates, clears CachedProvider).
	SetProvider(nullptr);

	if (ReopenExpiryTickHandle.IsValid())
	{
		FTSTicker::GetCoreTicker().RemoveTicker(ReopenExpiryTickHandle);
		ReopenExpiryTickHandle.Reset();
	}

	TicketChannelToOpener.Empty();
	OpenerToTicketChannel.Empty();
	TicketChannelToAssignee.Empty();
	TicketChannelToAssigneeName.Empty();
	TicketChannelToLastActivity.Empty();
	TicketChannelToOpenerName.Empty();
	TicketChannelToOpenTime.Empty();
	TicketChannelToType.Empty();
	TicketChannelToPriority.Empty();
	UserTicketCooldown.Empty();
	PendingReopenExpiry.Empty();
	PendingReopenOpener.Empty();
	OpenerToTicketsByType.Empty();
	TicketChannelToTags.Empty();
	TicketChannelToNotes.Empty();
	TicketChannelStaffReplied.Empty();
	TicketChannelToReminder.Empty();
	TicketBlacklist.Empty();

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: get the Discord provider
// ─────────────────────────────────────────────────────────────────────────────

IDiscordBridgeProvider* UTicketSubsystem::GetBridge() const
{
	return CachedProvider;
}

// ─────────────────────────────────────────────────────────────────────────────
// SetProvider – inject (or clear) the Discord bridge provider
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::SetProvider(IDiscordBridgeProvider* InProvider)
{
	// Idempotency: if the same non-null provider is already registered, do
	// nothing.  Without this guard, calling SetProvider twice with the same
	// provider would add a second subscription for both interactions and raw
	// messages, causing every event to be dispatched to the handler twice.
	if (InProvider && InProvider == CachedProvider)
	{
		return;
	}

	// Unsubscribe from the current provider before switching.
	if (CachedProvider && CachedProvider != InProvider)
	{
		CachedProvider->UnsubscribeInteraction(InteractionDelegateHandle);
		CachedProvider->UnsubscribeRawMessage(RawMessageDelegateHandle);
		InteractionDelegateHandle.Reset();
		RawMessageDelegateHandle.Reset();
	}

	CachedProvider = InProvider;

	if (CachedProvider)
	{
		// Use weak-object captures so the lambda is safe if this subsystem is
		// garbage-collected before the provider removes the subscription.
		TWeakObjectPtr<UTicketSubsystem> WeakThis(this);

		InteractionDelegateHandle = CachedProvider->SubscribeInteraction(
			[WeakThis](const TSharedPtr<FJsonObject>& DataObj)
			{
				if (UTicketSubsystem* Ts = WeakThis.Get())
				{
					Ts->OnInteractionReceived(DataObj);
				}
			});

		RawMessageDelegateHandle = CachedProvider->SubscribeRawMessage(
			[WeakThis](const TSharedPtr<FJsonObject>& MsgObj)
			{
				if (UTicketSubsystem* Ts = WeakThis.Get())
				{
					Ts->OnRawDiscordMessage(MsgObj);
				}
			});

		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Discord provider set. Subscribed to interaction and message events."));
	}
	else
	{
		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Discord provider cleared."));
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: sanitize a Discord username into a valid channel-name segment
// ─────────────────────────────────────────────────────────────────────────────

FString UTicketSubsystem::SanitizeUsernameForChannel(const FString& Username)
{
	FString Result;
	for (TCHAR C : Username)
	{
		if (FChar::IsAlnum(C))
		{
			Result.AppendChar(FChar::ToLower(C));
		}
		else if (C == TCHAR(' ') || C == TCHAR('_') || C == TCHAR('-'))
		{
			Result.AppendChar(TCHAR('-'));
		}
	}

	// Collapse consecutive dashes and trim leading/trailing dashes.
	while (Result.Contains(TEXT("--")))
	{
		Result = Result.Replace(TEXT("--"), TEXT("-"), ESearchCase::CaseSensitive);
	}
	Result = Result.TrimStartAndEnd();
	if (Result.StartsWith(TEXT("-")))
	{
		Result = Result.Mid(1);
	}
	if (Result.EndsWith(TEXT("-")))
	{
		Result = Result.Left(Result.Len() - 1);
	}

	// Clamp to 40 chars so the full channel name fits within Discord's 100-char limit.
	if (Result.Len() > 40)
	{
		Result = Result.Left(40);
	}

	return Result.IsEmpty() ? TEXT("user") : Result;
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: build the close-ticket button message body
// ─────────────────────────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> UTicketSubsystem::MakeCloseButtonMessage(const FString& OpenerUserId)
{
	TSharedPtr<FJsonObject> CloseButton = MakeShared<FJsonObject>();
	CloseButton->SetNumberField(TEXT("type"),  2);  // BUTTON
	CloseButton->SetNumberField(TEXT("style"), 4);  // DANGER (red)
	CloseButton->SetStringField(TEXT("label"), TEXT("Close Ticket"));
	CloseButton->SetStringField(TEXT("custom_id"),
		FString::Printf(TEXT("ticket_close:%s"), *OpenerUserId));

	TSharedPtr<FJsonObject> ActionRow = MakeShared<FJsonObject>();
	ActionRow->SetNumberField(TEXT("type"), 1); // ACTION_ROW
	ActionRow->SetArrayField(TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(CloseButton) });

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetStringField(TEXT("content"),
		TEXT(":lock: Click **Close Ticket** when the issue is resolved. "
		     "The channel will be deleted automatically.\n"
		     ":information_source: The ticket opener and any member with the "
		     "admin/support role can close this ticket."));
	Body->SetArrayField(TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(ActionRow) });

	return Body;
}

// ─────────────────────────────────────────────────────────────────────────────
// Interaction routing
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::OnInteractionReceived(const TSharedPtr<FJsonObject>& DataObj)
{
	if (!DataObj.IsValid())
	{
		return;
	}

	double TypeD = 0.0;
	DataObj->TryGetNumberField(TEXT("type"), TypeD);
	const int32 InteractionType = static_cast<int32>(TypeD);

	// We handle APPLICATION_COMMAND (2), MESSAGE_COMPONENT (3), and MODAL_SUBMIT (5).
	if (InteractionType == 2)
	{
		HandleSlashTicketCommand(DataObj);
		return;
	}
	if (InteractionType != 3 && InteractionType != 5)
	{
		return;
	}

	FString InteractionId;
	FString InteractionToken;
	DataObj->TryGetStringField(TEXT("id"),    InteractionId);
	DataObj->TryGetStringField(TEXT("token"), InteractionToken);

	const TSharedPtr<FJsonObject>* InteractionDataPtr = nullptr;
	if (!DataObj->TryGetObjectField(TEXT("data"), InteractionDataPtr) || !InteractionDataPtr)
	{
		return;
	}

	FString CustomId;
	(*InteractionDataPtr)->TryGetStringField(TEXT("custom_id"), CustomId);

	// Only handle custom IDs belonging to our ticket system.
	if (!CustomId.StartsWith(TEXT("ticket_")))
	{
		return;
	}

	// Extract member info.
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	DataObj->TryGetObjectField(TEXT("member"), MemberPtr);

	FString DiscordUserId;
	FString DiscordUsername;

	if (MemberPtr && (*MemberPtr).IsValid())
	{
		const TSharedPtr<FJsonObject>* UserPtr = nullptr;
		if ((*MemberPtr)->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
		{
			(*UserPtr)->TryGetStringField(TEXT("id"), DiscordUserId);

			if (!(*UserPtr)->TryGetStringField(TEXT("global_name"), DiscordUsername)
			    || DiscordUsername.IsEmpty())
			{
				(*UserPtr)->TryGetStringField(TEXT("username"), DiscordUsername);
			}
		}

		FString Nick;
		if ((*MemberPtr)->TryGetStringField(TEXT("nick"), Nick) && !Nick.IsEmpty())
		{
			DiscordUsername = Nick;
		}
	}

	if (DiscordUsername.IsEmpty())
	{
		DiscordUsername = TEXT("Discord User");
	}

	TArray<FString> MemberRoles;
	if (MemberPtr && (*MemberPtr).IsValid())
	{
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
		{
			for (const TSharedPtr<FJsonValue>& RoleVal : *Roles)
			{
				FString RoleId;
				if (RoleVal->TryGetString(RoleId))
				{
					MemberRoles.Add(RoleId);
				}
			}
		}
	}

	FString SourceChannelId;
	DataObj->TryGetStringField(TEXT("channel_id"), SourceChannelId);

	if (InteractionType == 5) // MODAL_SUBMIT
	{
		HandleTicketModalSubmit(InteractionId, InteractionToken, CustomId,
		                        *InteractionDataPtr,
		                        DiscordUserId, DiscordUsername);
	}
	else // MESSAGE_COMPONENT (type 3)
	{
		HandleTicketButtonInteraction(InteractionId, InteractionToken, CustomId,
		                              DiscordUserId, DiscordUsername,
		                              MemberRoles, SourceChannelId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Button click handler
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::HandleTicketButtonInteraction(
	const FString& InteractionId,
	const FString& InteractionToken,
	const FString& CustomId,
	const FString& DiscordUserId,
	const FString& DiscordUsername,
	const TArray<FString>& MemberRoles,
	const FString& SourceChannelId)
{
	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Ticket button '%s' clicked by '%s' (%s)"),
	       *CustomId, *DiscordUsername, *DiscordUserId);

	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge)
	{
		return;
	}

	// ── Reopen-ticket button ──────────────────────────────────────────────────
	if (CustomId.StartsWith(TEXT("ticket_reopen:")))
	{
		const FString ChanId = CustomId.Mid(FCString::Strlen(TEXT("ticket_reopen:")));
		const FString* PendingOpener = PendingReopenOpener.Find(ChanId);
		const bool bIsOpenerReopen = PendingOpener && *PendingOpener == DiscordUserId;
		bool bIsAdminReopen = false;
		if (!Config.TicketNotifyRoleId.IsEmpty())
			bIsAdminReopen = MemberRoles.Contains(Config.TicketNotifyRoleId);

		if (!bIsOpenerReopen && !bIsAdminReopen)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, 4,
				TEXT(":no_entry: Only the ticket opener or admin can reopen this ticket."),
				true);
			return;
		}

		FString RestoredOpener;
		PendingReopenOpener.RemoveAndCopyValue(ChanId, RestoredOpener);
		PendingReopenExpiry.Remove(ChanId);

		TicketChannelToOpener.Add(ChanId, RestoredOpener);
		OpenerToTicketChannel.Add(RestoredOpener, ChanId);
		SaveTicketState();

		Bridge->RespondToInteraction(InteractionId, InteractionToken, 4,
			TEXT(":white_check_mark: Ticket reopened!"), false);
		return;
	}

	// ── Approve & Whitelist button ────────────────────────────────────────────
	if (CustomId.StartsWith(TEXT("ticket_approve_wl:")))
	{
		const FString ApproveOpenerId = CustomId.Mid(FCString::Strlen(TEXT("ticket_approve_wl:")));

		bool bIsAdminApprove = false;
		if (!Config.TicketNotifyRoleId.IsEmpty())
			bIsAdminApprove = MemberRoles.Contains(Config.TicketNotifyRoleId);
		if (!bIsAdminApprove)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, 4,
				TEXT(":no_entry: Only admins with the support role can approve whitelist requests."),
				true);
			return;
		}

		FString OpenerName;
		const FString* NamePtr = TicketChannelToOpenerName.Find(SourceChannelId);
		if (NamePtr)
		{
			OpenerName = *NamePtr;
		}

		FString ApproveResponse;
		if (!OpenerName.IsEmpty() && FWhitelistManager::AddPlayer(OpenerName, TEXT(""), DiscordUsername))
		{
			ApproveResponse = FString::Printf(
				TEXT(":white_check_mark: **%s** has been added to the whitelist!"), *OpenerName);
		}
		else if (!OpenerName.IsEmpty())
		{
			ApproveResponse = FString::Printf(
				TEXT(":yellow_circle: **%s** is already on the whitelist."), *OpenerName);
		}
		else
		{
			ApproveResponse = TEXT(":white_check_mark: Player has been whitelisted.");
		}

		Bridge->RespondToInteraction(InteractionId, InteractionToken, 4, ApproveResponse, false);

		// Close the ticket
		FString RemovedOpenerApprove;
		TicketChannelToOpener.RemoveAndCopyValue(SourceChannelId, RemovedOpenerApprove);
		if (!RemovedOpenerApprove.IsEmpty())
			OpenerToTicketChannel.Remove(RemovedOpenerApprove);
		TicketChannelToAssignee.Remove(SourceChannelId);
		TicketChannelToAssigneeName.Remove(SourceChannelId);
		TicketChannelToLastActivity.Remove(SourceChannelId);
		TicketChannelToOpenerName.Remove(SourceChannelId);
		FString RemovedTypeApprove;
		TicketChannelToType.RemoveAndCopyValue(SourceChannelId, RemovedTypeApprove);
		if (Config.bAllowMultipleTicketTypes && !RemovedOpenerApprove.IsEmpty() && !RemovedTypeApprove.IsEmpty())
		{
			if (TMap<FString, FString>* TypeMap = OpenerToTicketsByType.Find(RemovedOpenerApprove))
				TypeMap->Remove(RemovedTypeApprove);
		}
		TicketChannelToOpenTime.Remove(SourceChannelId);
		TicketChannelToPriority.Remove(SourceChannelId);
		TicketChannelToTags.Remove(SourceChannelId);
		TicketChannelToNotes.Remove(SourceChannelId);
		TicketChannelStaffReplied.Remove(SourceChannelId);
		TicketChannelToReminder.Remove(SourceChannelId);
		SaveTicketState();

		Bridge->DeleteDiscordChannel(SourceChannelId);
		return;
	}

	// ── Ticket feedback rating ────────────────────────────────────────────────
	if (CustomId.StartsWith(TEXT("ticket_fb:")))
	{
		const int32 Rating = FCString::Atoi(*CustomId.Mid(FCString::Strlen(TEXT("ticket_fb:"))));
		if (Rating >= 1 && Rating <= 5)
		{
			const FString StatsPath = GetStatsFilePath();
			FString StatsJson;
			TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
			if (FFileHelper::LoadFileToString(StatsJson, *StatsPath))
			{
				TSharedRef<TJsonReader<>> SR = TJsonReaderFactory<>::Create(StatsJson);
				TSharedPtr<FJsonObject> Loaded;
				if (FJsonSerializer::Deserialize(SR, Loaded) && Loaded.IsValid())
					Stats = Loaded;
			}
			const TSharedPtr<FJsonObject>* RatingsPtr = nullptr;
			TSharedPtr<FJsonObject> Ratings;
			if (Stats->TryGetObjectField(TEXT("ratings"), RatingsPtr) && RatingsPtr)
				Ratings = *RatingsPtr;
			else
				Ratings = MakeShared<FJsonObject>();

			const FString RatingKey = FString::FromInt(Rating);
			double OldRating = 0.0;
			Ratings->TryGetNumberField(RatingKey, OldRating);
			Ratings->SetNumberField(RatingKey, OldRating + 1.0);
			Stats->SetObjectField(TEXT("ratings"), Ratings);

			double TotalRatings = 0.0, RatingSum = 0.0;
			Stats->TryGetNumberField(TEXT("total_ratings"), TotalRatings);
			Stats->TryGetNumberField(TEXT("rating_sum"), RatingSum);
			Stats->SetNumberField(TEXT("total_ratings"), TotalRatings + 1.0);
			Stats->SetNumberField(TEXT("rating_sum"), RatingSum + Rating);

			FString OutStats;
			TSharedRef<TJsonWriter<>> SW = TJsonWriterFactory<>::Create(&OutStats);
			FJsonSerializer::Serialize(Stats.ToSharedRef(), SW);
			FPlatformFileManager::Get().GetPlatformFile().CreateDirectoryTree(*FPaths::GetPath(StatsPath));
			FFileHelper::SaveStringToFile(OutStats, *StatsPath,
				FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
		}

		Bridge->RespondToInteraction(InteractionId, InteractionToken, 4,
			TEXT(":white_check_mark: Thank you for your feedback!"), true);
		return;
	}

	// ── Close-ticket button ───────────────────────────────────────────────────
	if (CustomId.StartsWith(TEXT("ticket_close:")))
	{
		const FString OpenerUserId =
			CustomId.Mid(FCString::Strlen(TEXT("ticket_close:")));

		if (!TicketChannelToOpener.Contains(SourceChannelId))
		{
			// Channel may have been lost on restart; still allow closure.
			UE_LOG(LogTicketSystem, Warning,
			       TEXT("TicketSystem: Close button in untracked channel %s – proceeding with deletion."),
			       *SourceChannelId);
		}

		// Authorisation: opener OR admin role OR guild owner.
		const bool bIsOpener = (!DiscordUserId.IsEmpty() && DiscordUserId == OpenerUserId);
		bool bIsAdmin = (!Bridge->GetGuildOwnerId().IsEmpty() &&
		                  DiscordUserId == Bridge->GetGuildOwnerId());
		if (!bIsAdmin && !Config.TicketNotifyRoleId.IsEmpty())
		{
			bIsAdmin = MemberRoles.Contains(Config.TicketNotifyRoleId);
		}

		if (!bIsOpener && !bIsAdmin)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken,
				/*type=*/4,
				TEXT(":no_entry: Only the ticket opener or an admin with the support "
				     "role can close this ticket."),
				/*bEphemeral=*/true);
			return;
		}

		// Ack silently (type 6) before deleting the channel so Discord does not
		// show an error after the channel disappears.
		Bridge->RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/6, TEXT(""), /*bEphemeral=*/false);

		// Capture notes for transcript before removing from maps.
		TArray<FString> CloseNotesForLog;
		if (const TArray<FString>* CloseNotesPtr = TicketChannelToNotes.Find(SourceChannelId))
			CloseNotesForLog = *CloseNotesPtr;

		// Remove from tracking and delete.
		FString RemovedOpener;
		if (TicketChannelToOpener.RemoveAndCopyValue(SourceChannelId, RemovedOpener))
		{
			OpenerToTicketChannel.Remove(RemovedOpener);
		}
		TicketChannelToAssignee.Remove(SourceChannelId);
		TicketChannelToAssigneeName.Remove(SourceChannelId);
		TicketChannelToLastActivity.Remove(SourceChannelId);
		TicketChannelToOpenerName.Remove(SourceChannelId);
		FString RemovedType;
		TicketChannelToType.RemoveAndCopyValue(SourceChannelId, RemovedType);
		TicketChannelToOpenTime.Remove(SourceChannelId);
		TicketChannelToPriority.Remove(SourceChannelId);
		TicketChannelToTags.Remove(SourceChannelId);
		TicketChannelToNotes.Remove(SourceChannelId);
		TicketChannelStaffReplied.Remove(SourceChannelId);
		TicketChannelToReminder.Remove(SourceChannelId);
		if (Config.bAllowMultipleTicketTypes && !RemovedOpener.IsEmpty() && !RemovedType.IsEmpty())
		{
			if (TMap<FString, FString>* TypeMap = OpenerToTicketsByType.Find(RemovedOpener))
			{
				TypeMap->Remove(RemovedType);
			}
		}

		// Apply cooldown
		if (Config.TicketCooldownMinutes > 0 && !RemovedOpener.IsEmpty())
		{
			UserTicketCooldown.Add(RemovedOpener,
				FDateTime::UtcNow() + FTimespan::FromMinutes(Config.TicketCooldownMinutes));
		}

		// Persist the updated state so this closure survives a server restart.
		SaveTicketState();

		// If this was a ban-appeal ticket, dismiss the pending registry entry so
		// it doesn't accumulate as "pending" after the ticket is closed without
		// a formal approve/deny decision.
		if (RemovedType == TEXT("banappeal") && !RemovedOpener.IsEmpty())
		{
			if (UGameInstance* GI = GetGameInstance())
			{
				if (UBanAppealRegistry* AppealReg = GI->GetSubsystem<UBanAppealRegistry>())
				{
					const FString AppealUid = FString::Printf(TEXT("Discord:%s"), *RemovedOpener);
					for (const FBanAppealEntry& E : AppealReg->GetAllAppeals())
					{
						if (E.Uid == AppealUid && E.Status == EAppealStatus::Pending)
						{
							AppealReg->DeleteAppeal(E.Id);
							OpenerToAppealId.Remove(RemovedOpener);
							UE_LOG(LogTicketSystem, Log,
							       TEXT("TicketSystem: Auto-dismissed pending appeal id=%lld for Discord user %s on ticket close."),
							       E.Id, *RemovedOpener);
							break;
						}
					}
				}
			}
		}

		// Reopen grace period: if configured, don't delete immediately
		if (Config.TicketReopenGracePeriodMinutes > 0)
		{
			PendingReopenExpiry.Add(SourceChannelId,
				FDateTime::UtcNow() + FTimespan::FromMinutes(Config.TicketReopenGracePeriodMinutes));
			if (!RemovedOpener.IsEmpty())
			{
				PendingReopenOpener.Add(SourceChannelId, RemovedOpener);
			}

			const FString GraceMsg = FString::Printf(
				TEXT(":hourglass: This ticket will be closed in **%d minutes**. "
				     "Click Reopen to keep it open."),
				Config.TicketReopenGracePeriodMinutes);

			TSharedPtr<FJsonObject> ReopenBtn = MakeShared<FJsonObject>();
			ReopenBtn->SetNumberField(TEXT("type"), 2);
			ReopenBtn->SetNumberField(TEXT("style"), 3);
			ReopenBtn->SetStringField(TEXT("label"), TEXT("Reopen"));
			ReopenBtn->SetStringField(TEXT("custom_id"),
				FString::Printf(TEXT("ticket_reopen:%s"), *SourceChannelId));

			TSharedPtr<FJsonObject> ReopenRow = MakeShared<FJsonObject>();
			ReopenRow->SetNumberField(TEXT("type"), 1);
			ReopenRow->SetArrayField(TEXT("components"),
				TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueObject>(ReopenBtn)});

			TSharedPtr<FJsonObject> GraceBody = MakeShared<FJsonObject>();
			GraceBody->SetStringField(TEXT("content"), GraceMsg);
			GraceBody->SetArrayField(TEXT("components"),
				TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueObject>(ReopenRow)});

			Bridge->SendMessageBodyToChannel(SourceChannelId, GraceBody);
			return; // don't delete yet
		}

		// ── Transcript archiving ──────────────────────────────────────────────
		// Fetch message history and post to TicketLogChannelId before deleting.
		if (!Config.TicketLogChannelId.IsEmpty() && !Bridge->GetBotToken().IsEmpty())
		{
			const FString LogChannelId     = Config.TicketLogChannelId;
			const FString BotToken         = Bridge->GetBotToken();
			const FString ClosedChannelId  = SourceChannelId;
			const FString OpenerIdForLog   = RemovedOpener.IsEmpty() ? OpenerUserId : RemovedOpener;
			const FString ClosedAt         = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d"));

			TWeakObjectPtr<UTicketSubsystem> WeakThis(this);

			const FString FetchUrl = FString::Printf(
				TEXT("https://discord.com/api/v10/channels/%s/messages?limit=100"),
				*ClosedChannelId);

			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FetchReq =
				FHttpModule::Get().CreateRequest();
			FetchReq->SetURL(FetchUrl);
			FetchReq->SetVerb(TEXT("GET"));
			FetchReq->SetHeader(TEXT("Authorization"),
				FString::Printf(TEXT("Bot %s"), *BotToken));

			FetchReq->OnProcessRequestComplete().BindWeakLambda(
				WeakThis.Get(),
				[WeakThis, LogChannelId, BotToken, OpenerIdForLog, ClosedAt, ClosedChannelId, CloseNotesForLog]
				(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bOk) mutable
				{
					UTicketSubsystem* Self = WeakThis.Get();
					if (!Self) return;
					IDiscordBridgeProvider* B = Self->GetBridge();
					if (!B) return;

					FString TranscriptText =
						FString::Printf(
							TEXT(":scroll: **Ticket Closed**\n")
							TEXT("Opener: <@%s>\n")
							TEXT("Date closed: %s\n\n"),
							*OpenerIdForLog, *ClosedAt);

					if (bOk && Resp.IsValid() &&
					    Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
					{
						TSharedPtr<FJsonObject> Ignored;
						TArray<TSharedPtr<FJsonValue>> Messages;
						TSharedRef<TJsonReader<>> Reader =
							TJsonReaderFactory<>::Create(Resp->GetContentAsString());

						// Messages come newest-first; reverse for chronological order.
						TSharedPtr<FJsonValue> Root;
						if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
						{
							const TArray<TSharedPtr<FJsonValue>>* MsgArr = nullptr;
							if (Root->TryGetArray(MsgArr) && MsgArr)
							{
								// Reverse to get chronological order (oldest first).
								for (int32 Idx = MsgArr->Num() - 1; Idx >= 0; --Idx)
								{
									const TSharedPtr<FJsonObject>* MsgObjPtr = nullptr;
									if (!(*MsgArr)[Idx]->TryGetObject(MsgObjPtr) || !MsgObjPtr)
										continue;

									FString AuthorName;
									const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
									if ((*MsgObjPtr)->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
									{
										if (!(*AuthorPtr)->TryGetStringField(TEXT("global_name"), AuthorName) || AuthorName.IsEmpty())
											(*AuthorPtr)->TryGetStringField(TEXT("username"), AuthorName);
									}
									FString MsgContent;
									(*MsgObjPtr)->TryGetStringField(TEXT("content"), MsgContent);

									if (!AuthorName.IsEmpty() && !MsgContent.IsEmpty())
									{
										TranscriptText += FString::Printf(
											TEXT("[%s]: %s\n"), *AuthorName, *MsgContent);
									}
								}
							}
						}
					}
					else
					{
						TranscriptText += TEXT("*(Could not fetch message history.)*\n");
					}

					// Append staff notes (internal) if any.
					if (!CloseNotesForLog.IsEmpty())
					{
						TranscriptText += TEXT("\n**Staff Notes (internal):**\n");
						for (const FString& N : CloseNotesForLog)
							TranscriptText += FString::Printf(TEXT("- %s\n"), *N);
					}

					// Discord messages have a 2000-char limit; truncate if necessary.
					if (TranscriptText.Len() > 1900)
					{
						TranscriptText = TranscriptText.Left(1900) + TEXT("\n*(transcript truncated)*");
					}

					B->SendDiscordChannelMessage(LogChannelId, TranscriptText);
				});

			FetchReq->ProcessRequest();

			// Delete the channel AFTER queuing the fetch; the HTTP response is
			// async so we cannot wait for it here.  The fetch finishes quickly
			// (typically < 1 s) and the log channel post follows independently.
			Bridge->DeleteDiscordChannel(SourceChannelId);
		}
		else
		{
			Bridge->DeleteDiscordChannel(SourceChannelId);
		}
		return;
	}

	// ── Blacklist check ───────────────────────────────────────────────────────
	// Applied before opening any ticket type.
	if (!DiscordUserId.IsEmpty() && TicketBlacklist.Contains(DiscordUserId))
	{
		Bridge->RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			TEXT(":no_entry: You are not allowed to open tickets. "
			     "Contact an admin if you believe this is an error."),
			/*bEphemeral=*/true);
		return;
	}

	// ── Prevent duplicate tickets ─────────────────────────────────────────────
	if (OpenerToTicketChannel.Contains(DiscordUserId))
	{
		const FString ExistingChanId = OpenerToTicketChannel[DiscordUserId];
		Bridge->RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			FString::Printf(
				TEXT(":warning: You already have an open ticket (<#%s>). "
				     "Please continue there, or close it before opening a new one."),
				*ExistingChanId),
			/*bEphemeral=*/true);
		return;
	}

	// ── Open-ticket buttons ───────────────────────────────────────────────────
	if (CustomId == TEXT("ticket_wl"))
	{
		if (!Config.bTicketWhitelistEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Whitelist request tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		ShowTicketReasonModal(InteractionId, InteractionToken,
			TEXT("ticket_modal:wl"),
			TEXT("Whitelist Request"),
			TEXT("Describe why you want to join (optional)"));
	}
	else if (CustomId == TEXT("ticket_help"))
	{
		if (!Config.bTicketHelpEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Help tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		ShowTicketReasonModal(InteractionId, InteractionToken,
			TEXT("ticket_modal:help"),
			TEXT("Help / Support"),
			TEXT("Briefly describe your issue (optional)"));
	}
	else if (CustomId == TEXT("ticket_report"))
	{
		if (!Config.bTicketReportEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Player report tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		ShowTicketReasonModal(InteractionId, InteractionToken,
			TEXT("ticket_modal:report"),
			TEXT("Report a Player"),
			TEXT("Describe the incident and the player you are reporting (optional)"));
	}
	else if (CustomId == TEXT("ticket_appeal"))
	{
		if (!Config.bTicketBanAppealEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Ban appeal tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		ShowTicketReasonModal(InteractionId, InteractionToken,
			TEXT("ticket_modal:appeal"),
			TEXT("Ban Appeal"),
			TEXT("State your in-game name and explain why your ban should be removed"));
	}
	else if (CustomId.StartsWith(TEXT("ticket_cr_")))
	{
		const int32 ReasonIndex =
			FCString::Atoi(*CustomId.Mid(FCString::Strlen(TEXT("ticket_cr_"))));

		if (!Config.CustomTicketReasons.IsValidIndex(ReasonIndex))
		{
			UE_LOG(LogTicketSystem, Warning,
			       TEXT("TicketSystem: Custom ticket reason index %d out of range (have %d)."),
			       ReasonIndex, Config.CustomTicketReasons.Num());
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: This ticket type is no longer available. "
				     "Please contact an admin directly."),
				/*bEphemeral=*/true);
			return;
		}

		FString Label, Desc;
		Config.CustomTicketReasons[ReasonIndex].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			Label = TEXT("Custom");
		}

		ShowTicketReasonModal(InteractionId, InteractionToken,
			FString::Printf(TEXT("ticket_modal:cr_%d"), ReasonIndex),
			Label.Left(45),
			TEXT("Provide any details about your request (optional)"));
	}
	else
	{
		UE_LOG(LogTicketSystem, Warning,
		       TEXT("TicketSystem: Unrecognised ticket button custom_id: %s"), *CustomId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Show a ticket reason modal (popup form) before creating the ticket channel
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::ShowTicketReasonModal(
	const FString& InteractionId,
	const FString& InteractionToken,
	const FString& ModalCustomId,
	const FString& Title,
	const FString& Placeholder)
{
	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	Bridge->RespondWithModal(InteractionId, InteractionToken, ModalCustomId, Title, Placeholder);
}

// ─────────────────────────────────────────────────────────────────────────────
// Handle MODAL_SUBMIT for ticket reason modal
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::HandleTicketModalSubmit(
	const FString& InteractionId,
	const FString& InteractionToken,
	const FString& ModalCustomId,
	const TSharedPtr<FJsonObject>& ModalData,
	const FString& DiscordUserId,
	const FString& DiscordUsername)
{
	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Ticket modal '%s' submitted by '%s' (%s)"),
	       *ModalCustomId, *DiscordUsername, *DiscordUserId);

	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge)
	{
		return;
	}

	// Extract the user-supplied reason.
	FString Reason;
	const TArray<TSharedPtr<FJsonValue>>* Rows = nullptr;
	if (ModalData->TryGetArrayField(TEXT("components"), Rows) && Rows)
	{
		for (const TSharedPtr<FJsonValue>& RowVal : *Rows)
		{
			const TSharedPtr<FJsonObject>* RowObj = nullptr;
			if (!RowVal->TryGetObject(RowObj) || !RowObj) { continue; }

			const TArray<TSharedPtr<FJsonValue>>* Inputs = nullptr;
			if (!(*RowObj)->TryGetArrayField(TEXT("components"), Inputs) || !Inputs) { continue; }

			for (const TSharedPtr<FJsonValue>& InputVal : *Inputs)
			{
				const TSharedPtr<FJsonObject>* InputObj = nullptr;
				if (!InputVal->TryGetObject(InputObj) || !InputObj) { continue; }

				FString InputCustomId;
				(*InputObj)->TryGetStringField(TEXT("custom_id"), InputCustomId);
				if (InputCustomId == TEXT("ticket_reason"))
				{
					(*InputObj)->TryGetStringField(TEXT("value"), Reason);
					Reason.TrimStartAndEndInline();
				}
			}
		}
	}

	// Prevent duplicate tickets.
	if (OpenerToTicketChannel.Contains(DiscordUserId))
	{
		const FString ExistingChanId = OpenerToTicketChannel[DiscordUserId];
		Bridge->RespondToInteraction(InteractionId, InteractionToken,
			/*type=*/4,
			FString::Printf(
				TEXT(":warning: You already have an open ticket (<#%s>). "
				     "Please continue there, or close it before opening a new one."),
				*ExistingChanId),
			/*bEphemeral=*/true);
		return;
	}

	if (ModalCustomId == TEXT("ticket_modal:wl"))
	{
		if (!Config.bTicketWhitelistEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Whitelist request tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
			TEXT(":white_check_mark: Opening your whitelist request ticket…  "
			     "A private channel will appear shortly."),
			/*bEphemeral=*/true);
		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("whitelist"), Reason,
		                    TEXT(""), TEXT(""));
	}
	else if (ModalCustomId == TEXT("ticket_modal:help"))
	{
		if (!Config.bTicketHelpEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Help tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
			TEXT(":white_check_mark: Opening your help ticket…  "
			     "A private channel will appear shortly."),
			/*bEphemeral=*/true);
		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("help"), Reason,
		                    TEXT(""), TEXT(""));
	}
	else if (ModalCustomId == TEXT("ticket_modal:report"))
	{
		if (!Config.bTicketReportEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Player report tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}
		Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
			TEXT(":white_check_mark: Opening your report ticket…  "
			     "A private channel will appear shortly."),
			/*bEphemeral=*/true);
		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("report"), Reason,
		                    TEXT(""), TEXT(""));
	}
	else if (ModalCustomId == TEXT("ticket_modal:appeal"))
	{
		if (!Config.bTicketBanAppealEnabled)
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: Ban appeal tickets are currently disabled."),
				/*bEphemeral=*/true);
			return;
		}

		// Save to BanAppealRegistry if BanSystem is installed.
		// Use "Discord:<userId>" as the appeal UID so the registry can persist
		// and retrieve the entry.  Admins can resolve the EOS UID from the
		// in-game name the player provides in the ticket channel.
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UBanAppealRegistry* AppealReg = GI->GetSubsystem<UBanAppealRegistry>())
			{
				const FString AppealUid     = FString::Printf(TEXT("Discord:%s"), *DiscordUserId);
				const FString ContactInfo   = FString::Printf(TEXT("Discord: %s (%s)"), *DiscordUsername, *DiscordUserId);
				const FBanAppealEntry NewEntry = AppealReg->AddAppeal(AppealUid, Reason, ContactInfo);
				if (NewEntry.Id > 0)
				{
					// Keep the appeal ID mapped to this opener so approve/deny can
					// look it up without scanning the full registry.
					OpenerToAppealId.Add(DiscordUserId, NewEntry.Id);
				}

				UE_LOG(LogTicketSystem, Log,
				       TEXT("TicketSystem: Ban appeal from Discord user '%s' (%s) saved to BanAppealRegistry (id=%lld)."),
				       *DiscordUsername, *DiscordUserId, NewEntry.Id);
			}
		}

		Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
			TEXT(":white_check_mark: Opening your ban appeal ticket…  "
			     "A private channel will appear shortly."),
			/*bEphemeral=*/true);
		CreateTicketChannel(DiscordUserId, DiscordUsername, TEXT("banappeal"), Reason,
		                    TEXT(""), TEXT(""));
	}
	else if (ModalCustomId.StartsWith(TEXT("ticket_modal:cr_")))
	{
		const int32 ReasonIndex =
			FCString::Atoi(*ModalCustomId.Mid(FCString::Strlen(TEXT("ticket_modal:cr_"))));

		if (!Config.CustomTicketReasons.IsValidIndex(ReasonIndex))
		{
			Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
				TEXT(":no_entry: This ticket type is no longer available. "
				     "Please contact an admin directly."),
				/*bEphemeral=*/true);
			return;
		}

		FString Label, Desc;
		Config.CustomTicketReasons[ReasonIndex].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty())
		{
			Label = TEXT("Custom");
		}

		const FString TypeSlug = SanitizeUsernameForChannel(Label);

		Bridge->RespondToInteraction(InteractionId, InteractionToken, /*type=*/4,
			FString::Printf(
				TEXT(":white_check_mark: Opening your **%s** ticket…  "
				     "A private channel will appear shortly."),
				*Label),
			/*bEphemeral=*/true);
		CreateTicketChannel(DiscordUserId, DiscordUsername, TypeSlug, Reason, Label, Desc);
	}
	else
	{
		UE_LOG(LogTicketSystem, Warning,
		       TEXT("TicketSystem: Unrecognised ticket modal custom_id: %s"), *ModalCustomId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Create a private ticket channel in the guild
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::CreateTicketChannel(
	const FString& OpenerUserId,
	const FString& OpenerUsername,
	const FString& TicketType,
	const FString& ExtraInfo,
	const FString& DisplayLabel,
	const FString& DisplayDesc)
{
	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge)
	{
		return;
	}

	const FString ChannelName = FString::Printf(TEXT("ticket-%s-%s"),
		*TicketType, *SanitizeUsernameForChannel(OpenerUsername));

	// ── Permission overwrites ─────────────────────────────────────────────────
	// VIEW_CHANNEL (1024), SEND_MESSAGES (2048), READ_MESSAGE_HISTORY (65536)
	// Combined allow: 68608

	TArray<TSharedPtr<FJsonValue>> Overwrites;

	// 1. Deny VIEW_CHANNEL for @everyone (role ID == guild ID).
	{
		TSharedPtr<FJsonObject> EveryoneDeny = MakeShared<FJsonObject>();
		EveryoneDeny->SetStringField(TEXT("id"),    Bridge->GetGuildId());
		EveryoneDeny->SetNumberField(TEXT("type"),  0);   // role
		EveryoneDeny->SetStringField(TEXT("allow"), TEXT("0"));
		EveryoneDeny->SetStringField(TEXT("deny"),  TEXT("1024")); // VIEW_CHANNEL
		Overwrites.Add(MakeShared<FJsonValueObject>(EveryoneDeny));
	}

	// 2. Allow the admin/support role (TicketNotifyRoleId).
	if (!Config.TicketNotifyRoleId.IsEmpty())
	{
		TSharedPtr<FJsonObject> AdminAllow = MakeShared<FJsonObject>();
		AdminAllow->SetStringField(TEXT("id"),    Config.TicketNotifyRoleId);
		AdminAllow->SetNumberField(TEXT("type"),  0);   // role
		AdminAllow->SetStringField(TEXT("allow"), TEXT("68608")); // VIEW|SEND|HISTORY
		AdminAllow->SetStringField(TEXT("deny"),  TEXT("0"));
		Overwrites.Add(MakeShared<FJsonValueObject>(AdminAllow));
	}

	// 3. Allow the ticket opener (by Discord user ID).
	if (!OpenerUserId.IsEmpty())
	{
		TSharedPtr<FJsonObject> UserAllow = MakeShared<FJsonObject>();
		UserAllow->SetStringField(TEXT("id"),    OpenerUserId);
		UserAllow->SetNumberField(TEXT("type"),  1);   // member
		UserAllow->SetStringField(TEXT("allow"), TEXT("68608")); // VIEW|SEND|HISTORY
		UserAllow->SetStringField(TEXT("deny"),  TEXT("0"));
		Overwrites.Add(MakeShared<FJsonValueObject>(UserAllow));
	}

	// Capture everything the async callback needs.
	const FString NotifyRoleIdCopy   = Config.TicketNotifyRoleId;
	const FString TicketChannelCopy  = Config.TicketChannelId;
	const FString OpenerUserIdCopy   = OpenerUserId;
	const FString OpenerUsernameCopy = OpenerUsername;
	const FString TicketTypeCopy     = TicketType;
	const FString ExtraInfoCopy      = ExtraInfo;
	const FString DisplayLabelCopy   = DisplayLabel;
	const FString DisplayDescCopy    = DisplayDesc;

	// Select per-type category
	FString CategoryIdCopy;
	if (TicketType == TEXT("whitelist"))
		CategoryIdCopy = Config.WhitelistCategoryId.IsEmpty() ? Config.TicketCategoryId : Config.WhitelistCategoryId;
	else if (TicketType == TEXT("help"))
		CategoryIdCopy = Config.HelpCategoryId.IsEmpty() ? Config.TicketCategoryId : Config.HelpCategoryId;
	else if (TicketType == TEXT("report"))
		CategoryIdCopy = Config.ReportCategoryId.IsEmpty() ? Config.TicketCategoryId : Config.ReportCategoryId;
	else if (TicketType == TEXT("banappeal"))
		CategoryIdCopy = Config.AppealCategoryId.IsEmpty() ? Config.TicketCategoryId : Config.AppealCategoryId;
	else
		CategoryIdCopy = Config.TicketCategoryId;

	// Capture a weak pointer so we never dereference a GC'd subsystem if the
	// HTTP response arrives after UTicketSubsystem has been destroyed.
	TWeakObjectPtr<UTicketSubsystem> WeakThis(this);
	Bridge->CreateDiscordGuildTextChannel(
		ChannelName,
		CategoryIdCopy,
		Overwrites,
		[WeakThis, NotifyRoleIdCopy, TicketChannelCopy,
		 OpenerUserIdCopy, OpenerUsernameCopy, TicketTypeCopy,
		 ExtraInfoCopy, DisplayLabelCopy, DisplayDescCopy, CategoryIdCopy]
		(const FString& NewChannelId)
		{
			UTicketSubsystem* Self = WeakThis.Get();
			if (!Self) return;

			if (NewChannelId.IsEmpty())
			{
				UE_LOG(LogTicketSystem, Warning,
				       TEXT("TicketSystem: CreateTicketChannel failed for user '%s'."),
				       *OpenerUsernameCopy);
				return;
			}

			UE_LOG(LogTicketSystem, Log,
			       TEXT("TicketSystem: Created ticket channel %s for user '%s' (%s)."),
			       *NewChannelId, *OpenerUsernameCopy, *OpenerUserIdCopy);

			// Track the channel.
			Self->TicketChannelToOpener.Add(NewChannelId, OpenerUserIdCopy);
			Self->OpenerToTicketChannel.Add(OpenerUserIdCopy, NewChannelId);
			// Initialise last-activity to now so the inactive-timeout
			// does not immediately fire on freshly-opened tickets.
			Self->TicketChannelToLastActivity.Add(NewChannelId, FDateTime::UtcNow());
			// Persist the updated state so this ticket survives a server restart.
			Self->SaveTicketState();
			Self->TicketChannelToOpenerName.Add(NewChannelId, OpenerUsernameCopy);
			Self->TicketChannelToOpenTime.Add(NewChannelId, FDateTime::UtcNow());
			Self->TicketChannelToType.Add(NewChannelId, TicketTypeCopy);
			Self->TicketChannelStaffReplied.Add(NewChannelId, false);
			if (Self->Config.bAllowMultipleTicketTypes)
			{
				Self->OpenerToTicketsByType.FindOrAdd(OpenerUserIdCopy).Add(TicketTypeCopy, NewChannelId);
			}

			// ── Build the welcome message ─────────────────────────────────────
			const FString MentionPrefix = NotifyRoleIdCopy.IsEmpty()
				? TEXT("")
				: FString::Printf(TEXT("<@&%s> "), *NotifyRoleIdCopy);
			const FString UserMention = OpenerUserIdCopy.IsEmpty()
				? FString::Printf(TEXT("**%s**"), *OpenerUsernameCopy)
				: FString::Printf(TEXT("<@%s>"), *OpenerUserIdCopy);

			FString WelcomeContent;
			if (TicketTypeCopy == TEXT("whitelist"))
			{
				WelcomeContent = FString::Printf(
					TEXT("%s:ticket: **Whitelist Request** from %s\n\n"
					     "Please tell us your **in-game name** so we can add you.\n"
					     "Feel free to share any additional information here.\n\n"
					     ":information_source: An admin will review your request shortly."),
					*MentionPrefix, *UserMention);
			}
			else if (TicketTypeCopy == TEXT("help"))
			{
				WelcomeContent = FString::Printf(
					TEXT("%s:ticket: **Help / Support Request** from %s\n\n"
					     "Please describe your issue in as much detail as possible.\n\n"
					     ":information_source: An admin will respond here shortly."),
					*MentionPrefix, *UserMention);
			}
			else if (TicketTypeCopy == TEXT("report"))
			{
				WelcomeContent = FString::Printf(
					TEXT("%s:ticket: **Player Report** from %s\n\n"
					     "Please provide:\n"
					     "- The **in-game name** of the player you are reporting\n"
					     "- A **description** of the issue or incident\n"
					     "- Any **screenshots** or evidence if available\n\n"
					     ":information_source: An admin will review the report here."),
					*MentionPrefix, *UserMention);
			}
			else if (TicketTypeCopy == TEXT("banappeal"))
			{
				WelcomeContent = FString::Printf(
					TEXT("%s:scales: **Ban Appeal** from %s\n\n"
					     "To help admins review your case, please provide:\n"
					     "- Your **in-game name** (as it appeared when you were banned)\n"
					     "- Your **EOS Player UID** – run `/whoami` in-game if you still have access, "
					       "or look for it in your ban notice\n"
					     "- A **reason** why the ban should be lifted\n\n"
					     ":information_source: An admin will review your appeal here. "
					       "Please be patient; appeals are handled in the order they are received."),
					*MentionPrefix, *UserMention);
			}
			else
			{
				const FString LabelDisplay =
					DisplayLabelCopy.IsEmpty() ? TicketTypeCopy : DisplayLabelCopy;
				WelcomeContent = FString::Printf(
					TEXT("%s:ticket: **%s** from %s\n\n"),
					*MentionPrefix, *LabelDisplay, *UserMention);
				if (!DisplayDescCopy.IsEmpty())
				{
					WelcomeContent += DisplayDescCopy + TEXT("\n\n");
				}
				WelcomeContent += TEXT(":information_source: An admin will be with you shortly. "
				                       "Please describe your request here.");
			}

			if (!ExtraInfoCopy.IsEmpty())
			{
				WelcomeContent += FString::Printf(
					TEXT("\n\n**Details provided:** %s"), *ExtraInfoCopy);
			}

			// Post the welcome message (plain text).
			if (IDiscordBridgeProvider* B = Self->GetBridge())
			{
				TSharedPtr<FJsonObject> WelcomeMsg = MakeShared<FJsonObject>();
				WelcomeMsg->SetStringField(TEXT("content"), WelcomeContent);
				B->SendMessageBodyToChannel(NewChannelId, WelcomeMsg);

				// Post the close-ticket button.
				B->SendMessageBodyToChannel(NewChannelId,
					Self->MakeCloseButtonMessage(OpenerUserIdCopy));

				// Approve & Whitelist button for whitelist tickets
				if (TicketTypeCopy == TEXT("whitelist"))
				{
					TSharedPtr<FJsonObject> ApproveBtn = MakeShared<FJsonObject>();
					ApproveBtn->SetNumberField(TEXT("type"), 2);
					ApproveBtn->SetNumberField(TEXT("style"), 3); // SUCCESS
					ApproveBtn->SetStringField(TEXT("label"), TEXT("Approve & Whitelist"));
					ApproveBtn->SetStringField(TEXT("custom_id"),
						FString::Printf(TEXT("ticket_approve_wl:%s"), *OpenerUserIdCopy));

					TSharedPtr<FJsonObject> ApproveRow = MakeShared<FJsonObject>();
					ApproveRow->SetNumberField(TEXT("type"), 1);
					ApproveRow->SetArrayField(TEXT("components"),
						TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueObject>(ApproveBtn)});

					TSharedPtr<FJsonObject> ApproveBody = MakeShared<FJsonObject>();
					ApproveBody->SetStringField(TEXT("content"),
						TEXT(":shield: **Approve Whitelist Request**\n"
						     "Click to add the opener to the whitelist and close this ticket."));
					ApproveBody->SetArrayField(TEXT("components"),
						TArray<TSharedPtr<FJsonValue>>{MakeShared<FJsonValueObject>(ApproveRow)});

					IDiscordBridgeProvider* B2 = Self->GetBridge();
					if (B2) B2->SendMessageBodyToChannel(NewChannelId, ApproveBody);
				}

				// Notify EVERY configured admin/ticket channel (not just the first).
				const TArray<FString> TicketChans =
					FTicketConfig::ParseChannelIds(TicketChannelCopy);
				const FString NoticeTypeName = DisplayLabelCopy.IsEmpty()
					? TicketTypeCopy : DisplayLabelCopy;
				const FString AdminNotice = FString::Printf(
					TEXT("%s:new: New **%s** ticket from %s: <#%s>"),
					NotifyRoleIdCopy.IsEmpty()
						? TEXT("")
						: *FString::Printf(TEXT("<@&%s> "), *NotifyRoleIdCopy),
					*NoticeTypeName, *UserMention, *NewChannelId);

				for (const FString& AdminChanId : TicketChans)
				{
					if (!AdminChanId.IsEmpty() && AdminChanId != NewChannelId)
					{
						TSharedPtr<FJsonObject> NoticeMsg = MakeShared<FJsonObject>();
						NoticeMsg->SetStringField(TEXT("content"), AdminNotice);
						B->SendMessageBodyToChannel(AdminChanId, NoticeMsg);
					}
				}

				// Post auto-response if configured for this ticket type.
				for (const FString& AR : Self->Config.TicketAutoResponses)
				{
					FString ARType, ARText;
					AR.Split(TEXT("|"), &ARType, &ARText);
					ARType.TrimStartAndEndInline();
					ARText.TrimStartAndEndInline();
					if (ARType.Equals(TicketTypeCopy, ESearchCase::IgnoreCase) && !ARText.IsEmpty())
					{
						B->SendDiscordChannelMessage(NewChannelId, ARText);
						break;
					}
				}
			}
		});
}

// ─────────────────────────────────────────────────────────────────────────────
// Post the ticket panel message
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::PostTicketPanel(
	const FString& PanelChannelId,
	const FString& ResponseChannelId)
{
	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge)
	{
		return;
	}

	if (Bridge->GetBotToken().IsEmpty() || PanelChannelId.IsEmpty())
	{
		if (!ResponseChannelId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(ResponseChannelId,
				TEXT(":warning: Cannot post ticket panel: bot token or panel channel ID "
				     "is not configured."));
		}
		return;
	}

	// Panel content text.
	FString PanelContent =
		TEXT(":ticket: **Support Tickets**\n"
		     "Click one of the buttons below to open a support ticket. "
		     "A private channel will be created that only you and the support team can see.\n\n");

	if (Config.bTicketWhitelistEnabled)
	{
		PanelContent += TEXT(":clipboard: **Whitelist Request** – request to be added to the server whitelist\n");
	}
	if (Config.bTicketHelpEnabled)
	{
		PanelContent += TEXT(":question: **Help / Support** – ask for help or report a problem\n");
	}
	if (Config.bTicketReportEnabled)
	{
		PanelContent += TEXT(":warning: **Report a Player** – report another player to the admins\n");
	}
	if (Config.bTicketBanAppealEnabled)
	{
		PanelContent += TEXT(":scales: **Ban Appeal** – appeal a ban issued on this server\n");
	}
	for (int32 i = 0; i < Config.CustomTicketReasons.Num(); ++i)
	{
		FString Label, Desc;
		Config.CustomTicketReasons[i].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		Desc.TrimStartAndEndInline();
		if (Label.IsEmpty()) { continue; }
		PanelContent += FString::Printf(TEXT(":small_blue_diamond: **%s**"), *Label);
		if (!Desc.IsEmpty())
		{
			PanelContent += TEXT(" – ") + Desc;
		}
		PanelContent += TEXT("\n");
	}

	// Build all buttons.
	TArray<TSharedPtr<FJsonValue>> AllButtons;

	auto AddButton = [&](const FString& Label, const FString& CustomId, int32 Style)
	{
		TSharedPtr<FJsonObject> Btn = MakeShared<FJsonObject>();
		Btn->SetNumberField(TEXT("type"),      2); // BUTTON
		Btn->SetNumberField(TEXT("style"),     Style);
		Btn->SetStringField(TEXT("label"),     Label);
		Btn->SetStringField(TEXT("custom_id"), CustomId);
		AllButtons.Add(MakeShared<FJsonValueObject>(Btn));
	};

	if (Config.bTicketWhitelistEnabled)
	{
		AddButton(TEXT("Whitelist Request"), TEXT("ticket_wl"), 1); // PRIMARY
	}
	if (Config.bTicketHelpEnabled)
	{
		AddButton(TEXT("Help / Support"), TEXT("ticket_help"), 3); // SUCCESS
	}
	if (Config.bTicketReportEnabled)
	{
		AddButton(TEXT("Report a Player"), TEXT("ticket_report"), 4); // DANGER
	}
	if (Config.bTicketBanAppealEnabled)
	{
		AddButton(TEXT("Ban Appeal"), TEXT("ticket_appeal"), 2); // SECONDARY
	}
	for (int32 i = 0; i < Config.CustomTicketReasons.Num() && AllButtons.Num() < 25; ++i)
	{
		FString Label, Desc;
		Config.CustomTicketReasons[i].Split(TEXT("|"), &Label, &Desc);
		Label.TrimStartAndEndInline();
		if (Label.IsEmpty()) { continue; }
		if (Label.Len() > 80)
		{
			Label = Label.Left(80);
		}
		AddButton(Label, FString::Printf(TEXT("ticket_cr_%d"), i), 2); // SECONDARY
	}

	if (AllButtons.Num() == 0)
	{
		if (!ResponseChannelId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(ResponseChannelId,
				TEXT(":warning: No ticket types are enabled. Enable at least one in "
				     "DefaultTickets.ini or add a TicketReason= entry."));
		}
		return;
	}

	// Split buttons into action rows (max 5 per row, max 5 rows = 25 buttons).
	TArray<TSharedPtr<FJsonValue>> ActionRows;
	for (int32 Start = 0; Start < AllButtons.Num(); Start += 5)
	{
		TArray<TSharedPtr<FJsonValue>> RowButtons;
		const int32 End = FMath::Min(Start + 5, AllButtons.Num());
		for (int32 j = Start; j < End; ++j)
		{
			RowButtons.Add(AllButtons[j]);
		}
		TSharedPtr<FJsonObject> Row = MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("type"), 1); // ACTION_ROW
		Row->SetArrayField(TEXT("components"), RowButtons);
		ActionRows.Add(MakeShared<FJsonValueObject>(Row));
	}

	TSharedPtr<FJsonObject> MessageBody = MakeShared<FJsonObject>();
	MessageBody->SetStringField(TEXT("content"), PanelContent.TrimEnd());
	MessageBody->SetArrayField(TEXT("components"), ActionRows);

	Bridge->SendMessageBodyToChannel(PanelChannelId, MessageBody);

	if (!ResponseChannelId.IsEmpty() && ResponseChannelId != PanelChannelId)
	{
		Bridge->SendDiscordChannelMessage(ResponseChannelId,
			FString::Printf(
				TEXT(":white_check_mark: Ticket panel posted to <#%s>."),
				*PanelChannelId));
	}

	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Ticket panel posted to channel %s."), *PanelChannelId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Slash command handler – handles /ticket command group
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::HandleSlashTicketCommand(const TSharedPtr<FJsonObject>& DataObj)
{
	if (!DataObj.IsValid()) return;

	// Only handle APPLICATION_COMMAND interactions (type 2).
	int32 InteractionType = 0;
	DataObj->TryGetNumberField(TEXT("type"), InteractionType);
	if (InteractionType != 2) return;

	// Extract command data.
	const TSharedPtr<FJsonObject>* CmdDataPtr = nullptr;
	if (!DataObj->TryGetObjectField(TEXT("data"), CmdDataPtr) || !CmdDataPtr) return;

	FString CmdGroupName;
	(*CmdDataPtr)->TryGetStringField(TEXT("name"), CmdGroupName);
	if (!CmdGroupName.Equals(TEXT("ticket"), ESearchCase::IgnoreCase)) return;

	// Extract the subcommand.
	const TArray<TSharedPtr<FJsonValue>>* TopOpts = nullptr;
	(*CmdDataPtr)->TryGetArrayField(TEXT("options"), TopOpts);
	if (!TopOpts || TopOpts->IsEmpty()) return;

	const TSharedPtr<FJsonObject>* SubCmdPtr = nullptr;
	if (!(*TopOpts)[0]->TryGetObject(SubCmdPtr) || !SubCmdPtr) return;

	FString SubCmdName;
	(*SubCmdPtr)->TryGetStringField(TEXT("name"), SubCmdName);
	SubCmdName = SubCmdName.ToLower();

	const TArray<TSharedPtr<FJsonValue>>* SubOpts = nullptr;
	(*SubCmdPtr)->TryGetArrayField(TEXT("options"), SubOpts);

	// Helper: extract a named option value as a string.
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
			return FString();
		}
		return FString();
	};

	// Extract channel and member info.
	FString SourceChannelId;
	DataObj->TryGetStringField(TEXT("channel_id"), SourceChannelId);

	FString SenderName, SenderId;
	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (DataObj->TryGetObjectField(TEXT("member"), MemberPtr) && MemberPtr)
	{
		(*MemberPtr)->TryGetStringField(TEXT("nick"), SenderName);
		const TSharedPtr<FJsonObject>* UserPtr = nullptr;
		if ((*MemberPtr)->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
		{
			(*UserPtr)->TryGetStringField(TEXT("id"), SenderId);
			if (SenderName.IsEmpty())
				(*UserPtr)->TryGetStringField(TEXT("global_name"), SenderName);
			if (SenderName.IsEmpty())
				(*UserPtr)->TryGetStringField(TEXT("username"), SenderName);
		}
	}
	if (SenderName.IsEmpty()) SenderName = TEXT("Staff");

	// Check sender has TicketNotifyRoleId.
	auto SenderHasNotifyRole = [&]() -> bool
	{
		if (Config.TicketNotifyRoleId.IsEmpty()) return false;
		if (!MemberPtr) return false;
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if (!(*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) || !Roles)
			return false;
		for (const TSharedPtr<FJsonValue>& R : *Roles)
		{
			FString RId;
			if (R->TryGetString(RId) && RId == Config.TicketNotifyRoleId)
				return true;
		}
		return false;
	};

	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge) return;

	// Synthesize an internal "!ticket-<subcmd> <args>" routing string (never
	// shown to users) and construct a
	// minimal message JSON that OnRawDiscordMessage can process directly.
	// This approach reuses all existing command logic without duplication.
	FString SynthContent;

	if (SubCmdName == TEXT("panel"))
		SynthContent = TEXT("!ticket-panel");
	else if (SubCmdName == TEXT("list"))
		SynthContent = TEXT("!ticket-list");
	else if (SubCmdName == TEXT("assign"))
		SynthContent = FString::Printf(TEXT("!ticket-assign <@%s>"), *GetOpt(TEXT("user")));
	else if (SubCmdName == TEXT("claim"))
		SynthContent = TEXT("!ticket-claim");
	else if (SubCmdName == TEXT("unclaim"))
		SynthContent = TEXT("!ticket-unclaim");
	else if (SubCmdName == TEXT("transfer"))
		SynthContent = FString::Printf(TEXT("!ticket-transfer <@%s>"), *GetOpt(TEXT("user")));
	else if (SubCmdName == TEXT("priority"))
		SynthContent = FString::Printf(TEXT("!ticket-priority %s"), *GetOpt(TEXT("level")));
	else if (SubCmdName == TEXT("macro"))
		SynthContent = FString::Printf(TEXT("!ticket-macro %s"), *GetOpt(TEXT("name")));
	else if (SubCmdName == TEXT("macros"))
		SynthContent = TEXT("!ticket-macros");
	else if (SubCmdName == TEXT("stats"))
		SynthContent = TEXT("!ticket-stats");
	else if (SubCmdName == TEXT("report"))
		SynthContent = FString::Printf(TEXT("!ticket-report %s"), *GetOpt(TEXT("text")));
	else if (SubCmdName == TEXT("tag"))
		SynthContent = FString::Printf(TEXT("!ticket-tag %s"), *GetOpt(TEXT("tag")));
	else if (SubCmdName == TEXT("untag"))
		SynthContent = FString::Printf(TEXT("!ticket-untag %s"), *GetOpt(TEXT("tag")));
	else if (SubCmdName == TEXT("tags"))
		SynthContent = TEXT("!ticket-tags");
	else if (SubCmdName == TEXT("note"))
		SynthContent = FString::Printf(TEXT("!ticket-note %s"), *GetOpt(TEXT("text")));
	else if (SubCmdName == TEXT("notes"))
		SynthContent = TEXT("!ticket-notes");
	else if (SubCmdName == TEXT("escalate"))
		SynthContent = TEXT("!ticket-escalate");
	else if (SubCmdName == TEXT("remind"))
		SynthContent = FString::Printf(TEXT("!ticket-remind %s"), *GetOpt(TEXT("text")));
	else if (SubCmdName == TEXT("blacklist"))
		SynthContent = FString::Printf(TEXT("!ticket-blacklist %s"), *GetOpt(TEXT("user")));
	else if (SubCmdName == TEXT("unblacklist"))
		SynthContent = FString::Printf(TEXT("!ticket-unblacklist %s"), *GetOpt(TEXT("user")));
	else if (SubCmdName == TEXT("blacklistlist"))
		SynthContent = TEXT("!ticket-blacklist-list");
	else if (SubCmdName == TEXT("merge"))
		SynthContent = FString::Printf(TEXT("!ticket-merge %s"), *GetOpt(TEXT("ticket_id")));
	else
		return;

	// Build a synthetic MESSAGE_CREATE JSON and route through OnRawDiscordMessage.
	TSharedPtr<FJsonObject> SynthMsg = MakeShared<FJsonObject>();
	SynthMsg->SetStringField(TEXT("content"),    SynthContent);
	SynthMsg->SetStringField(TEXT("channel_id"), SourceChannelId);

	// Copy the member object so role checks and name extraction work.
	if (MemberPtr)
		SynthMsg->SetObjectField(TEXT("member"), (*MemberPtr)->IsValid() ? MakeShared<FJsonObject>(**MemberPtr) : MakeShared<FJsonObject>());

	// Copy author object for name extraction fallback.
	const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
	if (DataObj->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
		SynthMsg->SetObjectField(TEXT("author"), MakeShared<FJsonObject>(**AuthorPtr));
	else if (MemberPtr)
	{
		// Synthesize a minimal author from the member's user field.
		const TSharedPtr<FJsonObject>* UserPtr = nullptr;
		if ((*MemberPtr)->TryGetObjectField(TEXT("user"), UserPtr) && UserPtr)
			SynthMsg->SetObjectField(TEXT("author"), MakeShared<FJsonObject>(**UserPtr));
	}

	OnRawDiscordMessage(SynthMsg);
}

// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::OnRawDiscordMessage(const TSharedPtr<FJsonObject>& MessageObj)
{
	if (!MessageObj.IsValid())
	{
		return;
	}

	FString Content;
	MessageObj->TryGetStringField(TEXT("content"), Content);
	Content.TrimStartAndEndInline();

	// ── Verify sender has TicketNotifyRoleId (required for all ticket commands) ─
	auto SenderHasNotifyRole = [&]() -> bool
	{
		if (Config.TicketNotifyRoleId.IsEmpty()) return false;
		const TSharedPtr<FJsonObject>* MemberPtrLocal = nullptr;
		if (!MessageObj->TryGetObjectField(TEXT("member"), MemberPtrLocal) || !MemberPtrLocal)
			return false;
		const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
		if (!(*MemberPtrLocal)->TryGetArrayField(TEXT("roles"), Roles) || !Roles)
			return false;
		for (const TSharedPtr<FJsonValue>& R : *Roles)
		{
			FString RId;
			if (R->TryGetString(RId) && RId == Config.TicketNotifyRoleId)
				return true;
		}
		return false;
	};

	// Helper to extract sender display name.
	auto ExtractSenderDisplayName = [&]() -> FString
	{
		FString Name;
		const TSharedPtr<FJsonObject>* MP = nullptr;
		if (MessageObj->TryGetObjectField(TEXT("member"), MP) && MP)
			(*MP)->TryGetStringField(TEXT("nick"), Name);
		if (Name.IsEmpty())
		{
			const TSharedPtr<FJsonObject>* AuthorPtrLocal = nullptr;
			if (MessageObj->TryGetObjectField(TEXT("author"), AuthorPtrLocal) && AuthorPtrLocal)
			{
				if (!(*AuthorPtrLocal)->TryGetStringField(TEXT("global_name"), Name) || Name.IsEmpty())
					(*AuthorPtrLocal)->TryGetStringField(TEXT("username"), Name);
			}
		}
		return Name.IsEmpty() ? TEXT("Staff") : Name;
	};

	FString SourceChannelId;
	MessageObj->TryGetStringField(TEXT("channel_id"), SourceChannelId);

	// Update last-activity timestamp for ticket channels on every message.
	if (!SourceChannelId.IsEmpty() && TicketChannelToOpener.Contains(SourceChannelId))
	{
		TicketChannelToLastActivity.Add(SourceChannelId, FDateTime::UtcNow());

		// Track first staff reply for SLA monitoring.
		if (SenderHasNotifyRole())
		{
			bool* bSlaReplied = TicketChannelStaffReplied.Find(SourceChannelId);
			if (!bSlaReplied || !*bSlaReplied)
				TicketChannelStaffReplied.Add(SourceChannelId, true);
		}
	}

	IDiscordBridgeProvider* Bridge = GetBridge();

	// All ticket commands are now handled exclusively via Discord slash commands
	// (/ticket subcommand).  HandleSlashTicketCommand synthesises an internal
	// message object (without a Discord snowflake "id" field) and calls this
	// method directly.  Any real Discord message that still starts with "!ticket-"
	// (typed manually by a user) is ignored here, so ! prefix commands no longer
	// work from Discord.
	FString SyntheticCheckId;
	const bool bIsSyntheticMessage = !MessageObj->TryGetStringField(TEXT("id"), SyntheticCheckId)
	                                  || SyntheticCheckId.IsEmpty();
	if (Content.StartsWith(TEXT("!ticket-"), ESearchCase::IgnoreCase) && !bIsSyntheticMessage)
	{
		return; // Real Discord !ticket-* message — ignore, use /ticket slash commands.
	}

	// ── !ticket-assign <@userId> ──────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-assign"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;

		// Must be inside an active ticket channel.
		if (!TicketChannelToOpener.Contains(SourceChannelId))
			return; // Silently ignore when not in a ticket channel.

		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can assign tickets."));
			return;
		}

		// Extract <@userId> from the content.
		// Accepts both <@userId> and <@!userId> (legacy nickname mention).
		FString MentionPart = Content.Mid(FCString::Strlen(TEXT("!ticket-assign"))).TrimStartAndEnd();
		FString AssigneeId;
		if (MentionPart.StartsWith(TEXT("<@")))
		{
			int32 EndBracket = INDEX_NONE;
			MentionPart.FindChar(TEXT('>'), EndBracket);
			if (EndBracket != INDEX_NONE)
			{
				AssigneeId = MentionPart.Mid(2, EndBracket - 2);
				// Strip optional '!' for legacy nickname mentions (<@!id>).
				if (AssigneeId.StartsWith(TEXT("!")))
					AssigneeId = AssigneeId.Mid(1);
			}
		}

		if (AssigneeId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket assign <@user>`"));
			return;
		}

		const FString SenderName = ExtractSenderDisplayName();
		TicketChannelToAssignee.Add(SourceChannelId, AssigneeId);
		TicketChannelToAssigneeName.Add(SourceChannelId, SenderName);
		SaveTicketState();

		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(
				TEXT(":pencil: This ticket has been claimed by **%s**."),
				*SenderName));
		return;
	}

	// ── !ticket-list ─────────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-list"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;

		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can view the ticket list."));
			return;
		}

		if (TicketChannelToOpener.Num() == 0)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":white_check_mark: No open tickets."));
			return;
		}

		FString ListMsg = FString::Printf(
			TEXT(":tickets: **Open tickets (%d):**\n"), TicketChannelToOpener.Num());
		for (const TPair<FString, FString>& Pair : TicketChannelToOpener)
		{
			const FString* AssigneeName = TicketChannelToAssigneeName.Find(Pair.Key);
			FString AssignedSuffix;
			if (AssigneeName && !AssigneeName->IsEmpty())
				AssignedSuffix = FString::Printf(TEXT("  [assigned to %s]"), **AssigneeName);

			FString TagSuffix;
			const TArray<FString>* ListTags = TicketChannelToTags.Find(Pair.Key);
			if (ListTags && ListTags->Num() > 0)
				TagSuffix = TEXT("  [tags: ") + FString::Join(*ListTags, TEXT(", ")) + TEXT("]");

			ListMsg += FString::Printf(
				TEXT("• <#%s> — opened by <@%s>%s%s\n"),
				*Pair.Key, *Pair.Value, *AssignedSuffix, *TagSuffix);
		}

		Bridge->SendDiscordChannelMessage(SourceChannelId, ListMsg.TrimEnd());
		return;
	}

	// ── DM opener on staff reply ─────────────────────────────────────────────
	// If the message is in a ticket channel and is not from the opener, DM opener.
	if (Config.bDmOpenerOnStaffReply && !SourceChannelId.IsEmpty()
	    && TicketChannelToOpener.Contains(SourceChannelId))
	{
		FString AuthorId;
		const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
		if (MessageObj->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
		{
			(*AuthorPtr)->TryGetStringField(TEXT("id"), AuthorId);
			bool bIsBot = false;
			(*AuthorPtr)->TryGetBoolField(TEXT("bot"), bIsBot);
			const FString OpenerId = TicketChannelToOpener[SourceChannelId];
			if (!bIsBot && !AuthorId.IsEmpty() && AuthorId != OpenerId && Bridge)
			{
				const FString BotToken = Bridge->GetBotToken();
				FString StaffMsg = Content.Left(200);
				if (Content.Len() > 200) StaffMsg += TEXT("…");
				const FString DmText = FString::Printf(
					TEXT(":envelope: **New reply in your ticket** <#%s>:\n**Staff:** %s"),
					*SourceChannelId, *StaffMsg);
				const FString OpenDmBody = FString::Printf(
					TEXT("{\"recipient_id\":\"%s\"}"), *OpenerId);
				TWeakObjectPtr<UTicketSubsystem> WeakDm(this);
				TSharedRef<IHttpRequest, ESPMode::ThreadSafe> DmReq =
					FHttpModule::Get().CreateRequest();
				DmReq->SetURL(TEXT("https://discord.com/api/v10/users/@me/channels"));
				DmReq->SetVerb(TEXT("POST"));
				DmReq->SetHeader(TEXT("Authorization"),
					FString::Printf(TEXT("Bot %s"), *BotToken));
				DmReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
				DmReq->SetContentAsString(OpenDmBody);
				DmReq->OnProcessRequestComplete().BindWeakLambda(
					this,
					[WeakDm, DmText, BotToken]
					(FHttpRequestPtr, FHttpResponsePtr R, bool bOk)
					{
						if (!bOk || !R.IsValid() || R->GetResponseCode() < 200
						    || R->GetResponseCode() >= 300) return;
						TSharedPtr<FJsonObject> DmChan;
						TSharedRef<TJsonReader<>> DR =
							TJsonReaderFactory<>::Create(R->GetContentAsString());
						if (!FJsonSerializer::Deserialize(DR, DmChan) || !DmChan.IsValid()) return;
						FString DmChanId;
						DmChan->TryGetStringField(TEXT("id"), DmChanId);
						if (DmChanId.IsEmpty()) return;
						TSharedPtr<FJsonObject> Msg = MakeShared<FJsonObject>();
						Msg->SetStringField(TEXT("content"), DmText);
						FString MsgBody;
						TSharedRef<TJsonWriter<>> MW = TJsonWriterFactory<>::Create(&MsgBody);
						FJsonSerializer::Serialize(Msg.ToSharedRef(), MW);
						TSharedRef<IHttpRequest, ESPMode::ThreadSafe> SendReq =
							FHttpModule::Get().CreateRequest();
						SendReq->SetURL(FString::Printf(
							TEXT("https://discord.com/api/v10/channels/%s/messages"),
							*DmChanId));
						SendReq->SetVerb(TEXT("POST"));
						SendReq->SetHeader(TEXT("Authorization"),
							FString::Printf(TEXT("Bot %s"), *BotToken));
						SendReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
						SendReq->SetContentAsString(MsgBody);
						SendReq->ProcessRequest();
					});
				DmReq->ProcessRequest();
			}
		}
	}

	// ── !ticket-claim ─────────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-claim"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can claim tickets."));
			return;
		}
		FString SenderId;
		const TSharedPtr<FJsonObject>* AP = nullptr;
		if (MessageObj->TryGetObjectField(TEXT("author"), AP) && AP)
			(*AP)->TryGetStringField(TEXT("id"), SenderId);
		const FString SenderName = ExtractSenderDisplayName();
		TicketChannelToAssignee.Add(SourceChannelId, SenderId);
		TicketChannelToAssigneeName.Add(SourceChannelId, SenderName);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":pencil: This ticket has been claimed by **%s**."), *SenderName));
		return;
	}

	// ── !ticket-unclaim ───────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-unclaim"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can unclaim tickets."));
			return;
		}
		TicketChannelToAssignee.Remove(SourceChannelId);
		TicketChannelToAssigneeName.Remove(SourceChannelId);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			TEXT(":information_source: This ticket is now unassigned."));
		return;
	}

	// ── !ticket-transfer <@userId> ────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-transfer"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can transfer tickets."));
			return;
		}
		FString MentionPart = Content.Mid(FCString::Strlen(TEXT("!ticket-transfer"))).TrimStartAndEnd();
		FString TargetId;
		if (MentionPart.StartsWith(TEXT("<@")))
		{
			int32 End = INDEX_NONE;
			MentionPart.FindChar(TEXT('>'), End);
			if (End != INDEX_NONE)
			{
				TargetId = MentionPart.Mid(2, End - 2);
				if (TargetId.StartsWith(TEXT("!"))) TargetId = TargetId.Mid(1);
			}
		}
		if (TargetId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket transfer <@user>`"));
			return;
		}
		TicketChannelToAssignee.Add(SourceChannelId, TargetId);
		TicketChannelToAssigneeName.Add(SourceChannelId, MentionPart);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":arrows_counterclockwise: Ticket transferred to <@%s>."), *TargetId));
		return;
	}

	// ── !ticket-priority <Low|Medium|High|Urgent> ─────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-priority"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can set ticket priority."));
			return;
		}
		FString PriorityArg = Content.Mid(FCString::Strlen(TEXT("!ticket-priority"))).TrimStartAndEnd();
		const TArray<FString> ValidPriorities = { TEXT("Low"), TEXT("Medium"), TEXT("High"), TEXT("Urgent") };
		FString Matched;
		for (const FString& P : ValidPriorities)
		{
			if (PriorityArg.Equals(P, ESearchCase::IgnoreCase))
			{
				Matched = P;
				break;
			}
		}
		if (Matched.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket priority <Low|Medium|High|Urgent>`"));
			return;
		}
		TicketChannelToPriority.Add(SourceChannelId, Matched);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":white_check_mark: Ticket priority set to **%s**."), *Matched));
		// Update channel topic via REST.
		if (!Bridge->GetBotToken().IsEmpty())
		{
			const FString BotToken = Bridge->GetBotToken();
			const FString OpenerMention = FString::Printf(TEXT("<@%s>"),
				*TicketChannelToOpener[SourceChannelId]);
			const FString TopicBody = FString::Printf(
				TEXT("{\"topic\":\"Priority: %s | Opened by: %s\"}"),
				*Matched, *OpenerMention);
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> TopicReq =
				FHttpModule::Get().CreateRequest();
			TopicReq->SetURL(FString::Printf(
				TEXT("https://discord.com/api/v10/channels/%s"), *SourceChannelId));
			TopicReq->SetVerb(TEXT("PATCH"));
			TopicReq->SetHeader(TEXT("Authorization"),
				FString::Printf(TEXT("Bot %s"), *BotToken));
			TopicReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			TopicReq->SetContentAsString(TopicBody);
			TopicReq->ProcessRequest();
		}
		return;
	}

	// ── !ticket-macros ────────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-macros"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole()) return;
		if (Config.TicketMacros.Num() == 0)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":information_source: No macros configured. Add `TicketMacro=name|text` entries to DefaultTickets.ini."));
			return;
		}
		FString MacroList = TEXT(":notepad_spiral: **Available macros:**\n");
		for (const FString& M : Config.TicketMacros)
		{
			FString MName, MText;
			M.Split(TEXT("|"), &MName, &MText);
			MacroList += FString::Printf(TEXT("• `%s`\n"), *MName.TrimStartAndEnd());
		}
		Bridge->SendDiscordChannelMessage(SourceChannelId, MacroList.TrimEnd());
		return;
	}

	// ── !ticket-macro <name> ──────────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-macro"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can use macros."));
			return;
		}
		const FString MacroName = Content.Mid(FCString::Strlen(TEXT("!ticket-macro"))).TrimStartAndEnd();
		if (MacroName.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket macro <name>`. Use `/ticket macros` for a list."));
			return;
		}
		FString FoundText;
		for (const FString& M : Config.TicketMacros)
		{
			FString MName, MText;
			M.Split(TEXT("|"), &MName, &MText);
			if (MName.TrimStartAndEnd().Equals(MacroName, ESearchCase::IgnoreCase))
			{
				FoundText = MText.TrimStartAndEnd();
				break;
			}
		}
		if (FoundText.IsEmpty())
		{
			FString AvailNames;
			for (const FString& M : Config.TicketMacros)
			{
				FString MN, MT;
				M.Split(TEXT("|"), &MN, &MT);
				if (!AvailNames.IsEmpty()) AvailNames += TEXT(", ");
				AvailNames += MN.TrimStartAndEnd();
			}
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":question: No macro named **%s**. Available: %s"),
					*MacroName, *AvailNames));
			return;
		}
		Bridge->SendDiscordChannelMessage(SourceChannelId, FoundText);
		return;
	}

	// ── !ticket-stats ─────────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-stats"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can view ticket stats."));
			return;
		}
		const FString StatsPath = GetStatsFilePath();
		FString StatsJson;
		TSharedPtr<FJsonObject> Stats = MakeShared<FJsonObject>();
		if (FFileHelper::LoadFileToString(StatsJson, *StatsPath))
		{
			TSharedRef<TJsonReader<>> SR = TJsonReaderFactory<>::Create(StatsJson);
			TSharedPtr<FJsonObject> Loaded;
			if (FJsonSerializer::Deserialize(SR, Loaded) && Loaded.IsValid())
				Stats = Loaded;
		}
		double TotalOpened = 0, TotalClosed = 0, AvgTime = 0, TotalRatings = 0, RatingSum = 0;
		Stats->TryGetNumberField(TEXT("total_opened"), TotalOpened);
		Stats->TryGetNumberField(TEXT("total_closed"), TotalClosed);
		Stats->TryGetNumberField(TEXT("avg_close_time_seconds"), AvgTime);
		Stats->TryGetNumberField(TEXT("total_ratings"), TotalRatings);
		Stats->TryGetNumberField(TEXT("rating_sum"), RatingSum);

		FString AvgTimeStr;
		if (AvgTime >= 3600)
			AvgTimeStr = FString::Printf(TEXT("%.1fh"), AvgTime / 3600.0);
		else if (AvgTime >= 60)
			AvgTimeStr = FString::Printf(TEXT("%.0fm"), AvgTime / 60.0);
		else
			AvgTimeStr = FString::Printf(TEXT("%.0fs"), AvgTime);

		FString RatingStr = TEXT("N/A");
		if (TotalRatings > 0)
			RatingStr = FString::Printf(TEXT("%.1f/5 (%d rating%s)"),
				RatingSum / TotalRatings,
				static_cast<int32>(TotalRatings),
				TotalRatings == 1 ? TEXT("") : TEXT("s"));

		const FString Msg = FString::Printf(
			TEXT(":bar_chart: **Ticket Statistics**\n")
			TEXT("Total opened: %d | Total closed: %d | Open now: %d\n")
			TEXT("Avg close time: %s\n")
			TEXT("Average rating: %s"),
			static_cast<int32>(TotalOpened),
			static_cast<int32>(TotalClosed),
			TicketChannelToOpener.Num(),
			*AvgTimeStr,
			*RatingStr);
		Bridge->SendDiscordChannelMessage(SourceChannelId, Msg);
		return;
	}

	// ── !ticket-report <week|month> ───────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-report"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can view ticket reports."));
			return;
		}
		const FString PeriodArg = Content.Mid(FCString::Strlen(TEXT("!ticket-report"))).TrimStartAndEnd();
		if (PeriodArg.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket report <week|month>`"));
			return;
		}
		const FString StatsPathRpt = GetStatsFilePath();
		FString StatsJsonRpt;
		TSharedPtr<FJsonObject> StatsRpt = MakeShared<FJsonObject>();
		if (FFileHelper::LoadFileToString(StatsJsonRpt, *StatsPathRpt))
		{
			TSharedRef<TJsonReader<>> SR2 = TJsonReaderFactory<>::Create(StatsJsonRpt);
			TSharedPtr<FJsonObject> Loaded2;
			if (FJsonSerializer::Deserialize(SR2, Loaded2) && Loaded2.IsValid())
				StatsRpt = Loaded2;
		}
		double RptOpened = 0, RptClosed = 0, RptAvgTime = 0, RptTotalRatings = 0, RptRatingSum = 0;
		StatsRpt->TryGetNumberField(TEXT("total_opened"), RptOpened);
		StatsRpt->TryGetNumberField(TEXT("total_closed"), RptClosed);
		StatsRpt->TryGetNumberField(TEXT("avg_close_time_seconds"), RptAvgTime);
		StatsRpt->TryGetNumberField(TEXT("total_ratings"), RptTotalRatings);
		StatsRpt->TryGetNumberField(TEXT("rating_sum"), RptRatingSum);

		FString RptAvgTimeStr;
		if (RptAvgTime >= 3600)
			RptAvgTimeStr = FString::Printf(TEXT("%.1fh"), RptAvgTime / 3600.0);
		else if (RptAvgTime >= 60)
			RptAvgTimeStr = FString::Printf(TEXT("%.0fm"), RptAvgTime / 60.0);
		else
			RptAvgTimeStr = FString::Printf(TEXT("%.0fs"), RptAvgTime);

		FString RptRatingStr = TEXT("N/A");
		if (RptTotalRatings > 0)
			RptRatingStr = FString::Printf(TEXT("%.1f/5 (%d rating%s)"),
				RptRatingSum / RptTotalRatings,
				static_cast<int32>(RptTotalRatings),
				RptTotalRatings == 1 ? TEXT("") : TEXT("s"));

		const FString PeriodDisplay = PeriodArg.Equals(TEXT("week"), ESearchCase::IgnoreCase)
			? TEXT("week") : TEXT("month");
		const FString ReportMsg = FString::Printf(
			TEXT(":bar_chart: **Ticket Report (%s) — All-time aggregate**\n")
			TEXT("Opened: %d | Closed: %d | Currently open: %d\n")
			TEXT("Avg close time: %s\n")
			TEXT("Average rating: %s\n")
			TEXT("*(Date-filtered reporting unavailable; showing all-time totals.)*"),
			*PeriodDisplay,
			static_cast<int32>(RptOpened),
			static_cast<int32>(RptClosed),
			TicketChannelToOpener.Num(),
			*RptAvgTimeStr,
			*RptRatingStr);
		Bridge->SendDiscordChannelMessage(SourceChannelId, ReportMsg);
		return;
	}

	// ── !ticket-tags ─────────────────────────────────────────────────────────
	// (Must be checked before !ticket-tag to avoid prefix collision.)
	if (Content.Equals(TEXT("!ticket-tags"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		const TArray<FString>* TagsView = TicketChannelToTags.Find(SourceChannelId);
		if (!TagsView || TagsView->Num() == 0)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":label: This ticket has no tags."));
		}
		else
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":label: **Tags:** ") + FString::Join(*TagsView, TEXT(", ")));
		}
		return;
	}

	// ── !ticket-untag <tag> ───────────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-untag"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can remove tags."));
			return;
		}
		const FString UntagArg = Content.Mid(FCString::Strlen(TEXT("!ticket-untag"))).TrimStartAndEnd();
		if (UntagArg.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket untag <tag>`"));
			return;
		}
		TArray<FString>& UntagList = TicketChannelToTags.FindOrAdd(SourceChannelId);
		const int32 Removed = UntagList.RemoveSingleSwap(UntagArg);
		SaveTicketState();
		if (Removed > 0)
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":label: Tag **%s** removed."), *UntagArg));
		else
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":question: Tag **%s** was not found on this ticket."), *UntagArg));
		return;
	}

	// ── !ticket-tag <tag> ─────────────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-tag"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can tag tickets."));
			return;
		}
		const FString TagArg = Content.Mid(FCString::Strlen(TEXT("!ticket-tag"))).TrimStartAndEnd();
		if (TagArg.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket tag <tag>`"));
			return;
		}
		TArray<FString>& TagList = TicketChannelToTags.FindOrAdd(SourceChannelId);
		TagList.AddUnique(TagArg);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":label: Tag **%s** added."), *TagArg));
		return;
	}

	// ── !ticket-notes ─────────────────────────────────────────────────────────
	// (Must be checked before !ticket-note to avoid prefix collision.)
	if (Content.Equals(TEXT("!ticket-notes"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can view ticket notes."));
			return;
		}
		const TArray<FString>* NotesView = TicketChannelToNotes.Find(SourceChannelId);
		if (!NotesView || NotesView->Num() == 0)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":notepad_spiral: No notes on this ticket."));
		}
		else
		{
			FString NotesList = TEXT(":notepad_spiral: **Staff notes (internal):**\n");
			for (int32 i = 0; i < NotesView->Num(); ++i)
				NotesList += FString::Printf(TEXT("%d. %s\n"), i + 1, *(*NotesView)[i]);
			Bridge->SendDiscordChannelMessage(SourceChannelId, NotesList.TrimEnd());
		}
		return;
	}

	// ── !ticket-note <text> ───────────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-note"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can add notes."));
			return;
		}
		const FString NoteText = Content.Mid(FCString::Strlen(TEXT("!ticket-note"))).TrimStartAndEnd();
		if (NoteText.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket note <text>`"));
			return;
		}
		TicketChannelToNotes.FindOrAdd(SourceChannelId).Add(NoteText);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			TEXT(":notepad_spiral: Note added (visible to staff only)."));
		return;
	}

	// ── !ticket-escalate ──────────────────────────────────────────────────────
	if (Content.Equals(TEXT("!ticket-escalate"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can escalate tickets."));
			return;
		}
		TicketChannelToPriority.Add(SourceChannelId, TEXT("Urgent"));
		SaveTicketState();

		const FString EscalateMsg = Config.TicketEscalationRoleId.IsEmpty()
			? TEXT(":rotating_light: **This ticket has been escalated.**")
			: FString::Printf(TEXT(":rotating_light: **This ticket has been escalated.** <@&%s>"),
			                  *Config.TicketEscalationRoleId);
		Bridge->SendDiscordChannelMessage(SourceChannelId, EscalateMsg);

		// Move to escalation category if configured.
		if (!Config.TicketEscalationCategoryId.IsEmpty() && !Bridge->GetBotToken().IsEmpty())
		{
			const FString EscPatchBody = FString::Printf(
				TEXT("{\"parent_id\":\"%s\"}"), *Config.TicketEscalationCategoryId);
			TSharedRef<IHttpRequest, ESPMode::ThreadSafe> EscReq =
				FHttpModule::Get().CreateRequest();
			EscReq->SetURL(FString::Printf(
				TEXT("https://discord.com/api/v10/channels/%s"), *SourceChannelId));
			EscReq->SetVerb(TEXT("PATCH"));
			EscReq->SetHeader(TEXT("Authorization"),
				FString::Printf(TEXT("Bot %s"), *Bridge->GetBotToken()));
			EscReq->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			EscReq->SetContentAsString(EscPatchBody);
			EscReq->ProcessRequest();
		}
		return;
	}

	// ── !ticket-remind <duration> ─────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-remind"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can set reminders."));
			return;
		}
		const FString RemindArg = Content.Mid(FCString::Strlen(TEXT("!ticket-remind"))).TrimStartAndEnd();
		if (RemindArg.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket remind <duration>` (e.g. `30m`, `2h`, `1d`)"));
			return;
		}
		// Parse duration: numeric prefix + suffix (w/d/h/m).
		FTimespan RemindSpan = FTimespan::Zero();
		bool bParsedRemind = false;
		if (RemindArg.Len() >= 2)
		{
			const TCHAR RemindSuffix = FChar::ToLower(RemindArg[RemindArg.Len() - 1]);
			const FString RemindNumStr = RemindArg.Left(RemindArg.Len() - 1);
			const double RemindVal = FCString::Atod(*RemindNumStr);
			if (RemindVal > 0.0)
			{
				if (RemindSuffix == TCHAR('w'))      { RemindSpan = FTimespan::FromDays(RemindVal * 7.0);  bParsedRemind = true; }
				else if (RemindSuffix == TCHAR('d')) { RemindSpan = FTimespan::FromDays(RemindVal);         bParsedRemind = true; }
				else if (RemindSuffix == TCHAR('h')) { RemindSpan = FTimespan::FromHours(RemindVal);        bParsedRemind = true; }
				else if (RemindSuffix == TCHAR('m')) { RemindSpan = FTimespan::FromMinutes(RemindVal);      bParsedRemind = true; }
			}
		}
		if (!bParsedRemind)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Invalid duration. Examples: `30m`, `2h`, `1d`, `1w`"));
			return;
		}
		TicketChannelToReminder.Add(SourceChannelId, FDateTime::UtcNow() + RemindSpan);
		SaveTicketState();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":alarm_clock: Reminder set for **%s** from now."), *RemindArg));
		return;
	}

	// ── !ticket-blacklist-list ────────────────────────────────────────────────
	// (Must be checked before !ticket-blacklist to avoid prefix collision.)
	if (Content.Equals(TEXT("!ticket-blacklist-list"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can view the blacklist."));
			return;
		}
		if (TicketBlacklist.Num() == 0)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":white_check_mark: The ticket blacklist is empty."));
		}
		else
		{
			FString BLList = FString::Printf(
				TEXT(":no_entry: **Ticket blacklist (%d):**\n"), TicketBlacklist.Num());
			for (const FString& BLID : TicketBlacklist)
				BLList += FString::Printf(TEXT("• %s\n"), *BLID);
			Bridge->SendDiscordChannelMessage(SourceChannelId, BLList.TrimEnd());
		}
		return;
	}

	// ── !ticket-blacklist <userId> ────────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-blacklist"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can blacklist users."));
			return;
		}
		FString BLArg = Content.Mid(FCString::Strlen(TEXT("!ticket-blacklist"))).TrimStartAndEnd();
		// Extract user ID from <@userId> or plain ID.
		FString BLUserId;
		if (BLArg.StartsWith(TEXT("<@")))
		{
			int32 BLEnd = INDEX_NONE;
			BLArg.FindChar(TCHAR('>'), BLEnd);
			if (BLEnd != INDEX_NONE)
			{
				BLUserId = BLArg.Mid(2, BLEnd - 2);
				if (BLUserId.StartsWith(TEXT("!"))) BLUserId = BLUserId.Mid(1);
			}
		}
		else
		{
			BLUserId = BLArg;
		}
		if (BLUserId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket blacklist <@user or userId>`"));
			return;
		}
		TicketBlacklist.Add(BLUserId);
		SaveTicketBlacklist();
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(TEXT(":no_entry: User **%s** has been blacklisted from opening tickets."),
			                *BLUserId));
		return;
	}

	// ── !ticket-unblacklist <userId> ──────────────────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-unblacklist"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can unblacklist users."));
			return;
		}
		FString UBLArg = Content.Mid(FCString::Strlen(TEXT("!ticket-unblacklist"))).TrimStartAndEnd();
		FString UBLUserId;
		if (UBLArg.StartsWith(TEXT("<@")))
		{
			int32 UBLEnd = INDEX_NONE;
			UBLArg.FindChar(TCHAR('>'), UBLEnd);
			if (UBLEnd != INDEX_NONE)
			{
				UBLUserId = UBLArg.Mid(2, UBLEnd - 2);
				if (UBLUserId.StartsWith(TEXT("!"))) UBLUserId = UBLUserId.Mid(1);
			}
		}
		else
		{
			UBLUserId = UBLArg;
		}
		if (UBLUserId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket unblacklist <@user or userId>`"));
			return;
		}
		const bool bWasBlacklisted = TicketBlacklist.Remove(UBLUserId) > 0;
		SaveTicketBlacklist();
		if (bWasBlacklisted)
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":white_check_mark: User **%s** has been removed from the ticket blacklist."),
				                *UBLUserId));
		else
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":question: User **%s** was not in the blacklist."), *UBLUserId));
		return;
	}

	// ── !ticket-merge <channelId or #channelId> ───────────────────────────────
	if (Content.StartsWith(TEXT("!ticket-merge"), ESearchCase::IgnoreCase))
	{
		if (!Bridge) return;
		if (!TicketChannelToOpener.Contains(SourceChannelId)) return;
		if (!SenderHasNotifyRole())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":no_entry: Only members with the support role can merge tickets."));
			return;
		}
		FString MergeArg = Content.Mid(FCString::Strlen(TEXT("!ticket-merge"))).TrimStartAndEnd();
		// Support <#channelId> channel mention format.
		FString TargetChanId;
		if (MergeArg.StartsWith(TEXT("<#")))
		{
			int32 MergeEnd = INDEX_NONE;
			MergeArg.FindChar(TCHAR('>'), MergeEnd);
			if (MergeEnd != INDEX_NONE)
				TargetChanId = MergeArg.Mid(2, MergeEnd - 2);
		}
		else
		{
			TargetChanId = MergeArg;
		}
		TargetChanId.TrimStartAndEndInline();

		if (TargetChanId.IsEmpty())
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: Usage: `/ticket merge <#channel or channelId>`"));
			return;
		}
		if (TargetChanId == SourceChannelId)
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				TEXT(":warning: You cannot merge a ticket with itself."));
			return;
		}
		if (!TicketChannelToOpener.Contains(TargetChanId))
		{
			Bridge->SendDiscordChannelMessage(SourceChannelId,
				FString::Printf(TEXT(":warning: <#%s> is not a tracked ticket channel."), *TargetChanId));
			return;
		}

		// Capture opener name from A to add as a note in B.
		FString MergeOpenerName;
		if (const FString* MN = TicketChannelToOpenerName.Find(SourceChannelId))
			MergeOpenerName = *MN;
		if (MergeOpenerName.IsEmpty())
		{
			if (const FString* MO = TicketChannelToOpener.Find(SourceChannelId))
				MergeOpenerName = FString::Printf(TEXT("<@%s>"), **MO);
		}

		// Post merge notices to both channels.
		Bridge->SendDiscordChannelMessage(SourceChannelId,
			FString::Printf(
				TEXT(":twisted_rightwards_arrows: **Merged with** <#%s>. This ticket has been closed."),
				*TargetChanId));
		Bridge->SendDiscordChannelMessage(TargetChanId,
			FString::Printf(
				TEXT(":twisted_rightwards_arrows: **Ticket merged from** <#%s>. Contents were merged here."),
				*SourceChannelId));

		// Add opener info from A as a note in B.
		if (!MergeOpenerName.IsEmpty())
		{
			TicketChannelToNotes.FindOrAdd(TargetChanId).Add(
				FString::Printf(TEXT("Merged from <#%s> (opener: %s)"), *SourceChannelId, *MergeOpenerName));
		}

		// Close channel A: remove from all tracking maps.
		FString MergeRemovedOpener;
		TicketChannelToOpener.RemoveAndCopyValue(SourceChannelId, MergeRemovedOpener);
		if (!MergeRemovedOpener.IsEmpty())
			OpenerToTicketChannel.Remove(MergeRemovedOpener);
		TicketChannelToAssignee.Remove(SourceChannelId);
		TicketChannelToAssigneeName.Remove(SourceChannelId);
		TicketChannelToLastActivity.Remove(SourceChannelId);
		TicketChannelToOpenerName.Remove(SourceChannelId);
		FString MergeRemovedType;
		TicketChannelToType.RemoveAndCopyValue(SourceChannelId, MergeRemovedType);
		TicketChannelToOpenTime.Remove(SourceChannelId);
		TicketChannelToPriority.Remove(SourceChannelId);
		TicketChannelToTags.Remove(SourceChannelId);
		TicketChannelToNotes.Remove(SourceChannelId);
		TicketChannelStaffReplied.Remove(SourceChannelId);
		TicketChannelToReminder.Remove(SourceChannelId);
		if (Config.bAllowMultipleTicketTypes && !MergeRemovedOpener.IsEmpty() && !MergeRemovedType.IsEmpty())
		{
			if (TMap<FString, FString>* MergeTypeMap = OpenerToTicketsByType.Find(MergeRemovedOpener))
				MergeTypeMap->Remove(MergeRemovedType);
		}
		SaveTicketState();

		// Delete channel A.
		Bridge->DeleteDiscordChannel(SourceChannelId);
		return;
	}

	// ── !ticket-panel ─────────────────────────────────────────────────────────
	if (!Content.Equals(TEXT("!ticket-panel"), ESearchCase::IgnoreCase))
	{
		return;
	}

	// Verify the sender holds TicketNotifyRoleId.
	if (!SenderHasNotifyRole())
	{
		return;
	}

	const FString PanelChannelId = Config.TicketPanelChannelId.IsEmpty()
		? SourceChannelId
		: Config.TicketPanelChannelId;

	PostTicketPanel(PanelChannelId, SourceChannelId);
}

// ─────────────────────────────────────────────────────────────────────────────
// Ticket state persistence
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::SaveTicketState() const
{
	// Build the directory path and ensure it exists.
	const FString StateDir  = FPaths::ProjectSavedDir() / TEXT("Config/TicketSystem");
	const FString StatePath = StateDir / TEXT("ActiveTickets.json");

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	if (!PlatformFile.DirectoryExists(*StateDir))
	{
		PlatformFile.CreateDirectoryTree(*StateDir);
	}

	// Serialize TicketChannelToOpener to a JSON array.
	TArray<TSharedPtr<FJsonValue>> TicketArray;
	for (const TPair<FString, FString>& Pair : TicketChannelToOpener)
	{
		TSharedPtr<FJsonObject> Entry = MakeShared<FJsonObject>();
		Entry->SetStringField(TEXT("channel_id"), Pair.Key);
		Entry->SetStringField(TEXT("opener_id"),  Pair.Value);
		const FString* AssigneeId   = TicketChannelToAssignee.Find(Pair.Key);
		const FString* AssigneeName = TicketChannelToAssigneeName.Find(Pair.Key);
		if (AssigneeId)   Entry->SetStringField(TEXT("assignee_id"),   *AssigneeId);
		if (AssigneeName) Entry->SetStringField(TEXT("assignee_name"), *AssigneeName);
		const FString* TicketType = TicketChannelToType.Find(Pair.Key);
		if (TicketType)   Entry->SetStringField(TEXT("ticket_type"),   *TicketType);
		const FString* Priority = TicketChannelToPriority.Find(Pair.Key);
		if (Priority)     Entry->SetStringField(TEXT("priority"),      *Priority);
		const FDateTime* OpenTime = TicketChannelToOpenTime.Find(Pair.Key);
		if (OpenTime && OpenTime->GetTicks() > 0)
			Entry->SetStringField(TEXT("open_time"), OpenTime->ToIso8601());
		const FString* OpenerName = TicketChannelToOpenerName.Find(Pair.Key);
		if (OpenerName)   Entry->SetStringField(TEXT("opener_name"),   *OpenerName);

		// Persist tags.
		const TArray<FString>* SaveTags = TicketChannelToTags.Find(Pair.Key);
		if (SaveTags && SaveTags->Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> TagJsonArr;
			for (const FString& T : *SaveTags)
				TagJsonArr.Add(MakeShared<FJsonValueString>(T));
			Entry->SetArrayField(TEXT("tags"), TagJsonArr);
		}

		// Persist notes.
		const TArray<FString>* SaveNotes = TicketChannelToNotes.Find(Pair.Key);
		if (SaveNotes && SaveNotes->Num() > 0)
		{
			TArray<TSharedPtr<FJsonValue>> NoteJsonArr;
			for (const FString& N : *SaveNotes)
				NoteJsonArr.Add(MakeShared<FJsonValueString>(N));
			Entry->SetArrayField(TEXT("notes"), NoteJsonArr);
		}

		// Persist staff-replied flag.
		const bool* bSaveStaffReplied = TicketChannelStaffReplied.Find(Pair.Key);
		if (bSaveStaffReplied)
			Entry->SetBoolField(TEXT("staff_replied"), *bSaveStaffReplied);

		// Persist reminder.
		const FDateTime* SaveReminder = TicketChannelToReminder.Find(Pair.Key);
		if (SaveReminder && SaveReminder->GetTicks() > 0)
			Entry->SetStringField(TEXT("reminder"), SaveReminder->ToIso8601());

		TicketArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	// Serialize cooldowns.
	TArray<TSharedPtr<FJsonValue>> CooldownArray;
	const FDateTime UtcNow = FDateTime::UtcNow();
	for (const TPair<FString, FDateTime>& CD : UserTicketCooldown)
	{
		if (CD.Value > UtcNow)
		{
			TSharedPtr<FJsonObject> CDEntry = MakeShared<FJsonObject>();
			CDEntry->SetStringField(TEXT("user_id"), CD.Key);
			CDEntry->SetStringField(TEXT("until"),   CD.Value.ToIso8601());
			CooldownArray.Add(MakeShared<FJsonValueObject>(CDEntry));
		}
	}

	// Serialize multi-ticket map.
	TArray<TSharedPtr<FJsonValue>> MultiArray;
	for (const TPair<FString, TMap<FString, FString>>& MT : OpenerToTicketsByType)
	{
		TSharedPtr<FJsonObject> MTEntry = MakeShared<FJsonObject>();
		MTEntry->SetStringField(TEXT("opener_id"), MT.Key);
		TSharedPtr<FJsonObject> TypeMap = MakeShared<FJsonObject>();
		for (const TPair<FString, FString>& TP : MT.Value)
			TypeMap->SetStringField(TP.Key, TP.Value);
		MTEntry->SetObjectField(TEXT("types"), TypeMap);
		MultiArray.Add(MakeShared<FJsonValueObject>(MTEntry));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("tickets"),   TicketArray);
	Root->SetArrayField(TEXT("cooldowns"), CooldownArray);
	Root->SetArrayField(TEXT("multi_tickets"), MultiArray);
	if (!StoredPanelMessageId.IsEmpty()) Root->SetStringField(TEXT("panel_message_id"), StoredPanelMessageId);
	if (!StoredPanelChannelId.IsEmpty()) Root->SetStringField(TEXT("panel_channel_id"), StoredPanelChannelId);

	FString JsonContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonContent);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(JsonContent, *StatePath,
		FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
	{
		UE_LOG(LogTicketSystem, Warning,
		       TEXT("TicketSystem: Failed to save active ticket state to '%s'."),
		       *StatePath);
	}
}

void UTicketSubsystem::LoadTicketState()
{
	const FString StatePath =
		FPaths::ProjectSavedDir() / TEXT("Config/TicketSystem/ActiveTickets.json");

	FString JsonContent;
	if (!FFileHelper::LoadFileToString(JsonContent, *StatePath))
	{
		// No state file is fine – this is a clean first run.
		return;
	}

	TSharedPtr<FJsonObject> Root;
	TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
	if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
	{
		UE_LOG(LogTicketSystem, Warning,
		       TEXT("TicketSystem: Failed to parse active ticket state from '%s'. "
		            "State discarded; any pre-restart tickets will not block duplicates."),
		       *StatePath);
		return;
	}

	const TArray<TSharedPtr<FJsonValue>>* Tickets = nullptr;
	if (!Root->TryGetArrayField(TEXT("tickets"), Tickets) || !Tickets)
	{
		return;
	}

	int32 Loaded = 0;
	for (const TSharedPtr<FJsonValue>& Entry : *Tickets)
	{
		const TSharedPtr<FJsonObject>* EntryObj = nullptr;
		if (!Entry->TryGetObject(EntryObj) || !EntryObj)
		{
			continue;
		}

		FString ChannelId;
		FString OpenerId;
		(*EntryObj)->TryGetStringField(TEXT("channel_id"), ChannelId);
		(*EntryObj)->TryGetStringField(TEXT("opener_id"),  OpenerId);

		if (ChannelId.IsEmpty() || OpenerId.IsEmpty())
		{
			continue;
		}

		TicketChannelToOpener.Add(ChannelId, OpenerId);
		OpenerToTicketChannel.Add(OpenerId,  ChannelId);

		// Restore assignment state if present.
		FString AssigneeId, AssigneeName;
		if ((*EntryObj)->TryGetStringField(TEXT("assignee_id"), AssigneeId) && !AssigneeId.IsEmpty())
		{
			TicketChannelToAssignee.Add(ChannelId, AssigneeId);
		}
		if ((*EntryObj)->TryGetStringField(TEXT("assignee_name"), AssigneeName) && !AssigneeName.IsEmpty())
		{
			TicketChannelToAssigneeName.Add(ChannelId, AssigneeName);
		}

		// Restore new fields.
		FString TicketType, Priority, OpenerName, OpenTimeStr;
		if ((*EntryObj)->TryGetStringField(TEXT("ticket_type"), TicketType) && !TicketType.IsEmpty())
			TicketChannelToType.Add(ChannelId, TicketType);
		if ((*EntryObj)->TryGetStringField(TEXT("priority"), Priority) && !Priority.IsEmpty())
			TicketChannelToPriority.Add(ChannelId, Priority);
		if ((*EntryObj)->TryGetStringField(TEXT("opener_name"), OpenerName) && !OpenerName.IsEmpty())
			TicketChannelToOpenerName.Add(ChannelId, OpenerName);
		if ((*EntryObj)->TryGetStringField(TEXT("open_time"), OpenTimeStr) && !OpenTimeStr.IsEmpty())
		{
			FDateTime OT;
			if (FDateTime::ParseIso8601(*OpenTimeStr, OT))
				TicketChannelToOpenTime.Add(ChannelId, OT);
		}

		// Restore tags.
		const TArray<TSharedPtr<FJsonValue>>* LoadTagArr = nullptr;
		if ((*EntryObj)->TryGetArrayField(TEXT("tags"), LoadTagArr) && LoadTagArr)
		{
			TArray<FString> LoadedTags;
			for (const TSharedPtr<FJsonValue>& TV : *LoadTagArr)
			{
				FString TStr;
				if (TV->TryGetString(TStr) && !TStr.IsEmpty())
					LoadedTags.Add(TStr);
			}
			if (LoadedTags.Num() > 0)
				TicketChannelToTags.Add(ChannelId, LoadedTags);
		}

		// Restore notes.
		const TArray<TSharedPtr<FJsonValue>>* LoadNoteArr = nullptr;
		if ((*EntryObj)->TryGetArrayField(TEXT("notes"), LoadNoteArr) && LoadNoteArr)
		{
			TArray<FString> LoadedNotes;
			for (const TSharedPtr<FJsonValue>& NV : *LoadNoteArr)
			{
				FString NStr;
				if (NV->TryGetString(NStr) && !NStr.IsEmpty())
					LoadedNotes.Add(NStr);
			}
			if (LoadedNotes.Num() > 0)
				TicketChannelToNotes.Add(ChannelId, LoadedNotes);
		}

		// Restore staff-replied flag.
		bool bLoadedStaffReplied = false;
		if ((*EntryObj)->TryGetBoolField(TEXT("staff_replied"), bLoadedStaffReplied))
			TicketChannelStaffReplied.Add(ChannelId, bLoadedStaffReplied);
		else
			TicketChannelStaffReplied.Add(ChannelId, false);

		// Restore reminder.
		FString LoadReminderStr;
		if ((*EntryObj)->TryGetStringField(TEXT("reminder"), LoadReminderStr) && !LoadReminderStr.IsEmpty())
		{
			FDateTime LoadRemTime;
			if (FDateTime::ParseIso8601(*LoadReminderStr, LoadRemTime) && LoadRemTime > FDateTime::UtcNow())
				TicketChannelToReminder.Add(ChannelId, LoadRemTime);
		}

		++Loaded;
	}

	// Restore cooldowns.
	const TArray<TSharedPtr<FJsonValue>>* Cooldowns = nullptr;
	if (Root->TryGetArrayField(TEXT("cooldowns"), Cooldowns) && Cooldowns)
	{
		const FDateTime Now = FDateTime::UtcNow();
		for (const TSharedPtr<FJsonValue>& CDVal : *Cooldowns)
		{
			const TSharedPtr<FJsonObject>* CDObj = nullptr;
			if (!CDVal->TryGetObject(CDObj) || !CDObj) continue;
			FString UserId, UntilStr;
			(*CDObj)->TryGetStringField(TEXT("user_id"), UserId);
			(*CDObj)->TryGetStringField(TEXT("until"),   UntilStr);
			if (UserId.IsEmpty() || UntilStr.IsEmpty()) continue;
			FDateTime Until;
			if (FDateTime::ParseIso8601(*UntilStr, Until) && Until > Now)
				UserTicketCooldown.Add(UserId, Until);
		}
	}

	// Restore multi-ticket map.
	const TArray<TSharedPtr<FJsonValue>>* MultiTickets = nullptr;
	if (Root->TryGetArrayField(TEXT("multi_tickets"), MultiTickets) && MultiTickets)
	{
		for (const TSharedPtr<FJsonValue>& MTVal : *MultiTickets)
		{
			const TSharedPtr<FJsonObject>* MTObj = nullptr;
			if (!MTVal->TryGetObject(MTObj) || !MTObj) continue;
			FString OpenerId;
			(*MTObj)->TryGetStringField(TEXT("opener_id"), OpenerId);
			if (OpenerId.IsEmpty()) continue;
			const TSharedPtr<FJsonObject>* TypeMapObj = nullptr;
			if ((*MTObj)->TryGetObjectField(TEXT("types"), TypeMapObj) && TypeMapObj)
			{
				for (const TPair<FString, TSharedPtr<FJsonValue>>& TP : (*TypeMapObj)->Values)
				{
					FString ChanId;
					if (TP.Value->TryGetString(ChanId))
						OpenerToTicketsByType.FindOrAdd(OpenerId).Add(TP.Key, ChanId);
				}
			}
		}
	}

	// Restore panel IDs.
	Root->TryGetStringField(TEXT("panel_message_id"), StoredPanelMessageId);
	Root->TryGetStringField(TEXT("panel_channel_id"), StoredPanelChannelId);

	if (Loaded > 0)
	{
		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Restored %d active ticket(s) from previous session."),
		       Loaded);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// GetStatsFilePath
// ─────────────────────────────────────────────────────────────────────────────

FString UTicketSubsystem::GetStatsFilePath()
{
	return FPaths::ProjectSavedDir() / TEXT("Config/TicketSystem/TicketStats.json");
}

// ─────────────────────────────────────────────────────────────────────────────
// GetTicketChannelForOpener — public helper for BanDiscordSubsystem
// ─────────────────────────────────────────────────────────────────────────────

FString UTicketSubsystem::GetTicketChannelForOpener(const FString& DiscordUserId) const
{
	if (const FString* ChannelId = OpenerToTicketChannel.Find(DiscordUserId))
	{
		return *ChannelId;
	}
	return FString();
}

// ─────────────────────────────────────────────────────────────────────────────
// CloseAppealTicketForOpener — called by BanDiscordSubsystem on approve/deny
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::CloseAppealTicketForOpener(const FString& DiscordUserId,
                                                   const FString& Resolution)
{
	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge) return;

	const FString ChannelId = GetTicketChannelForOpener(DiscordUserId);
	if (ChannelId.IsEmpty()) return;

	// Remove from all tracking maps before deleting.
	FString RemovedOpener;
	TicketChannelToOpener.RemoveAndCopyValue(ChannelId, RemovedOpener);
	if (!RemovedOpener.IsEmpty())
		OpenerToTicketChannel.Remove(RemovedOpener);
	OpenerToAppealId.Remove(DiscordUserId);

	FString RemovedType;
	TicketChannelToType.RemoveAndCopyValue(ChannelId, RemovedType);
	TicketChannelToAssignee.Remove(ChannelId);
	TicketChannelToAssigneeName.Remove(ChannelId);
	TicketChannelToLastActivity.Remove(ChannelId);
	TicketChannelToOpenTime.Remove(ChannelId);
	TicketChannelToOpenerName.Remove(ChannelId);
	TicketChannelToPriority.Remove(ChannelId);
	TicketChannelToTags.Remove(ChannelId);
	TicketChannelToNotes.Remove(ChannelId);
	TicketChannelStaffReplied.Remove(ChannelId);
	TicketChannelToReminder.Remove(ChannelId);
	if (Config.bAllowMultipleTicketTypes && !RemovedOpener.IsEmpty() && !RemovedType.IsEmpty())
	{
		if (TMap<FString, FString>* TypeMap = OpenerToTicketsByType.Find(RemovedOpener))
			TypeMap->Remove(RemovedType);
	}

	SaveTicketState();

	// Post the resolution message so it appears in the transcript.
	if (!Resolution.IsEmpty())
		Bridge->SendDiscordChannelMessage(ChannelId, Resolution);

	// Fetch transcript and delete the channel (mirrors the standard close flow).
	if (!Config.TicketLogChannelId.IsEmpty() && !Bridge->GetBotToken().IsEmpty())
	{
		const FString LogChannelId    = Config.TicketLogChannelId;
		const FString BotToken        = Bridge->GetBotToken();
		const FString ClosedChannelId = ChannelId;
		const FString OpenerIdForLog  = RemovedOpener;
		const FString ClosedAt        = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d"));

		TWeakObjectPtr<UTicketSubsystem> WeakThis(this);

		const FString FetchUrl = FString::Printf(
			TEXT("https://discord.com/api/v10/channels/%s/messages?limit=100"),
			*ClosedChannelId);

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FetchReq =
			FHttpModule::Get().CreateRequest();
		FetchReq->SetURL(FetchUrl);
		FetchReq->SetVerb(TEXT("GET"));
		FetchReq->SetHeader(TEXT("Authorization"),
			FString::Printf(TEXT("Bot %s"), *BotToken));

		FetchReq->OnProcessRequestComplete().BindWeakLambda(
			WeakThis.Get(),
			[WeakThis, LogChannelId, BotToken, OpenerIdForLog, ClosedAt, ClosedChannelId]
			(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bOk) mutable
			{
				UTicketSubsystem* Self = WeakThis.Get();
				if (!Self) return;
				IDiscordBridgeProvider* B = Self->GetBridge();
				if (!B) return;

				FString TranscriptText = FString::Printf(
					TEXT(":scroll: **Ban Appeal Ticket Resolved**\n")
					TEXT("Opener: %s\n")
					TEXT("Date closed: %s\n\n"),
					OpenerIdForLog.IsEmpty()
						? TEXT("(unknown)")
						: *FString::Printf(TEXT("<@%s>"), *OpenerIdForLog),
					*ClosedAt);

				if (bOk && Resp.IsValid() &&
				    Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
				{
					TSharedPtr<FJsonValue> Root;
					TSharedRef<TJsonReader<>> Reader =
						TJsonReaderFactory<>::Create(Resp->GetContentAsString());
					if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
					{
						const TArray<TSharedPtr<FJsonValue>>* MsgArr = nullptr;
						if (Root->TryGetArray(MsgArr) && MsgArr)
						{
							for (int32 Idx = MsgArr->Num() - 1; Idx >= 0; --Idx)
							{
								const TSharedPtr<FJsonObject>* MsgObjPtr = nullptr;
								if (!(*MsgArr)[Idx]->TryGetObject(MsgObjPtr) || !MsgObjPtr) continue;

								FString AuthorName;
								const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
								if ((*MsgObjPtr)->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
									(*AuthorPtr)->TryGetStringField(TEXT("username"), AuthorName);

								FString MsgContent;
								(*MsgObjPtr)->TryGetStringField(TEXT("content"), MsgContent);

								if (!MsgContent.IsEmpty())
									TranscriptText += FString::Printf(TEXT("[%s]: %s\n"), *AuthorName, *MsgContent);
							}
						}
					}
				}

				B->SendDiscordChannelMessage(LogChannelId, TranscriptText);
				B->DeleteDiscordChannel(ClosedChannelId);
			});
		FetchReq->ProcessRequest();
	}
	else
	{
		Bridge->DeleteDiscordChannel(ChannelId);
	}

	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Appeal ticket for Discord user %s closed by BanDiscordSubsystem."),
	       *DiscordUserId);
}

// ─────────────────────────────────────────────────────────────────────────────
// CloseTicketChannelInactive — auto-close a ticket due to inactivity
// ─────────────────────────────────────────────────────────────────────────────

void UTicketSubsystem::CloseTicketChannelInactive(const FString& ChannelId)
{
	IDiscordBridgeProvider* Bridge = GetBridge();
	if (!Bridge) return;

	// Remove from all tracking maps.
	FString RemovedOpener;
	TicketChannelToOpener.RemoveAndCopyValue(ChannelId, RemovedOpener);
	if (!RemovedOpener.IsEmpty())
		OpenerToTicketChannel.Remove(RemovedOpener);

	TicketChannelToAssignee.Remove(ChannelId);
	TicketChannelToAssigneeName.Remove(ChannelId);
	TicketChannelToLastActivity.Remove(ChannelId);
	FString RemovedTypeInactive;
	TicketChannelToType.RemoveAndCopyValue(ChannelId, RemovedTypeInactive);
	TicketChannelToPriority.Remove(ChannelId);
	TicketChannelToOpenTime.Remove(ChannelId);
	TicketChannelToOpenerName.Remove(ChannelId);

	if (Config.bAllowMultipleTicketTypes && !RemovedOpener.IsEmpty() && !RemovedTypeInactive.IsEmpty())
	{
		if (TMap<FString, FString>* TypeMap = OpenerToTicketsByType.Find(RemovedOpener))
			TypeMap->Remove(RemovedTypeInactive);
	}

	// Capture notes for transcript before removing.
	TArray<FString> InactiveNotesForLog;
	if (const TArray<FString>* INPtr = TicketChannelToNotes.Find(ChannelId))
		InactiveNotesForLog = *INPtr;

	TicketChannelToTags.Remove(ChannelId);
	TicketChannelToNotes.Remove(ChannelId);
	TicketChannelStaffReplied.Remove(ChannelId);
	TicketChannelToReminder.Remove(ChannelId);
	SaveTicketState();

	// If this was a ban-appeal ticket closed by inactivity, dismiss the pending
	// registry entry so it doesn't pile up as "pending" indefinitely.
	if (RemovedTypeInactive == TEXT("banappeal") && !RemovedOpener.IsEmpty())
	{
		if (UGameInstance* GI = GetGameInstance())
		{
			if (UBanAppealRegistry* AppealReg = GI->GetSubsystem<UBanAppealRegistry>())
			{
				const FString AppealUid = FString::Printf(TEXT("Discord:%s"), *RemovedOpener);
				for (const FBanAppealEntry& E : AppealReg->GetAllAppeals())
				{
					if (E.Uid == AppealUid && E.Status == EAppealStatus::Pending)
					{
						AppealReg->DeleteAppeal(E.Id);
						OpenerToAppealId.Remove(RemovedOpener);
						UE_LOG(LogTicketSystem, Log,
						       TEXT("TicketSystem: Auto-dismissed pending appeal id=%lld for Discord user %s on inactivity close."),
						       E.Id, *RemovedOpener);
						break;
					}
				}
			}
		}
	}

	// Post an auto-close notice so the transcript includes it.
	Bridge->SendDiscordChannelMessage(ChannelId,
		FString::Printf(
			TEXT(":alarm_clock: This ticket has been automatically closed after %.1f hour(s) of inactivity."),
			Config.InactiveTicketTimeoutHours));

	// Archive transcript and delete the channel.
	if (!Config.TicketLogChannelId.IsEmpty() && !Bridge->GetBotToken().IsEmpty())
	{
		const FString LogChannelId    = Config.TicketLogChannelId;
		const FString BotToken        = Bridge->GetBotToken();
		const FString ClosedChannelId = ChannelId;
		const FString OpenerIdForLog  = RemovedOpener;
		const FString ClosedAt        = FDateTime::UtcNow().ToString(TEXT("%Y-%m-%d"));

		TWeakObjectPtr<UTicketSubsystem> WeakThis(this);

		const FString FetchUrl = FString::Printf(
			TEXT("https://discord.com/api/v10/channels/%s/messages?limit=100"),
			*ClosedChannelId);

		TSharedRef<IHttpRequest, ESPMode::ThreadSafe> FetchReq =
			FHttpModule::Get().CreateRequest();
		FetchReq->SetURL(FetchUrl);
		FetchReq->SetVerb(TEXT("GET"));
		FetchReq->SetHeader(TEXT("Authorization"),
			FString::Printf(TEXT("Bot %s"), *BotToken));

		FetchReq->OnProcessRequestComplete().BindWeakLambda(
			WeakThis.Get(),
			[WeakThis, LogChannelId, BotToken, OpenerIdForLog, ClosedAt, ClosedChannelId, InactiveNotesForLog]
			(FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bOk) mutable
			{
				UTicketSubsystem* Self = WeakThis.Get();
				if (!Self) return;
				IDiscordBridgeProvider* B = Self->GetBridge();
				if (!B) return;

				FString TranscriptText = FString::Printf(
					TEXT(":scroll: **Ticket Auto-Closed (inactivity)**\n")
					TEXT("Opener: %s\n")
					TEXT("Date closed: %s\n\n"),
					OpenerIdForLog.IsEmpty() ? TEXT("(unknown)") : *FString::Printf(TEXT("<@%s>"), *OpenerIdForLog),
					*ClosedAt);

				if (bOk && Resp.IsValid() &&
				    Resp->GetResponseCode() >= 200 && Resp->GetResponseCode() < 300)
				{
					TSharedPtr<FJsonValue> Root;
					TSharedRef<TJsonReader<>> Reader =
						TJsonReaderFactory<>::Create(Resp->GetContentAsString());
					if (FJsonSerializer::Deserialize(Reader, Root) && Root.IsValid())
					{
						const TArray<TSharedPtr<FJsonValue>>* MsgArr = nullptr;
						if (Root->TryGetArray(MsgArr) && MsgArr)
						{
							for (int32 Idx = MsgArr->Num() - 1; Idx >= 0; --Idx)
							{
								const TSharedPtr<FJsonObject>* MsgObjPtr = nullptr;
								if (!(*MsgArr)[Idx]->TryGetObject(MsgObjPtr) || !MsgObjPtr) continue;

								FString AuthorName;
								const TSharedPtr<FJsonObject>* AuthorPtr = nullptr;
								if ((*MsgObjPtr)->TryGetObjectField(TEXT("author"), AuthorPtr) && AuthorPtr)
								{
									if (!(*AuthorPtr)->TryGetStringField(TEXT("global_name"), AuthorName) || AuthorName.IsEmpty())
										(*AuthorPtr)->TryGetStringField(TEXT("username"), AuthorName);
								}
								FString MsgContent;
								(*MsgObjPtr)->TryGetStringField(TEXT("content"), MsgContent);

								if (!AuthorName.IsEmpty() && !MsgContent.IsEmpty())
									TranscriptText += FString::Printf(TEXT("[%s]: %s\n"), *AuthorName, *MsgContent);
							}
						}
					}
				}
				else
				{
					TranscriptText += TEXT("*(Could not fetch message history.)*\n");
				}

				// Append staff notes if any.
				if (!InactiveNotesForLog.IsEmpty())
				{
					TranscriptText += TEXT("\n**Staff Notes (internal):**\n");
					for (const FString& InN : InactiveNotesForLog)
						TranscriptText += FString::Printf(TEXT("- %s\n"), *InN);
				}

				if (TranscriptText.Len() > 1900)
					TranscriptText = TranscriptText.Left(1900) + TEXT("\n*(transcript truncated)*");

				B->SendDiscordChannelMessage(LogChannelId, TranscriptText);
			});

		FetchReq->ProcessRequest();
		Bridge->DeleteDiscordChannel(ChannelId);
	}
	else
	{
		Bridge->DeleteDiscordChannel(ChannelId);
	}
}

// ─────────────────────────────────────────────────────────────────────────────
// Ticket blacklist persistence
// ─────────────────────────────────────────────────────────────────────────────

FString UTicketSubsystem::GetBlacklistFilePath()
{
return FPaths::ProjectSavedDir() / TEXT("Config/TicketSystem/TicketBlacklist.json");
}

void UTicketSubsystem::LoadTicketBlacklist()
{
const FString Path = GetBlacklistFilePath();
FString JsonContent;
if (!FFileHelper::LoadFileToString(JsonContent, *Path))
{
// No file on first run is fine.
return;
}

TSharedPtr<FJsonObject> Root;
TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonContent);
if (!FJsonSerializer::Deserialize(Reader, Root) || !Root.IsValid())
{
UE_LOG(LogTicketSystem, Warning,
       TEXT("TicketSystem: Failed to parse blacklist from '%s'. Blacklist cleared."), *Path);
return;
}

const TArray<TSharedPtr<FJsonValue>>* Ids = nullptr;
if (!Root->TryGetArrayField(TEXT("blacklist"), Ids) || !Ids)
{
return;
}

TicketBlacklist.Empty();
for (const TSharedPtr<FJsonValue>& IdVal : *Ids)
{
FString UserId;
if (IdVal->TryGetString(UserId) && !UserId.IsEmpty())
{
TicketBlacklist.Add(UserId);
}
}

UE_LOG(LogTicketSystem, Log,
       TEXT("TicketSystem: Loaded %d blacklisted user(s)."), TicketBlacklist.Num());
}

void UTicketSubsystem::SaveTicketBlacklist() const
{
const FString Path = GetBlacklistFilePath();
IPlatformFile& PF = FPlatformFileManager::Get().GetPlatformFile();
PF.CreateDirectoryTree(*FPaths::GetPath(Path));

TArray<TSharedPtr<FJsonValue>> IdArray;
for (const FString& UserId : TicketBlacklist)
{
IdArray.Add(MakeShared<FJsonValueString>(UserId));
}

TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
Root->SetArrayField(TEXT("blacklist"), IdArray);

FString JsonContent;
TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonContent);
FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

if (!FFileHelper::SaveStringToFile(JsonContent, *Path,
	FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))
{
UE_LOG(LogTicketSystem, Warning,
       TEXT("TicketSystem: Failed to save blacklist to '%s'."), *Path);
}
}
