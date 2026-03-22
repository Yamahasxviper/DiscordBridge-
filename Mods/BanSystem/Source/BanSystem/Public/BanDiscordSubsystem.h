// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IBanDiscordCommandProvider.h"
#include "BanDiscordSubsystem.generated.h"

/**
 * UBanDiscordSubsystem
 *
 * GameInstance-level subsystem that handles Discord-sourced ban-by-name
 * commands on behalf of the BanSystem mod.
 *
 * When DiscordBridge is installed it injects itself as the active
 * IBanDiscordCommandProvider and routes `!ban add/remove` Discord messages
 * here.  BanSystem uses FBanPlayerLookup to resolve the player's platform IDs
 * (Steam64 and/or EOS PUID) from their in-game display name and then bans
 * or unbans through USteamBanSubsystem and UEOSBanSubsystem.
 *
 * BanSystem has zero compile-time knowledge of DiscordBridge.  All Discord
 * communication is tunnelled through IBanDiscordCommandProvider so the two
 * mods stay fully decoupled.
 *
 * Usage (done by DiscordBridgeSubsystem):
 *   1. Obtain this subsystem:
 *        UBanDiscordSubsystem* BDS = GI->GetSubsystem<UBanDiscordSubsystem>();
 *   2. Register the provider:
 *        BDS->SetProvider(this);          // in Initialize()
 *        BDS->SetProvider(nullptr);       // in Deinitialize()
 *   3. Forward Discord messages:
 *        BDS->HandleDiscordBanByNameCommand(NameAndArgs, AdminName, ChannelId);
 *        BDS->HandleDiscordUnbanByNameCommand(PlayerName, AdminName, ChannelId);
 */
UCLASS()
class BANSYSTEM_API UBanDiscordSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── USubsystem ─────────────────────────────────────────────────────────────
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	// ── Provider registration ──────────────────────────────────────────────────

	/**
	 * Register the Discord messaging provider.  Pass nullptr to detach.
	 * Called by UDiscordBridgeSubsystem in its Initialize() / Deinitialize().
	 *
	 * Only one provider may be active at a time.  Registering a new provider
	 * replaces the previous one without notification.
	 */
	void SetProvider(IBanDiscordCommandProvider* InProvider);

	// ── Discord command entry points ───────────────────────────────────────────

	/**
	 * Handle a Discord "!ban add" command by banning a connected player by name
	 * across all available platforms (Steam + EOS) simultaneously.
	 *
	 * Parses NameAndArgs as:
	 *   <PlayerName> [duration_minutes] [reason...]
	 *
	 * duration_minutes = 0 (or omitted) for a permanent ban.
	 *
	 * Examples:
	 *   HandleDiscordBanByNameCommand("SomePlayer",                "AdminUser", ChannelId)
	 *   HandleDiscordBanByNameCommand("SomePlayer 60 Spamming",   "AdminUser", ChannelId)
	 *
	 * @param NameAndArgs  Everything after "add " — player name plus optional duration/reason.
	 * @param AdminName    Discord display name of the admin issuing the command.
	 * @param ChannelId    Discord channel snowflake to post the response to.
	 */
	void HandleDiscordBanByNameCommand(const FString& NameAndArgs,
	                                   const FString& AdminName,
	                                   const FString& ChannelId);

	/**
	 * Handle a Discord "!ban remove" command by unbanning a currently-connected
	 * player by name.
	 *
	 * Because BanSystem stores bans by platform ID (not by display name),
	 * unban-by-name only works for players who are currently online so their
	 * IDs can be resolved.  For offline players the response will direct the
	 * admin to use the in-game /steamunban or /eosunban commands by raw ID.
	 *
	 * @param PlayerName  In-game display name of the player to unban.
	 * @param AdminName   Discord display name of the admin issuing the command.
	 * @param ChannelId   Discord channel snowflake to post the response to.
	 */
	void HandleDiscordUnbanByNameCommand(const FString& PlayerName,
	                                     const FString& AdminName,
	                                     const FString& ChannelId);

private:
	/** Active provider — raw pointer, owner clears via SetProvider(nullptr). */
	IBanDiscordCommandProvider* Provider = nullptr;

	/** Helper: post a message to Discord (no-op when Provider is null). */
	void Reply(const FString& ChannelId, const FString& Message) const;

	/** Helper: parse "<name>" or "<name> <duration> <reason...>" */
	static void ParseNameDurationReason(const FString& Input,
	                                    FString& OutName,
	                                    int32&   OutDurationMinutes,
	                                    FString& OutReason);
};
