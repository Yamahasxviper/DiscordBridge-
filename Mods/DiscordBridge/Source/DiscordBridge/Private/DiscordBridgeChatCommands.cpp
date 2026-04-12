// Copyright Yamahasxviper. All Rights Reserved.

#include "DiscordBridgeChatCommands.h"
#include "DiscordBridgeSubsystem.h"
#include "Command/CommandSender.h"
#include "Engine/GameInstance.h"

// ─────────────────────────────────────────────────────────────────────────────
// Helper: get the UDiscordBridgeSubsystem from any actor in the game world.
// ─────────────────────────────────────────────────────────────────────────────

static UDiscordBridgeSubsystem* GetBridge(AActor* ContextActor)
{
	if (!ContextActor) return nullptr;
	UGameInstance* GI = ContextActor->GetGameInstance();
	return GI ? GI->GetSubsystem<UDiscordBridgeSubsystem>() : nullptr;
}

// ─────────────────────────────────────────────────────────────────────────────
// AVerifyDiscordChatCommand  /verify <code>
// ─────────────────────────────────────────────────────────────────────────────

AVerifyDiscordChatCommand::AVerifyDiscordChatCommand()
{
	CommandName          = TEXT("verify");
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer  = true;
	Usage = NSLOCTEXT("DiscordBridge", "VerifyUsage",
		"/verify <code>  — Link your Discord account using the code from /whitelist link");
}

EExecutionStatus AVerifyDiscordChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& Label)
{
	UDiscordBridgeSubsystem* Bridge = GetBridge(this);
	if (!Bridge)
	{
		Sender->SendChatMessage(
			TEXT("[DiscordBridge] Discord bridge is not active."),
			FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	const FString PlayerName = Sender->GetSenderName();
	// Join all arguments in case the player typed the code with spaces.
	const FString Code = FString::Join(Arguments, TEXT(" ")).TrimStartAndEnd();

	Bridge->HandleInGameVerify(PlayerName, Code);
	return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
// ADiscordInviteChatCommand  /discord
// ─────────────────────────────────────────────────────────────────────────────

ADiscordInviteChatCommand::ADiscordInviteChatCommand()
{
	CommandName          = TEXT("discord");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer  = true;
	Usage = NSLOCTEXT("DiscordBridge", "DiscordInviteUsage",
		"/discord  — Show the Discord server invite link");
}

EExecutionStatus ADiscordInviteChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& Label)
{
	UDiscordBridgeSubsystem* Bridge = GetBridge(this);
	if (!Bridge)
	{
		Sender->SendChatMessage(
			TEXT("[DiscordBridge] Discord bridge is not active."),
			FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	const FString InviteUrl = Bridge->GetDiscordInviteUrl();
	if (InviteUrl.IsEmpty())
	{
		Sender->SendChatMessage(
			TEXT("[DiscordBridge] No Discord invite URL has been configured."),
			FLinearColor::Yellow);
	}
	else
	{
		Sender->SendChatMessage(
			FString::Printf(TEXT("[DiscordBridge] Join our Discord: %s"), *InviteUrl),
			FLinearColor::White);
	}
	return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
// AInGameWhitelistChatCommand  /ingamewhitelist <subcommand> [args...]
// ─────────────────────────────────────────────────────────────────────────────

AInGameWhitelistChatCommand::AInGameWhitelistChatCommand()
{
	CommandName          = TEXT("ingamewhitelist");
	MinNumberOfArguments = 1;
	bOnlyUsableByPlayer  = false;
	Usage = NSLOCTEXT("DiscordBridge", "InGameWhitelistUsage",
		"/ingamewhitelist <on|off|add|remove|list|status> [name]  — Manage the server whitelist");
}

EExecutionStatus AInGameWhitelistChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& Label)
{
	UDiscordBridgeSubsystem* Bridge = GetBridge(this);
	if (!Bridge)
	{
		Sender->SendChatMessage(
			TEXT("[DiscordBridge] Discord bridge is not active."),
			FLinearColor::Red);
		return EExecutionStatus::UNCOMPLETED;
	}

	// Reconstruct the sub-command string (verb + optional name argument).
	const FString SubCommand = FString::Join(Arguments, TEXT(" ")).TrimStartAndEnd();
	Bridge->HandleInGameWhitelistCommand(SubCommand);
	return EExecutionStatus::COMPLETED;
}

// ─────────────────────────────────────────────────────────────────────────────
// ADiscordCommandsListChatCommand  /commands
// ─────────────────────────────────────────────────────────────────────────────

ADiscordCommandsListChatCommand::ADiscordCommandsListChatCommand()
{
	CommandName          = TEXT("commands");
	MinNumberOfArguments = 0;
	bOnlyUsableByPlayer  = true;
	Usage = NSLOCTEXT("DiscordBridge", "CommandsListUsage",
		"/commands  — List all available DiscordBridge in-game commands");
}

EExecutionStatus ADiscordCommandsListChatCommand::ExecuteCommand_Implementation(
	UCommandSender* Sender,
	const TArray<FString>& Arguments,
	const FString& Label)
{
	UDiscordBridgeSubsystem* Bridge = GetBridge(this);

	Sender->SendChatMessage(
		TEXT("[DiscordBridge] Available commands:"),
		FLinearColor::White);

	Sender->SendChatMessage(
		TEXT("  /discord           — Show the Discord server invite link"),
		FLinearColor::White);

	// Only advertise /verify when verification is enabled on this server.
	if (Bridge && Bridge->IsVerificationEnabled())
	{
		Sender->SendChatMessage(
			TEXT("  /verify <code>     — Link your Discord account (use /whitelist link in Discord first)"),
			FLinearColor::White);
	}

	Sender->SendChatMessage(
		TEXT("  /commands          — Show this command list"),
		FLinearColor::White);

	return EExecutionStatus::COMPLETED;
}
