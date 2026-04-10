// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "Containers/Ticker.h"
#include "IDiscordBridgeProvider.h"
#include "TicketConfig.h"
#include "TicketSubsystem.generated.h"

/**
 * UTicketSubsystem
 *
 * A GameInstance-level subsystem that implements a button-based Discord support-ticket
 * panel.  The subsystem is standalone: it has no compile-time dependency on any
 * specific Discord mod.  Instead, any mod that implements IDiscordBridgeProvider can
 * wire itself in by calling SetProvider(this) during its own Initialize(), then
 * SetProvider(nullptr) during Deinitialize().
 *
 * How it works
 * ────────────
 *  1. On Initialize() the subsystem loads DefaultTickets.ini and waits for a provider
 *     to be injected via SetProvider().
 *  2. Once SetProvider() is called it subscribes to the provider's interaction and
 *     raw-message events.
 *  3. When a Discord member clicks a ticket button (or submits a ticket reason
 *     modal) the interaction payload is dispatched here via the subscribed callback.
 *  4. The subsystem creates a private guild text channel visible only to the
 *     ticket opener and the admin/support role (TicketNotifyRoleId), posts a
 *     welcome message and a Close Ticket button, and optionally notifies the
 *     admin channel (TicketChannelId).
 *  5. When either the ticket opener or an admin clicks Close Ticket, the channel
 *     is deleted via the provider's DeleteDiscordChannel() method.
 *
 * Admin commands
 * ──────────────
 *  !ticket-panel               – Post the ticket selection panel to TicketPanelChannelId.
 *                                Requires the sender to hold TicketNotifyRoleId.
 *  !ticket-assign <@userId>    – Claim an open ticket on behalf of the mentioning staff
 *                                member.  Can only be used inside an active ticket channel.
 *                                Updates the TicketChannelToAssignee state map and posts a
 *                                confirmation message to the channel.
 *  !ticket-list                – List all open tickets.  Requires TicketNotifyRoleId.
 *
 * TicketAssignments
 * ─────────────────
 *  Each active ticket channel can be assigned to a single staff member.
 *  The assignee is stored in TicketChannelToAssignee (channel → Discord user ID)
 *  and TicketChannelToAssigneeName (channel → display name).  Assignment state is
 *  included in SaveTicketState / LoadTicketState so it survives server restarts.
 *
 * Required bot permissions
 * ─────────────────────────
 *  Manage Channels – create and delete ticket channels
 *  View Channel    – read channels
 *  Send Messages   – post the welcome and close-button messages
 */
UCLASS()
class DISCORDBRIDGE_API UTicketSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── USubsystem ────────────────────────────────────────────────────────────

	/** Only create on dedicated servers / listen servers (not in the editor). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Inject (or clear) the Discord provider used to send and receive Discord
	 * events.  Called by DiscordBridge during its own Initialize() and
	 * Deinitialize().  Pass nullptr to detach the current provider.
	 */
	void SetProvider(IDiscordBridgeProvider* InProvider);

private:
	// ── Interaction routing ───────────────────────────────────────────────────

	/**
	 * Bound to the provider's interaction event via SetProvider().
	 * Routes button clicks (type 3) and modal submits (type 5) to the
	 * appropriate internal handler.
	 */
	void OnInteractionReceived(const TSharedPtr<FJsonObject>& DataObj);

	/**
	 * Handle a MESSAGE_COMPONENT interaction (button click, type 3).
	 * Dispatches to the ticket-open or ticket-close logic based on the
	 * interaction's custom_id.
	 */
	void HandleTicketButtonInteraction(const FString& InteractionId,
	                                   const FString& InteractionToken,
	                                   const FString& CustomId,
	                                   const FString& DiscordUserId,
	                                   const FString& DiscordUsername,
	                                   const TArray<FString>& MemberRoles,
	                                   const FString& SourceChannelId);

	/**
	 * Respond to a ticket button interaction by showing a Discord modal (popup
	 * form) that lets the user provide a reason before the ticket channel is
	 * created.
	 *
	 * @param ModalCustomId  The custom_id for the modal (e.g. "ticket_modal:wl").
	 * @param Title          The modal window title (max 45 chars).
	 * @param Placeholder    Placeholder text shown inside the text input.
	 */
	void ShowTicketReasonModal(const FString& InteractionId,
	                           const FString& InteractionToken,
	                           const FString& ModalCustomId,
	                           const FString& Title,
	                           const FString& Placeholder);

	/**
	 * Handle a MODAL_SUBMIT interaction (type 5) from a ticket reason modal.
	 * Extracts the user-supplied text, checks for duplicate tickets, and calls
	 * CreateTicketChannel().
	 */
	void HandleTicketModalSubmit(const FString& InteractionId,
	                             const FString& InteractionToken,
	                             const FString& ModalCustomId,
	                             const TSharedPtr<FJsonObject>& DataObj,
	                             const FString& DiscordUserId,
	                             const FString& DiscordUsername);

	/**
	 * Create a private Discord text channel for a support ticket.
	 * Uses the injected provider's CreateDiscordGuildTextChannel() helper.
	 *
	 * @param OpenerUserId    Discord user ID of the member who opened the ticket.
	 * @param OpenerUsername  Display name of the member.
	 * @param TicketType      One of "whitelist", "help", "report", or a custom slug.
	 * @param ExtraInfo       Optional reason text entered by the member in the modal.
	 * @param DisplayLabel    Human-readable label for custom ticket types.
	 * @param DisplayDesc     Description for custom ticket types shown in the welcome message.
	 */
	void CreateTicketChannel(const FString& OpenerUserId,
	                         const FString& OpenerUsername,
	                         const FString& TicketType,
	                         const FString& ExtraInfo,
	                         const FString& DisplayLabel,
	                         const FString& DisplayDesc);

	/**
	 * Post the ticket panel (a message with clickable buttons) to the configured
	 * TicketPanelChannelId channel.
	 * Called when a member holding TicketNotifyRoleId types "!ticket-panel".
	 *
	 * @param PanelChannelId     Destination channel for the panel message.
	 * @param ResponseChannelId  Channel where the command confirmation is sent.
	 */
	void PostTicketPanel(const FString& PanelChannelId,
	                     const FString& ResponseChannelId);

	// ── Message routing ───────────────────────────────────────────────────────

	/**
	 * Bound to the provider's raw-message event via SetProvider().
	 * Checks whether the message content is the "!ticket-panel" command and,
	 * if the sender holds TicketNotifyRoleId, posts the ticket panel.
	 *
	 * @param MessageObj  The full MESSAGE_CREATE data JSON object.
	 */
	void OnRawDiscordMessage(const TSharedPtr<FJsonObject>& MessageObj);

	// ── Internal helpers ──────────────────────────────────────────────────────

	/**
	 * Sanitize a Discord username into a valid channel-name segment.
	 * Produces only lowercase letters, digits, and dashes; clamped to 40 chars.
	 */
	static FString SanitizeUsernameForChannel(const FString& Username);

	/** Build and return the close-ticket button JSON message body. */
	static TSharedPtr<FJsonObject> MakeCloseButtonMessage(const FString& OpenerUserId);

	/**
	 * Close a ticket channel due to inactivity (or any programmatic trigger).
	 * Posts a warning message to the channel, archives a transcript if
	 * TicketLogChannelId is set, then deletes the Discord channel.
	 * Removes the channel from all tracking maps and persists state.
	 */
	void CloseTicketChannelInactive(const FString& ChannelId);

	/** Return a pointer to IDiscordBridgeProvider (set via SetProvider()). */
	IDiscordBridgeProvider* GetBridge() const;

	// ── Ticket state persistence ──────────────────────────────────────────────

	/**
	 * Persist TicketChannelToOpener (and its reverse map) to disk so active
	 * tickets survive server restarts.  Written to:
	 *   <ProjectSavedDir>/Config/TicketSystem/ActiveTickets.json
	 *
	 * Called every time the map is mutated (channel created, ticket closed).
	 */
	void SaveTicketState() const;

	/**
	 * Restore the ticket maps from the state file written by SaveTicketState().
	 * Called at the end of Initialize() so the duplicate-ticket check is
	 * immediately functional after a server restart.
	 *
	 * Any entries that reference channels that no longer exist in Discord are
	 * harmless: the Close button still works correctly because the opener's
	 * Discord user ID is embedded in the button's custom_id.
	 */
	void LoadTicketState();

	// ── State ─────────────────────────────────────────────────────────────────

	/** Loaded config (populated in Initialize()). */
	FTicketConfig Config;

	/** Delegate handle for the interaction delegate subscription. */
	FDelegateHandle InteractionDelegateHandle;

	/** Delegate handle for the raw gateway message subscription. */
	FDelegateHandle RawMessageDelegateHandle;

	/**
	 * Maps each active ticket channel ID to the Discord user ID of its opener.
	 * Used to verify the Close Ticket authorisation and to clean up on deletion.
	 */
	TMap<FString, FString> TicketChannelToOpener;

	/**
	 * Reverse of TicketChannelToOpener: maps each opener's Discord user ID to
	 * the active ticket channel they currently have open.
	 * Used to prevent duplicate tickets (one active ticket per user at a time).
	 */
	TMap<FString, FString> OpenerToTicketChannel;

	/**
	 * Maps each active ticket channel ID to the Discord user ID of the staff
	 * member who has claimed ("assigned") the ticket.
	 * Populated by the !ticket-assign command.
	 */
	TMap<FString, FString> TicketChannelToAssignee;

	/**
	 * Maps each active ticket channel ID to the display name of the assigned
	 * staff member (for display in !ticket-list without a separate API call).
	 */
	TMap<FString, FString> TicketChannelToAssigneeName;

	/**
	 * Maps each active ticket channel ID to the UTC timestamp of the most
	 * recent message sent in that channel.  Used by the inactive-ticket
	 * timeout ticker to auto-close channels that have been idle for longer
	 * than InactiveTicketTimeoutHours.
	 * Populated when the ticket is created and updated on each MESSAGE_CREATE
	 * event that originates from a tracked ticket channel.
	 */
	TMap<FString, FDateTime> TicketChannelToLastActivity;

	/** Ticker handle for the inactive-ticket check.  Valid only when
	 *  InactiveTicketTimeoutHours > 0. */
	FTSTicker::FDelegateHandle InactiveTicketCheckHandle;

	/** Maps ticket channel ID to the opener's Discord username (for Approve & Whitelist). */
	TMap<FString, FString> TicketChannelToOpenerName;

	/** Maps ticket channel ID to the ticket open time (for stats). */
	TMap<FString, FDateTime> TicketChannelToOpenTime;

	/** Maps ticket channel ID to the ticket type (for stats and per-type categories). */
	TMap<FString, FString> TicketChannelToType;

	/** Maps ticket channel ID to its priority level. */
	TMap<FString, FString> TicketChannelToPriority;

	/** Maps user ID to cooldown expiry (when they can open next ticket). */
	TMap<FString, FDateTime> UserTicketCooldown;

	/** Channels pending reopen (grace period). Maps channel ID to expiry time. */
	TMap<FString, FDateTime> PendingReopenExpiry;

	/** Channels pending reopen. Maps channel ID to opener user ID. */
	TMap<FString, FString> PendingReopenOpener;

	/** Maps opener user ID to (ticketType -> channelId) for multi-ticket support. */
	TMap<FString, TMap<FString, FString>> OpenerToTicketsByType;

	/** Stored panel message ID for auto-refresh. */
	FString StoredPanelMessageId;
	/** Stored panel channel ID for auto-refresh. */
	FString StoredPanelChannelId;

	/** Ticker handle for the reopen grace period check. */
	FTSTicker::FDelegateHandle ReopenExpiryTickHandle;

	/** Returns path to TicketStats.json. */
	static FString GetStatsFilePath();

	/** Injected Discord provider.  nullptr until SetProvider() is called. */
	IDiscordBridgeProvider* CachedProvider = nullptr;
};
