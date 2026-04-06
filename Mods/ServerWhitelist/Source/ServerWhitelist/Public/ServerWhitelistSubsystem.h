// Copyright Yamahasxviper. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "Containers/Ticker.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "FGChatManager.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"
#include "ServerWhitelistConfig.h"
#include "ServerWhitelistSubsystem.generated.h"

/**
 * UServerWhitelistSubsystem
 *
 * Standalone whitelist enforcement subsystem for Satisfactory dedicated servers.
 * Created automatically by the GameInstance on dedicated server builds.
 *
 * Features:
 *  – Kicks any player that is not on the whitelist when the whitelist is enabled.
 *  – Handles in-game chat commands (!whitelist add/remove/list/on/off/status)
 *    for players listed in AdminPlayerNames.
 *  – Reads and writes <ProjectSavedDir>/ServerWhitelist.json for persistence.
 *  – Defers to DiscordBridge when that mod is also installed (no double-kick).
 *
 * No Discord bot, SMLWebSocket, or internet connection is required.
 */
UCLASS()
class SERVERWHITELIST_API UServerWhitelistSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// USubsystem overrides
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	// ── PostLogin enforcement ──────────────────────────────────────────────────

	/** Bound to FGameModeEvents::GameModePostLoginEvent. Kicks non-whitelisted players. */
	void OnPostLogin(AGameModeBase* GameMode, APlayerController* Controller);

	// ── In-game chat command handling ─────────────────────────────────────────

	/**
	 * Attempts to locate AFGChatManager and bind to its OnChatMessageAdded delegate.
	 * Returns true when binding succeeds (ticker stops).
	 */
	bool TryBindToChatManager();

	/** Bound to AFGChatManager::OnChatMessageAdded. Diffs message list for new commands. */
	UFUNCTION()
	void OnGameChatMessageAdded();

	/**
	 * Parses and dispatches a !whitelist sub-command.
	 * @param SubCommand  The text after the command prefix, trimmed.
	 * @param SenderName  In-game name of the player who sent the command.
	 */
	void HandleWhitelistCommand(const FString& SubCommand, const FString& SenderName);

	/** Broadcasts a plain-text status message to all players via game chat. */
	void SendGameChatMessage(const FString& Message);

	// ── State ─────────────────────────────────────────────────────────────────

	FServerWhitelistConfig Config;

	FDelegateHandle PostLoginHandle;

	/** Handle for the periodic ticker that polls for AFGChatManager. */
	FTSTicker::FDelegateHandle ChatManagerBindTickHandle;

	/** Weak pointer to the chat manager we are bound to. */
	TWeakObjectPtr<AFGChatManager> BoundChatManager;

	/** Snapshot of messages seen so far – used to detect only new messages. */
	TArray<FChatMessageStruct> LastKnownMessages;
};
