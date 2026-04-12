// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Command/ChatCommandInstance.h"
#include "DiscordBridgeChatCommands.generated.h"

// ─────────────────────────────────────────────────────────────────────────────
//  /verify  — link a Discord account to an in-game player via a one-time code
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /verify <code>
 *
 * Links the player's Discord account to their in-game identity using the
 * one-time verification code issued by /whitelist link on Discord.
 * Requires bWhitelistVerificationEnabled=True in DefaultDiscordBridge.ini.
 *
 * The command is accessible to any connected player.
 *
 * Example:
 *   /verify A3F9
 */
UCLASS()
class DISCORDBRIDGE_API AVerifyDiscordChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	AVerifyDiscordChatCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /discord  — show the Discord server invite link
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /discord
 *
 * Replies with the Discord invite URL configured in DefaultDiscordBridge.ini
 * (DiscordInviteUrl). No arguments. Accessible to all players.
 *
 * Example:
 *   /discord
 */
UCLASS()
class DISCORDBRIDGE_API ADiscordInviteChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	ADiscordInviteChatCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};

// ─────────────────────────────────────────────────────────────────────────────
//  /ingamewhitelist  — manage the server whitelist from in-game chat
// ─────────────────────────────────────────────────────────────────────────────

/**
 * /ingamewhitelist <subcommand> [args...]
 *
 * In-game whitelist management command (equivalent to the old
 * InGameWhitelistCommandPrefix=!whitelist setting).
 *
 * Supported subcommands:
 *   /ingamewhitelist on             – enable the whitelist
 *   /ingamewhitelist off            – disable the whitelist
 *   /ingamewhitelist add <name>     – add a player by in-game name
 *   /ingamewhitelist remove <name>  – remove a player by in-game name
 *   /ingamewhitelist list           – list all whitelisted players
 *   /ingamewhitelist status         – show whether the whitelist is enabled
 *
 * Requires the caller to be an in-game admin (AdminEosPUIDs list in
 * BanChatCommands.ini), or to run from the server console.
 */
UCLASS()
class DISCORDBRIDGE_API AInGameWhitelistChatCommand : public AChatCommandInstance
{
	GENERATED_BODY()
public:
	AInGameWhitelistChatCommand();
	virtual EExecutionStatus ExecuteCommand_Implementation(
		UCommandSender* Sender,
		const TArray<FString>& Arguments,
		const FString& Label) override;
};
