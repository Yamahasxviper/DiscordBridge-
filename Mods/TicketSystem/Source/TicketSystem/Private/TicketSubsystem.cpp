// Copyright Coffee Stain Studios. All Rights Reserved.

#include "TicketSubsystem.h"

#include "DiscordBridgeSubsystem.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/JsonSerializer.h"
#include "HttpModule.h"
#include "Interfaces/IHttpRequest.h"
#include "Interfaces/IHttpResponse.h"
#include "Engine/GameInstance.h"

DEFINE_LOG_CATEGORY_STATIC(LogTicketSystem, Log, All);

// Discord REST API base URL (same version as DiscordBridge)
static const FString TicketDiscordApiBase = TEXT("https://discord.com/api/v10");

// ─────────────────────────────────────────────────────────────────────────────
// USubsystem lifetime
// ─────────────────────────────────────────────────────────────────────────────

bool UTicketSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
	if (!Super::ShouldCreateSubsystem(Outer))
	{
		return false;
	}
	// Only create on dedicated servers – mirrors UDiscordBridgeSubsystem behaviour.
	return IsRunningDedicatedServer();
}

void UTicketSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Ensure DiscordBridgeSubsystem is initialised first.
	Collection.InitializeDependency<UDiscordBridgeSubsystem>();

	// Load ticket configuration.
	Config = FTicketConfig::Load();

	// Subscribe to the DiscordBridge interaction delegate.
	if (UDiscordBridgeSubsystem* Bridge = GetBridge())
	{
		InteractionDelegateHandle = Bridge->OnDiscordInteractionReceived.AddUObject(
			this, &UTicketSubsystem::OnInteractionReceived);

		RawMessageDelegateHandle = Bridge->OnDiscordRawMessageReceived.AddUObject(
			this, &UTicketSubsystem::OnRawDiscordMessage);

		UE_LOG(LogTicketSystem, Log,
		       TEXT("TicketSystem: Initialized. Subscribed to DiscordBridge interactions."));
	}
	else
	{
		UE_LOG(LogTicketSystem, Warning,
		       TEXT("TicketSystem: UDiscordBridgeSubsystem not found. "
		            "Ticket interactions will not be processed."));
	}
}

void UTicketSubsystem::Deinitialize()
{
	// Unsubscribe from the DiscordBridge delegate.
	if (UDiscordBridgeSubsystem* Bridge = GetBridge())
	{
		Bridge->OnDiscordInteractionReceived.Remove(InteractionDelegateHandle);
		Bridge->OnDiscordRawMessageReceived.Remove(RawMessageDelegateHandle);
	}
	InteractionDelegateHandle.Reset();
	RawMessageDelegateHandle.Reset();

	TicketChannelToOpener.Empty();
	OpenerToTicketChannel.Empty();

	Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
// Helper: get DiscordBridgeSubsystem
// ─────────────────────────────────────────────────────────────────────────────

UDiscordBridgeSubsystem* UTicketSubsystem::GetBridge() const
{
	if (const UGameInstance* GI = GetGameInstance())
	{
		return GI->GetSubsystem<UDiscordBridgeSubsystem>();
	}
	return nullptr;
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

	UDiscordBridgeSubsystem* Bridge = GetBridge();
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
		Bridge->DeleteDiscordChannel(SourceChannelId);
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
	const UDiscordBridgeSubsystem* Bridge = GetBridge();
	if (!Bridge || Bridge->GetBotToken().IsEmpty()
	    || InteractionId.IsEmpty() || InteractionToken.IsEmpty())
	{
		return;
	}

	const FString BotToken = Bridge->GetBotToken();

	// Build the text input component.
	TSharedPtr<FJsonObject> TextInput = MakeShared<FJsonObject>();
	TextInput->SetNumberField(TEXT("type"),        4); // TEXT_INPUT
	TextInput->SetStringField(TEXT("custom_id"),   TEXT("ticket_reason"));
	TextInput->SetNumberField(TEXT("style"),       2); // PARAGRAPH (multi-line)
	TextInput->SetStringField(TEXT("label"),       TEXT("Reason"));
	TextInput->SetStringField(TEXT("placeholder"), Placeholder);
	TextInput->SetBoolField  (TEXT("required"),    false);
	TextInput->SetNumberField(TEXT("max_length"),  1000);

	TSharedPtr<FJsonObject> ActionRow = MakeShared<FJsonObject>();
	ActionRow->SetNumberField(TEXT("type"), 1); // ACTION_ROW
	ActionRow->SetArrayField(TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(TextInput) });

	TSharedPtr<FJsonObject> ModalData = MakeShared<FJsonObject>();
	ModalData->SetStringField(TEXT("custom_id"),  ModalCustomId);
	ModalData->SetStringField(TEXT("title"),      Title);
	ModalData->SetArrayField (TEXT("components"),
		TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueObject>(ActionRow) });

	TSharedPtr<FJsonObject> Body = MakeShared<FJsonObject>();
	Body->SetNumberField(TEXT("type"), 9); // MODAL
	Body->SetObjectField(TEXT("data"), ModalData);

	FString BodyString;
	TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&BodyString);
	FJsonSerializer::Serialize(Body.ToSharedRef(), Writer);

	const FString Url = FString::Printf(
		TEXT("%s/interactions/%s/%s/callback"),
		*TicketDiscordApiBase, *InteractionId, *InteractionToken);

	TSharedRef<IHttpRequest, ESPMode::ThreadSafe> Request =
		FHttpModule::Get().CreateRequest();

	Request->SetURL(Url);
	Request->SetVerb(TEXT("POST"));
	Request->SetHeader(TEXT("Content-Type"),  TEXT("application/json"));
	Request->SetHeader(TEXT("Authorization"),
	                   FString::Printf(TEXT("Bot %s"), *BotToken));
	Request->SetContentAsString(BodyString);

	Request->OnProcessRequestComplete().BindLambda(
		[InteractionId](FHttpRequestPtr /*Req*/, FHttpResponsePtr Resp, bool bConnected)
		{
			if (!bConnected || !Resp.IsValid())
			{
				UE_LOG(LogTicketSystem, Warning,
				       TEXT("TicketSystem: ShowTicketReasonModal request failed (id=%s)."),
				       *InteractionId);
				return;
			}
			if (Resp->GetResponseCode() != 200 && Resp->GetResponseCode() != 204)
			{
				UE_LOG(LogTicketSystem, Warning,
				       TEXT("TicketSystem: ShowTicketReasonModal returned HTTP %d: %s"),
				       Resp->GetResponseCode(), *Resp->GetContentAsString());
			}
		});

	Request->ProcessRequest();
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

	UDiscordBridgeSubsystem* Bridge = GetBridge();
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
	UDiscordBridgeSubsystem* Bridge = GetBridge();
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
	const FString BotTokenCopy       = Bridge->GetBotToken();
	const FString NotifyRoleIdCopy   = Config.TicketNotifyRoleId;
	const FString TicketChannelCopy  = Config.TicketChannelId;
	const FString OpenerUserIdCopy   = OpenerUserId;
	const FString OpenerUsernameCopy = OpenerUsername;
	const FString TicketTypeCopy     = TicketType;
	const FString ExtraInfoCopy      = ExtraInfo;
	const FString DisplayLabelCopy   = DisplayLabel;
	const FString DisplayDescCopy    = DisplayDesc;

	Bridge->CreateDiscordGuildTextChannel(
		ChannelName,
		Config.TicketCategoryId,
		Overwrites,
		[this, BotTokenCopy, NotifyRoleIdCopy, TicketChannelCopy,
		 OpenerUserIdCopy, OpenerUsernameCopy, TicketTypeCopy,
		 ExtraInfoCopy, DisplayLabelCopy, DisplayDescCopy]
		(const FString& NewChannelId)
		{
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
			TicketChannelToOpener.Add(NewChannelId, OpenerUserIdCopy);
			OpenerToTicketChannel.Add(OpenerUserIdCopy, NewChannelId);

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
			if (UDiscordBridgeSubsystem* B = GetBridge())
			{
				TSharedPtr<FJsonObject> WelcomeMsg = MakeShared<FJsonObject>();
				WelcomeMsg->SetStringField(TEXT("content"), WelcomeContent);
				B->SendMessageBodyToChannel(NewChannelId, WelcomeMsg);

				// Post the close-ticket button.
				B->SendMessageBodyToChannel(NewChannelId,
					MakeCloseButtonMessage(OpenerUserIdCopy));

				// Notify the admin/ticket channel when configured.
				const TArray<FString> TicketChans =
					FTicketConfig::ParseChannelIds(TicketChannelCopy);
				const FString AdminChanId = TicketChans.Num() > 0
					? TicketChans[0] : TEXT("");

				if (!AdminChanId.IsEmpty() && AdminChanId != NewChannelId)
				{
					const FString NoticeTypeName = DisplayLabelCopy.IsEmpty()
						? TicketTypeCopy : DisplayLabelCopy;
					const FString AdminNotice = FString::Printf(
						TEXT("%s:new: New **%s** ticket from %s: <#%s>"),
						NotifyRoleIdCopy.IsEmpty()
							? TEXT("")
							: *FString::Printf(TEXT("<@&%s> "), *NotifyRoleIdCopy),
						*NoticeTypeName, *UserMention, *NewChannelId);

					TSharedPtr<FJsonObject> NoticeMsg = MakeShared<FJsonObject>();
					NoticeMsg->SetStringField(TEXT("content"), AdminNotice);
					B->SendMessageBodyToChannel(AdminChanId, NoticeMsg);
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
	UDiscordBridgeSubsystem* Bridge = GetBridge();
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

	if (!Content.Equals(TEXT("!ticket-panel"), ESearchCase::IgnoreCase))
	{
		return;
	}

	// Verify the sender holds TicketNotifyRoleId.
	if (Config.TicketNotifyRoleId.IsEmpty())
	{
		return;
	}

	const TSharedPtr<FJsonObject>* MemberPtr = nullptr;
	if (!MessageObj->TryGetObjectField(TEXT("member"), MemberPtr) || !MemberPtr)
	{
		return;
	}

	TArray<FString> MemberRoles;
	const TArray<TSharedPtr<FJsonValue>>* Roles = nullptr;
	if ((*MemberPtr)->TryGetArrayField(TEXT("roles"), Roles) && Roles)
	{
		for (const TSharedPtr<FJsonValue>& R : *Roles)
		{
			FString RoleId;
			if (R->TryGetString(RoleId))
			{
				MemberRoles.Add(RoleId);
			}
		}
	}

	if (!MemberRoles.Contains(Config.TicketNotifyRoleId))
	{
		return;
	}

	FString SourceChannelId;
	MessageObj->TryGetStringField(TEXT("channel_id"), SourceChannelId);

	const FString PanelChannelId = Config.TicketPanelChannelId.IsEmpty()
		? SourceChannelId
		: Config.TicketPanelChannelId;

	PostTicketPanel(PanelChannelId, SourceChannelId);
}
