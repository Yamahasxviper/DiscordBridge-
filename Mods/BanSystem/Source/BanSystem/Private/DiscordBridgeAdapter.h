// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "IBanDiscordCommandProvider.h"
#include "DiscordBridgeSubsystem.h"

/**
 * FDiscordBridgeCommandProviderAdapter
 *
 * Private adapter that bridges UDiscordBridgeSubsystem's public API to the
 * IBanDiscordCommandProvider interface expected by UBanDiscordSubsystem.
 *
 * When BanSystem detects that DiscordBridge is installed it creates one of
 * these adapters and passes it to UBanDiscordSubsystem::SetCommandProvider().
 * This lets BanSystem's Discord ban commands share DiscordBridge's existing
 * Gateway connection instead of opening a second one.
 *
 * DiscordBridge has zero knowledge of BanSystem — the dependency is entirely
 * one-directional: BanSystem → DiscordBridge.
 */
class FDiscordBridgeCommandProviderAdapter final : public IBanDiscordCommandProvider
{
public:
	explicit FDiscordBridgeCommandProviderAdapter(UDiscordBridgeSubsystem* InBridge)
		: Bridge(InBridge)
	{
	}

	// ── IBanDiscordCommandProvider ────────────────────────────────────────────

	virtual FDelegateHandle SubscribeDiscordMessages(
		TFunction<void(const TSharedPtr<FJsonObject>&)> Callback) override
	{
		// Forward to DiscordBridge's native MESSAGE_CREATE multicast delegate.
		return Bridge->OnDiscordRawMessageReceived.AddLambda(MoveTemp(Callback));
	}

	virtual void UnsubscribeDiscordMessages(FDelegateHandle Handle) override
	{
		Bridge->OnDiscordRawMessageReceived.Remove(Handle);
	}

	virtual void SendDiscordChannelMessage(const FString& ChannelId,
	                                       const FString& Message) override
	{
		// Delegate directly to DiscordBridge's REST send helper.
		Bridge->SendDiscordChannelMessage(ChannelId, Message);
	}

	virtual const FString& GetGuildOwnerId() const override
	{
		return Bridge->GetGuildOwnerId();
	}

private:
	/** Non-owning pointer; valid for the lifetime of this adapter. */
	UDiscordBridgeSubsystem* Bridge;
};
