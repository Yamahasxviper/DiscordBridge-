// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "IDiscordBridgeProvider.h"
#include "BanBridgeConfig.h"
#include "BanDiscordSubsystem.generated.h"

/**
 * UBanDiscordSubsystem
 *
 * A GameInstance-level subsystem that exposes the BanSystem ban database and
 * BanChatCommands features to Discord staff via slash (/) commands.
 *
 * Slash Command Groups (registered by DiscordBridgeSubsystem)
 * ============================================================
 *
 * /ban (admin)
 *   add, temp, remove, removename, byname, check, reason, list,
 *   extend, duration, link, unlink, schedule, quick, bulk
 *
 * /warn (admin)
 *   add, list, clearall, clearone
 *
 * /mod (moderator or admin)
 *   kick, modban, mute, unmute, tempmute, tempunmute, mutecheck,
 *   mutelist, mutereason, announce, stafflist, staffchat
 *
 * /player (admin)
 *   history, note, notes, reason, playtime, reputation
 *
 * /appeal (admin)
 *   list, dismiss, approve, deny
 *
 * /admin (admin)
 *   say, poll, reloadconfig
 *
 * Authorisation
 * =============
 *  Admin commands require AdminRoleId.  Moderator commands accept AdminRoleId
 *  or ModeratorRoleId.  Both roles are configured in DefaultBanBridge.ini.
 *
 * Player target resolution
 * ========================
 *  1. 32-character hex string -> raw EOS PUID -> "EOS:<lower>"
 *  2. "EOS:<32hex>"           -> compound UID (normalised to lowercase)
 *  3. Anything else           -> substring name lookup in PlayerSessionRegistry.
 *
 * Integration
 * ===========
 *  DiscordBridgeSubsystem calls SetProvider(this) during its own Initialize()
 *  and SetProvider(nullptr) during Deinitialize().
 *
 * Configuration
 * =============
 *  Edit Mods/DiscordBridge/Config/DefaultBanBridge.ini.
 */
UCLASS()
class DISCORDBRIDGE_API UBanDiscordSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	// ── USubsystem ────────────────────────────────────────────────────────────

	/** Restrict to dedicated servers only (mirrors UDiscordBridgeSubsystem). */
	virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

	/**
	 * Inject (or clear) the Discord provider.
	 * Called by UDiscordBridgeSubsystem during its own Initialize() /
	 * Deinitialize().  Pass nullptr to detach.
	 */
	void SetProvider(IDiscordBridgeProvider* InProvider);

private:
	// ── Interaction routing ──────────────────────────────────────────────────

	/** Subscribed to the provider's INTERACTION_CREATE stream via SetProvider().
	 *  Handles APPLICATION_COMMAND (type 2) slash command interactions for the
	 *  ban/mod/warn/player/appeal/admin command groups. */
	void OnDiscordInteraction(const TSharedPtr<FJsonObject>& InteractionObj);

	// ── Authorisation / extraction helpers ───────────────────────────────────

	/** Extract the list of Discord role snowflakes from the message's member object. */
	static TArray<FString> ExtractMemberRoles(const TSharedPtr<FJsonObject>& MessageObj);

	/** Extract the sender's display name (nick > global_name > username). */
	static FString ExtractSenderName(const TSharedPtr<FJsonObject>& MessageObj);

	/** Returns true when the member's role list contains Config.AdminRoleId. */
	bool IsAdminMember(const TArray<FString>& Roles) const;

	/** Returns true when the member's role list contains AdminRoleId or ModeratorRoleId. */
	bool IsModeratorMember(const TArray<FString>& Roles) const;

	// ── Target resolution ─────────────────────────────────────────────────────

	/**
	 * Resolve an argument to a compound ban UID.
	 *
	 * Resolution order:
	 *   1. 32-char hex → EOS PUID → "EOS:<lower>"
	 *   2. "EOS:<32hex>" compound UID → normalised lowercase
	 *   3. Name substring → PlayerSessionRegistry lookup
	 *
	 * @param Arg             The raw command argument (PUID, compound UID, or name).
	 * @param OutUid          Populated with the resolved compound UID on success.
	 * @param OutDisplayName  Populated with a human-readable name (may be the PUID
	 *                        itself when no name is known).
	 * @param OutErrorMsg     Populated with a user-facing error on failure (e.g.
	 *                        "No player found matching 'name'").
	 * @return true on unambiguous resolution; false when the argument cannot be
	 *         resolved or the name matches multiple players.
	 */
	bool ResolveTarget(const FString& Arg,
	                   FString& OutUid,
	                   FString& OutDisplayName,
	                   FString& OutErrorMsg) const;

	/** Returns true if Id is a valid 32-character lowercase-hex EOS PUID. */
	static bool IsValidEOSPUID(const FString& Id);
	/** Returns true when Query looks like an IPv4/IPv6 address or partial IP (contains a dot). */
	static bool IsValidIPQuery(const FString& Query);

	/**
	 * Parses a human-readable duration string (e.g. "30m", "2h", "1d", "1w") into minutes.
	 * Returns the parsed duration in minutes, or 0 if the string is invalid.
	 */
	static int32 ParseDurationMinutes(const FString& DurationStr);

	// ── Command handlers ──────────────────────────────────────────────────────

	/**
	 * Handle /ban add and /ban temp.
	 * Usage: /ban add <player> [reason]
	 *        !tempban <PUID|name> <minutes> [reason]
	 */
	void HandleBanCommand(const TArray<FString>& Args,
	                      const FString& ChannelId,
	                      const FString& SenderName,
	                      bool bTemporary);

	/** Handle /ban remove. Usage: /ban remove <uid> */
	void HandleUnbanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName);

	/** Handle /ban removename. Usage: /ban removename <name> */
	void HandleUnbanNameCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle /ban byname. Usage: /ban byname <name> [reason] */
	void HandleBanNameCommand(const TArray<FString>& Args,
	                           const FString& ChannelId,
	                           const FString& SenderName);

	/** Handle /ban check. Usage: /ban check <player> */
	void HandleBanCheckCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /ban reason. Usage: /ban reason <player> <new_reason> */
	void HandleBanReasonCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle /ban list. Usage: /ban list [page] */
	void HandleBanListCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /ban link and /ban unlink. Usage: /ban link <uid1> <uid2> */
	void HandleLinkBansCommand(const TArray<FString>& Args,
	                            const FString& ChannelId,
	                            const FString& SenderName,
	                            bool bLink);

	/** Handle /ban extend. Usage: /ban extend <player> <duration> */
	void HandleExtendBanCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle /ban duration. Usage: /ban duration <player> */
	void HandleDurationCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /player history. Usage: /player history <query> */
	void HandlePlayerHistoryCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /warn add. Usage: /warn add <player> <reason> */
	void HandleWarnCommand(const TArray<FString>& Args,
	                       const FString& ChannelId,
	                       const FString& SenderName);

	/** Handle /warn list. Usage: /warn list <player> */
	void HandleWarningsCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /warn clearall. Usage: /warn clearall <player> */
	void HandleClearWarnsCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/** Handle /warn clearone. Usage: /warn clearone <warning_id> */
	void HandleClearWarnByIdCommand(const TArray<FString>& Args,
	                                 const FString& ChannelId,
	                                 const FString& SenderName);

	/** Handle /player note. Usage: /player note <player> <text> */
	void HandleNoteCommand(const TArray<FString>& Args,
	                       const FString& ChannelId,
	                       const FString& SenderName);

	/** Handle /player notes. Usage: /player notes <player> */
	void HandleNotesCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /player reason. Usage: /player reason <uid> */
	void HandleReasonCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /admin reloadconfig. Reloads BanBridge config from disk. */
	void HandleReloadConfigCommand(const FString& ChannelId, const FString& SenderName);

	/** Handle /appeal approve. Approves an appeal: unbans the player and deletes the appeal. Requires AdminRoleId. */
	void HandleAppealApproveCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle /appeal deny. Denies an appeal: deletes it without unbanning. Requires AdminRoleId. */
	void HandleAppealDenyCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/**
	 * Handle /appeal list.
	 * Lists all pending ban appeals from BanAppealRegistry (at most 10, then "(more)").
	 * Format:
	 *   :scales: **Pending Ban Appeals (N):**
	 *   `#1` uid=Discord:123... | contact: … | submitted: 2026-01-01 | reason: …
	 * Requires AdminRoleId.
	 */
	void HandleAppealsCommand(const FString& ChannelId);

	/**
	 * Handle /appeal dismiss.
	 * Deletes the appeal with the given integer ID from BanAppealRegistry.
	 * Replies with success or failure.
	 * Requires AdminRoleId.
	 */
	void HandleDismissAppealCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle /mod kick. Usage: /mod kick <player> [reason] */
	void HandleKickCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle /mod modban. Usage: /mod modban <player> [reason] */
	void HandleModBanCommand(const TArray<FString>& Args,
	                          const FString& ChannelId,
	                          const FString& SenderName);

	/** Handle /mod mute and /mod unmute. Usage: /mod mute <player> [minutes] [reason] */
	void HandleMuteCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName, bool bMute);

	/** Handle /mod tempmute. Usage: /mod tempmute <player> <minutes> */
	void HandleTempMuteCommand(const TArray<FString>& Args,
	                            const FString& ChannelId,
	                            const FString& SenderName);

	/** Handle /mod mutecheck. Usage: /mod mutecheck <player> */
	void HandleMuteCheckCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /mod mutelist. Lists all currently muted players. */
	void HandleMuteListCommand(const FString& ChannelId);

	/**
	 * Handle /mod tempunmute.
	 * Lifts a *timed* mute from a player.  Fails with an error when the mute
	 * is indefinite (use /mod unmute for those).
	 * Usage: !tempunmute <PUID|name>
	 */
	void HandleTempUnmuteCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/**
	 * Handle /mod mutereason.
	 * Updates the reason stored on an existing active mute without lifting it.
	 * Usage: /mod mutereason <player> <new_reason>
	 */
	void HandleMuteReasonCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/** Handle /mod announce. Usage: /mod announce <message> */
	void HandleAnnounceCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle /mod stafflist. Lists names of online admins/moderators. */
	void HandleStaffListCommand(const FString& ChannelId);

	/** Handle /mod staffchat. Usage: /mod staffchat <message> */
	void HandleStaffChatCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/**
	 * Post a moderation log message to ModerationLogChannelId (if configured).
	 * Call after every successful ban/unban/kick/mute/unmute/warn action.
	 */
	void PostModerationLog(const FString& Message) const;

	/**
	 * Create or reuse a per-player moderation thread in the ModerationLogChannelId.
	 * When a ban/warn/mute is issued, call this to route the audit entry into a
	 * named thread (e.g. "PlayerName [EOS:xxx]") so all actions on that player
	 * are visible in one place.
	 * Fires-and-forgets; no-op when ModerationLogChannelId is empty.
	 */
	void PostToPlayerModerationThread(const FString& PlayerName, const FString& Uid,
	                                  const FString& Message);

	/** Handle /player playtime. Usage: /player playtime <player> */
	void HandlePlaytimeCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /admin say. Usage: /admin say <message> — broadcasts as [ADMIN] in-game */
	void HandleSayCommand(const TArray<FString>& Args,
	                      const FString& ChannelId,
	                      const FString& SenderName);

	/** Handle /admin poll. Usage: /admin poll <question> <options> */
	void HandlePollCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /ban schedule. Usage: /ban schedule <player> <delay> [ban_duration] [reason] */
	void HandleScheduleBanCommand(const TArray<FString>& Args,
	                               const FString& ChannelId,
	                               const FString& SenderName);

	/** Handle /ban quick. Usage: /ban quick <template> <player> */
	void HandleQBanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName);

	/** Handle /player reputation. Usage: /player reputation <player> */
	void HandleReputationCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle /ban bulk. Usage: /ban bulk <players> <reason> */
	void HandleBulkBanCommand(const TArray<FString>& Args,
	                           const FString& ChannelId,
	                           const FString& SenderName);

	// ── State ─────────────────────────────────────────────────────────────────

	/** Loaded config (populated in Initialize()). */
	FBanBridgeConfig Config;

	/** Injected Discord provider — nullptr until SetProvider() is called. */
	IDiscordBridgeProvider* CachedProvider = nullptr;

	/** Handle for the INTERACTION_CREATE subscription; valid while CachedProvider is set. */
	FDelegateHandle InteractionDelegateHandle;

	/** Handle for the UBanAppealRegistry::OnBanAppealSubmitted subscription. */
	FDelegateHandle AppealSubmittedDelegateHandle;

	/**
	 * Cache of player thread IDs: "PlayerUID" → Discord thread channel ID.
	 * Populated lazily when PostToPlayerModerationThread creates a new thread.
	 * Not persisted across restarts (threads are reused by name-search when missing).
	 */
	TMap<FString, FString> PlayerThreadIdCache;

	/**
	 * Interaction token for the slash command currently being processed.
	 * Set at the start of OnDiscordInteraction and cleared at the end.
	 * Used by Respond() to send an ephemeral follow-up directly to the admin
	 * in addition to the public channel message.
	 */
	FString PendingInteractionToken;

	/**
	 * Send a command result to the Discord channel AND, if a slash command is
	 * currently being handled (PendingInteractionToken is set), also send the
	 * same message as an ephemeral follow-up to the interaction so the admin
	 * sees the result inline with their /command even if they missed the channel
	 * message.
	 */
	void Respond(const FString& ChannelId, const FString& Message);
};
