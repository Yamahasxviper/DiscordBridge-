// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IBanDiscordCommandProvider.h"
#include "BanDiscordConfig.h"
#include "BanDiscordSubsystem.generated.h"

/**
 * UBanDiscordSubsystem
 *
 * Optional GameInstance-level subsystem that enables BanSystem's ban commands
 * (steamban, eosban, steamunban, eosunban, steambanlist, eosbanlist, banbyname,
 * playerids) to be run from Discord.
 *
 * This subsystem depends on an IBanDiscordCommandProvider being injected by
 * DiscordBridge (or another mod).  Without a provider it loads its config and
 * waits silently — no Discord functionality is available until a provider
 * registers via SetCommandProvider().
 *
 * Config
 * ──────
 *   <ServerRoot>/FactoryGame/Mods/BanSystem/Config/DefaultBanSystem.ini
 *   Key settings: DiscordChannelId, DiscordCommandRoleId, and per-command prefixes.
 *   See BanDiscordConfig.h for the full list.
 *
 * Integration (for DiscordBridge or any IBanDiscordCommandProvider):
 *   1. Implement IBanDiscordCommandProvider.
 *   2. In Initialize(), call UBanDiscordSubsystem::SetCommandProvider(this).
 *   3. In Deinitialize(), call UBanDiscordSubsystem::SetCommandProvider(nullptr).
 */
UCLASS()
class BANSYSTEM_API UBanDiscordSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── USubsystem ────────────────────────────────────────────────────────────

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Provider registration ─────────────────────────────────────────────────

	/**
	 * Register a Discord command provider (typically UDiscordBridgeSubsystem).
	 * Calling with nullptr detaches the current provider.
	 *
	 * Only one provider can be active at a time.  Registering a new provider
	 * replaces the previous one; the old subscription is cancelled first.
	 *
	 * BanSystem has no compile-time knowledge of the provider; this is the
	 * only coupling point between the two mods.
	 */
	void SetCommandProvider(IBanDiscordCommandProvider* InProvider);

private:
	// ── Discord message handling ──────────────────────────────────────────────

	/** Called on the game thread for every Discord MESSAGE_CREATE event. */
	void OnDiscordMessageReceived(const TSharedPtr<FJsonObject>& MessageObj);

	/**
	 * Returns true if the message sender is authorised to run BanSystem
	 * commands (holds DiscordCommandRoleId OR is the guild owner).
	 */
	bool HasCommandPermission(const TSharedPtr<FJsonObject>& MessageObj,
	                          const FString&                  AuthorId) const;

	// ── Per-command handlers ──────────────────────────────────────────────────

	/** !steamban <ID|Name> [minutes] [reason] */
	void HandleSteamBanCommand(const FString& Args,
	                           const FString& IssuedBy,
	                           const FString& ChannelId);

	/** !steamunban <Steam64Id> */
	void HandleSteamUnbanCommand(const FString& Args,
	                             const FString& ChannelId);

	/** !steambanlist */
	void HandleSteamBanListCommand(const FString& ChannelId);

	/** !eosban <ID|Name> [minutes] [reason] */
	void HandleEOSBanCommand(const FString& Args,
	                         const FString& IssuedBy,
	                         const FString& ChannelId);

	/** !eosunban <EOSProductUserId> */
	void HandleEOSUnbanCommand(const FString& Args,
	                           const FString& ChannelId);

	/** !eosbanlist */
	void HandleEOSBanListCommand(const FString& ChannelId);

	/** !banbyname <Name> [minutes] [reason] */
	void HandleBanByNameCommand(const FString& Args,
	                            const FString& IssuedBy,
	                            const FString& ChannelId);

	/** !playerids [Name] */
	void HandlePlayerIdsCommand(const FString& Args,
	                            const FString& ChannelId);

	// ── Helpers ───────────────────────────────────────────────────────────────

	/** Send a response message via the provider (no-op when provider is null). */
	void Reply(const FString& ChannelId, const FString& Message) const;

	/** Format ban duration for user-facing messages. */
	static FString FormatDuration(int32 DurationMinutes);

	/**
	 * Parse   "<ID|Name> [minutes] [reason...]"  into its components.
	 * OutArgs[0] is the ID/name; the remainder are optional duration + reason.
	 */
	static TArray<FString> SplitArgs(const FString& Input);

	// ── State ─────────────────────────────────────────────────────────────────

	/** Active Discord provider; nullptr when no provider is registered. */
	IBanDiscordCommandProvider* CommandProvider = nullptr;

	/** Handle from SubscribeDiscordMessages() — used to unsubscribe cleanly. */
	FDelegateHandle MessageSubscriptionHandle;

	/** Loaded configuration (channel, role, prefixes, message formats). */
	FBanDiscordConfig Config;
};
