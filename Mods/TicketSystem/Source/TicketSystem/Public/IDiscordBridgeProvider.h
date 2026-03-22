// Copyright Coffee Stain Studios. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

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
	 * @param InteractionId    The interaction "id" field from the payload.
	 * @param InteractionToken The interaction "token" field from the payload.
	 * @param ModalCustomId    The custom_id to assign to the modal itself
	 *                         (e.g. "ticket_modal:wl").  Returned verbatim in
	 *                         the subsequent MODAL_SUBMIT interaction.
	 * @param Title            Modal window title (max 45 characters).
	 * @param Placeholder      Placeholder text shown inside the paragraph-style
	 *                         text input component.
	 */
	virtual void RespondWithModal(const FString& InteractionId,
	                              const FString& InteractionToken,
	                              const FString& ModalCustomId,
	                              const FString& Title,
	                              const FString& Placeholder) = 0;

	/**
	 * Post a plain-text message to any Discord channel via the REST API.
	 *
	 * @param TargetChannelId  Snowflake ID of the destination channel.
	 * @param Message          Plain-text content (Discord markdown supported).
	 */
	virtual void SendDiscordChannelMessage(const FString& TargetChannelId,
	                                       const FString& Message) = 0;

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
};
