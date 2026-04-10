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
	Config = FTicketConfig::Load();

	// Restore any active tickets from the previous session so the duplicate-ticket
	// check works correctly immediately after a server restart.
	LoadTicketState();

	UE_LOG(LogTicketSystem, Log,
	       TEXT("TicketSystem: Initialized. Waiting for Discord provider to be injected via SetProvider()."));
}

void UTicketSubsystem::Deinitialize()
{
	// Detach from the provider (unsubscribes delegates, clears CachedProvider).
	SetProvider(nullptr);

	TicketChannelToOpener.Empty();
	OpenerToTicketChannel.Empty();
	TicketChannelToAssignee.Empty();
	TicketChannelToAssigneeName.Empty();

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

	// We only handle MESSAGE_COMPONENT (3) and MODAL_SUBMIT (5).
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

		// Remove from tracking and delete.
		FString RemovedOpener;
		if (TicketChannelToOpener.RemoveAndCopyValue(SourceChannelId, RemovedOpener))
		{
			OpenerToTicketChannel.Remove(RemovedOpener);
		}
		TicketChannelToAssignee.Remove(SourceChannelId);
		TicketChannelToAssigneeName.Remove(SourceChannelId);
		// Persist the updated state so this closure survives a server restart.
		SaveTicketState();

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
				[WeakThis, LogChannelId, BotToken, OpenerIdForLog, ClosedAt, ClosedChannelId]
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
				AppealReg->AddAppeal(AppealUid, Reason, ContactInfo);

				UE_LOG(LogTicketSystem, Log,
				       TEXT("TicketSystem: Ban appeal from Discord user '%s' (%s) saved to BanAppealRegistry."),
				       *DiscordUsername, *DiscordUserId);
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

	// Capture a weak pointer so we never dereference a GC'd subsystem if the
	// HTTP response arrives after UTicketSubsystem has been destroyed.
	TWeakObjectPtr<UTicketSubsystem> WeakThis(this);
	Bridge->CreateDiscordGuildTextChannel(
		ChannelName,
		Config.TicketCategoryId,
		Overwrites,
		[WeakThis, NotifyRoleIdCopy, TicketChannelCopy,
		 OpenerUserIdCopy, OpenerUsernameCopy, TicketTypeCopy,
		 ExtraInfoCopy, DisplayLabelCopy, DisplayDescCopy]
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
			// Persist the updated state so this ticket survives a server restart.
			Self->SaveTicketState();

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
// Raw gateway message handler – listens for the "!ticket-panel" command
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

	IDiscordBridgeProvider* Bridge = GetBridge();

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
				TEXT(":warning: Usage: `!ticket-assign <@user>`"));
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

			ListMsg += FString::Printf(
				TEXT("• <#%s> — opened by <@%s>%s\n"),
				*Pair.Key, *Pair.Value, *AssignedSuffix);
		}

		Bridge->SendDiscordChannelMessage(SourceChannelId, ListMsg.TrimEnd());
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
		// Include assignee info if present.
		const FString* AssigneeId   = TicketChannelToAssignee.Find(Pair.Key);
		const FString* AssigneeName = TicketChannelToAssigneeName.Find(Pair.Key);
		if (AssigneeId)   Entry->SetStringField(TEXT("assignee_id"),   *AssigneeId);
		if (AssigneeName) Entry->SetStringField(TEXT("assignee_name"), *AssigneeName);
		TicketArray.Add(MakeShared<FJsonValueObject>(Entry));
	}

	TSharedPtr<FJsonObject> Root = MakeShared<FJsonObject>();
	Root->SetArrayField(TEXT("tickets"), TicketArray);

	FString JsonContent;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&JsonContent);
	FJsonSerializer::Serialize(Root.ToSharedRef(), Writer);

	if (!FFileHelper::SaveStringToFile(JsonContent, *StatePath))
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

		++Loaded;
	}

	if (Loaded > 0)
	{
		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Restored %d active ticket(s) from previous session."),
		       Loaded);
	}
}
