// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "IBanDiscordCommandProvider.h"
#include "BanDiscordSubsystem.generated.h"

/**
 * UBanDiscordSubsystem
 *
 * GameInstance-level subsystem that is the single owner of ALL Discord-facing
 * ban management for the BanSystem mod.
 *
 * When DiscordBridge is installed it injects itself as the active
 * IBanDiscordCommandProvider.  DiscordBridge then routes every incoming
 * `!ban` Discord message here via HandleDiscordCommand().  BanSystem processes
 * the sub-command, interacts with USteamBanSubsystem / UEOSBanSubsystem as
 * needed, and replies through the provider.
 *
 * Supported sub-commands (everything after the `!ban` prefix):
 *   add <name> [duration_minutes] [reason...]  — ban a connected player by name
 *   remove <name>                              — unban an online player by name
 *   list                                       — show all active Steam + EOS bans
 *   status                                     — show ban count summary
 *   role add <discord_user_id>                 — grant the ban-admin Discord role
 *   role remove <discord_user_id>              — revoke the ban-admin Discord role
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
 *        BDS->HandleDiscordCommand(SubCommand, AdminName, ChannelId);
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

	// ── Main Discord command entry point ───────────────────────────────────────

	/**
	 * Handle any Discord `!ban` sub-command.
	 *
	 * Called by DiscordBridgeSubsystem after it has stripped the `!ban` prefix
	 * and verified that the sender holds the required BanCommandRoleId.
	 *
	 * @param SubCommand  Everything after the `!ban` prefix (trimmed).
	 * @param AdminName   Discord display name of the admin issuing the command.
	 * @param ChannelId   Discord channel snowflake to post the response to.
	 */
	void HandleDiscordCommand(const FString& SubCommand,
	                          const FString& AdminName,
	                          const FString& ChannelId);

private:
	// ── Sub-command handlers ───────────────────────────────────────────────────

	void HandleBanAdd   (const FString& Arg, const FString& AdminName, const FString& ChannelId);
	void HandleBanRemove(const FString& Arg, const FString& AdminName, const FString& ChannelId);
	void HandleBanList  (const FString& ChannelId);
	void HandleBanStatus(const FString& ChannelId);
	void HandleBanRole  (const FString& Arg, const FString& ChannelId);

	// ── Helpers ────────────────────────────────────────────────────────────────

	/** Active provider — raw pointer, owner clears via SetProvider(nullptr). */
	IBanDiscordCommandProvider* Provider = nullptr;

	/** Post a message to Discord (no-op when Provider is null). */
	void Reply(const FString& ChannelId, const FString& Message) const;

	/** Parse "<name>" or "<name> <duration> <reason...>" */
	static void ParseNameDurationReason(const FString& Input,
	                                    FString& OutName,
	                                    int32&   OutDurationMinutes,
	                                    FString& OutReason);
};
