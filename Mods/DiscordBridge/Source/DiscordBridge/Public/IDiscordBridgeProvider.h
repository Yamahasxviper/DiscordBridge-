// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

/**
 * FModalField
 *
 * Describes a single text-input component for use in RespondWithMultiFieldModal().
 * Each field is rendered as its own ACTION_ROW inside the modal (Discord allows
 * up to 5 fields per modal).
 */
struct DISCORDBRIDGE_API FModalField
{
	/** Label shown above the text input field (max 45 characters). */
	FString Label;

	/** Unique custom_id for the field; used to retrieve the submitted value
	 *  from the MODAL_SUBMIT components array. */
	FString CustomId;

	/** Greyed-out hint text displayed inside the empty field (optional). */
	FString Placeholder;

	/** Pre-filled default value shown in the field (optional). */
	FString DefaultValue;

	/** When true, the user must provide a non-empty value before submitting. */
	bool bRequired = false;

	/** When true, renders as a multi-line paragraph input (style 2).
	 *  When false, renders as a single-line short input (style 1). */
	bool bParagraph = false;

	/** Minimum character count required.  0 means no minimum. */
	int32 MinLength = 0;

	/** Maximum character count allowed (clamped to Discord's 4000-char limit).
	 *  0 defaults to 200. */
	int32 MaxLength = 200;
};

/**
 * IDiscordBridgeProvider
 *
 * Abstract interface through which UTicketSubsystem communicates with the
 * Discord bridge.  Implemented by UDiscordBridgeSubsystem in the DiscordBridge
 * mod.  Decouples TicketSystem from a hard compile-time dependency on
 * DiscordBridge so that TicketSystem can be loaded and configured independently.
 *
 * DiscordBridge calls UTicketSubsystem::SetProvider(this) during its own
 * Initialize() to inject itself as the active provider.  When DiscordBridge
 * is deinitialized it calls SetProvider(nullptr) to detach.
 */
class IDiscordBridgeProvider
{
public:
	virtual ~IDiscordBridgeProvider() = default;

	// ── Bot info ──────────────────────────────────────────────────────────────

	/** Returns the Discord bot token used for REST API authentication. */
	virtual const FString& GetBotToken() const = 0;

	/** Returns the Discord guild (server) ID received from the READY event.
	 *  Empty until the Gateway handshake is complete. */
	virtual const FString& GetGuildId() const = 0;

	/** Returns the Discord user ID of the guild owner.
	 *  Populated after the first GUILD_CREATE event; may be empty
	 *  on servers that only receive READY without a full GUILD_CREATE. */
	virtual const FString& GetGuildOwnerId() const = 0;

	// ── Delegate subscriptions ────────────────────────────────────────────────

	/**
	 * Subscribe a callback to be invoked for every INTERACTION_CREATE gateway
	 * event (button clicks, modal submits, etc.).
	 * Returns a handle that must be passed to UnsubscribeInteraction() when
	 * the subscription is no longer needed.
	 */
	virtual FDelegateHandle SubscribeInteraction(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) = 0;

	/** Remove a subscription registered with SubscribeInteraction(). */
	virtual void UnsubscribeInteraction(FDelegateHandle Handle) = 0;

	/**
	 * Subscribe a callback to be invoked for every MESSAGE_CREATE gateway
	 * event.
	 * Returns a handle that must be passed to UnsubscribeRawMessage() when
	 * the subscription is no longer needed.
	 */
	virtual FDelegateHandle SubscribeRawMessage(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) = 0;

	/** Remove a subscription registered with SubscribeRawMessage(). */
	virtual void UnsubscribeRawMessage(FDelegateHandle Handle) = 0;

	// ── Discord REST helpers ──────────────────────────────────────────────────

	/**
	 * Respond to a Discord interaction (button click or modal submit) via the
	 * REST API.  Must be called within ~3 seconds of receiving the interaction
	 * or Discord will show an error to the user.
	 *
	 * @param InteractionId    The interaction "id" field from the payload.
	 * @param InteractionToken The interaction "token" field from the payload.
	 * @param ResponseType     Discord callback type:
	 *                           4 = CHANNEL_MESSAGE_WITH_SOURCE
	 *                           5 = DEFERRED_CHANNEL_MESSAGE_WITH_SOURCE
	 *                           6 = DEFERRED_UPDATE_MESSAGE (ack silently)
	 * @param Content          Message text (used when ResponseType == 4).
	 * @param bEphemeral       When true the response is only visible to the
	 *                         user who triggered the interaction.
	 */
	virtual void RespondToInteraction(const FString& InteractionId,
	                                  const FString& InteractionToken,
	                                  int32 ResponseType,
	                                  const FString& Content,
	                                  bool bEphemeral) = 0;

	/**
	 * Respond to a Discord button-click interaction by showing a modal popup
	 * (Discord response type 9 = MODAL) via the REST API.  Must be called
	 * within ~3 seconds of receiving the interaction.
	 *
	 * @param InteractionId      The interaction "id" field from the payload.
	 * @param InteractionToken   The interaction "token" field from the payload.
	 * @param ModalCustomId      The custom_id to assign to the modal itself
	 *                           (e.g. "ticket_modal:wl").  Returned verbatim in
	 *                           the subsequent MODAL_SUBMIT interaction.
	 * @param Title              Modal window title (max 45 characters).
	 * @param Placeholder        Placeholder text shown inside the paragraph-style
	 *                           text input component.
	 * @param ComponentCustomId  The custom_id assigned to the single text-input
	 *                           component inside the modal.  Returned verbatim
	 *                           inside the MODAL_SUBMIT components array so the
	 *                           caller can locate the submitted value.
	 *                           Defaults to "ticket_reason" for backward
	 *                           compatibility with existing TicketSystem callers.
	 */
	virtual void RespondWithModal(const FString& InteractionId,
	                              const FString& InteractionToken,
	                              const FString& ModalCustomId,
	                              const FString& Title,
	                              const FString& Placeholder,
	                              const FString& ComponentCustomId = TEXT("ticket_reason")) = 0;

	/**
	 * Post a plain-text message to any Discord channel via the REST API.
	 *
	 * @param TargetChannelId  Snowflake ID of the destination channel.
	 * @param Message          Plain-text content (Discord markdown supported).
	 */
	virtual void SendDiscordChannelMessage(const FString& TargetChannelId,
	                                       const FString& Message) = 0;

	/**
	 * Post a follow-up message to a Discord slash-command interaction.
	 * The initial interaction response (type 4 or 5) must already have been sent.
	 * Uses POST /webhooks/{app_id}/{token} so the reply appears inline with the
	 * slash command and is dismissable when bEphemeral is true.
	 *
	 * @param InteractionToken  The "token" field from the original interaction payload.
	 * @param Message           Plain-text content (Discord markdown supported).
	 * @param bEphemeral        When true the message is only visible to the invoker.
	 */
	virtual void FollowUpInteraction(const FString& InteractionToken,
	                                 const FString& Message,
	                                 bool bEphemeral) = 0;

	/**
	 * Send a pre-built JSON message body (content + optional components) to a
	 * Discord channel via the REST API.
	 *
	 * @param TargetChannelId  Snowflake ID of the destination channel.
	 * @param MessageBody      Fully constructed Discord message JSON object.
	 */
	virtual void SendMessageBodyToChannel(const FString& TargetChannelId,
	                                      const TSharedPtr<FJsonObject>& MessageBody) = 0;

	/**
	 * Delete a Discord channel via the REST API.
	 *
	 * @param ChannelId  Snowflake ID of the channel to delete.
	 */
	virtual void DeleteDiscordChannel(const FString& ChannelId) = 0;

	/**
	 * Create a new guild text channel via the Discord REST API and invoke
	 * OnCreated with the new channel's snowflake ID on success (empty string
	 * on failure).
	 *
	 * @param ChannelName            Desired channel name.
	 * @param CategoryId             Optional parent category snowflake ID.
	 * @param PermissionOverwrites   JSON array of permission-overwrite objects.
	 * @param OnCreated              Called on the game thread with the new channel ID.
	 */
	virtual void CreateDiscordGuildTextChannel(
		const FString& ChannelName,
		const FString& CategoryId,
		const TArray<TSharedPtr<FJsonValue>>& PermissionOverwrites,
		TFunction<void(const FString& NewChannelId)> OnCreated) = 0;

	/**
	 * Respond to a Discord button-click or slash-command interaction with a
	 * fully constructed message body (Discord response type 4 =
	 * CHANNEL_MESSAGE_WITH_SOURCE), supporting embeds and interactive
	 * components.  Must be called within ~3 seconds of receiving the
	 * interaction.
	 *
	 * Unlike RespondToInteraction(), this overload accepts a complete
	 * "data" JSON object so callers can include embeds, action-row
	 * buttons, and other rich content as the interaction response.
	 *
	 * @param InteractionId    The interaction "id" field from the payload.
	 * @param InteractionToken The interaction "token" field from the payload.
	 * @param MessageData      The "data" object for the callback body.
	 *                         May contain "content", "embeds", "components",
	 *                         and/or "flags".  Caller retains ownership.
	 * @param bEphemeral       When true, flags = 64 is added so the response
	 *                         is only visible to the invoker.
	 */
	virtual void RespondToInteractionWithBody(const FString& InteractionId,
	                                          const FString& InteractionToken,
	                                          const TSharedPtr<FJsonObject>& MessageData,
	                                          bool bEphemeral) = 0;

	/**
	 * Respond to a Discord button-click interaction by showing a multi-field
	 * modal popup (Discord response type 9 = MODAL) via the REST API.  Must
	 * be called within ~3 seconds of receiving the interaction.
	 *
	 * Each element of Fields becomes its own ACTION_ROW containing a single
	 * TEXT_INPUT component.  Discord allows a maximum of 5 fields per modal.
	 *
	 * @param InteractionId    The interaction "id" field from the payload.
	 * @param InteractionToken The interaction "token" field from the payload.
	 * @param ModalCustomId    The custom_id to assign to the modal itself.
	 *                         Returned verbatim in the subsequent MODAL_SUBMIT
	 *                         interaction payload.
	 * @param Title            Modal window title (max 45 characters).
	 * @param Fields           List of text-input fields (max 5).  Each field's
	 *                         CustomId is used as the component custom_id so
	 *                         submitted values can be located by name.
	 */
	virtual void RespondWithMultiFieldModal(const FString& InteractionId,
	                                        const FString& InteractionToken,
	                                        const FString& ModalCustomId,
	                                        const FString& Title,
	                                        const TArray<FModalField>& Fields) = 0;

	/**
	 * Asynchronously creates a public thread in @p ChannelId named @p ThreadName
	 * (auto-archive after 7 days).  Calls @p OnCreated with the new thread's
	 * channel ID on success, or with an empty string if the request fails.
	 *
	 * The caller is responsible for posting the first message to the returned
	 * thread ID.
	 *
	 * @param ChannelId   Parent channel where the thread is created.
	 * @param ThreadName  Thread name (max 100 characters; truncated by caller).
	 * @param OnCreated   Callback invoked on the game thread once the Discord
	 *                    API responds.  Receives the thread channel ID or "".
	 */
	virtual void CreateDiscordThread(
		const FString& ChannelId,
		const FString& ThreadName,
		TFunction<void(const FString& ThreadId)> OnCreated) = 0;
};
