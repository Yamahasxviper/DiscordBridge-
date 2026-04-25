// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Dom/JsonObject.h"
#include "IDiscordBridgeProvider.h"
#include "BanBridgeConfig.h"
#include "BanDiscordSubsystem.generated.h"

class AGameModeBase;
class UMuteRegistry;

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
 *   mutelist, mutereason, announce, stafflist, staffchat, freeze
 *
 * /player (admin)
 *   history, note, notes, reason, playtime, reputation
 *
 * /appeal (admin)
 *   list, dismiss, approve, deny
 *
 * /admin (admin)
 *   say, poll, reloadconfig, panel, clearchat
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
	 *  ban/mod/warn/player/appeal/admin command groups.
	 *  Also dispatches MESSAGE_COMPONENT (type 3) and MODAL_SUBMIT (type 5)
	 *  interactions whose custom_ids begin with "panel:" / "panel_modal:"
	 *  respectively to the admin panel handlers. */
	void OnDiscordInteraction(const TSharedPtr<FJsonObject>& InteractionObj);

	// ── Authorisation / extraction helpers ───────────────────────────────────

	/** Extract the list of Discord role snowflakes from the message's member object. */
	static TArray<FString> ExtractMemberRoles(const TSharedPtr<FJsonObject>& MessageObj);

	/** Extract the sender's display name (nick > global_name > username). */
	static FString ExtractSenderName(const TSharedPtr<FJsonObject>& MessageObj);

	/** Extract the Discord user ID (snowflake) of the sender from an interaction object. */
	static FString ExtractSenderId(const TSharedPtr<FJsonObject>& MessageObj);

	/** Returns true when the member's role list contains Config.AdminRoleId. */
	bool IsAdminMember(const TArray<FString>& Roles) const;

	/** Returns true when the member's role list contains AdminRoleId or ModeratorRoleId. */
	bool IsModeratorMember(const TArray<FString>& Roles) const;

	/**
	 * Resolve the in-game broadcast prefix for a staff action based on the
	 * acting member's Discord roles.
	 * Returns "[Admin]" for admins, "[Moderator]" for mods-only, "[Staff]" as
	 * a safe fallback (should not occur in practice since commands are gated).
	 */
	FString ResolveStaffPrefix(const TArray<FString>& Roles) const;

	/** Post-login hook used to remind players of active moderation status on rejoin. */
	void OnPostLoginModerationReminder(AGameModeBase* GameMode, APlayerController* Controller);

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
	                      bool bTemporary,
	                      const FString& StaffPrefix);

	/** Handle /ban remove. Usage: /ban remove <uid> */
	void HandleUnbanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName,
	                        const FString& StaffPrefix);

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
	                       const FString& SenderName,
	                       const FString& StaffPrefix);

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
	void HandleKickCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName, const FString& StaffPrefix);

	/** Handle /mod modban. Usage: /mod modban <player> [reason] */
	void HandleModBanCommand(const TArray<FString>& Args,
	                          const FString& ChannelId,
	                          const FString& SenderName);

	/** Handle /mod mute and /mod unmute. Usage: /mod mute <player> [minutes] [reason] */
	void HandleMuteCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName, bool bMute, const FString& StaffPrefix);

	/** Handle /mod tempmute. Usage: /mod tempmute <player> <minutes> */
	void HandleTempMuteCommand(const TArray<FString>& Args,
	                            const FString& ChannelId,
	                            const FString& SenderName,
	                            const FString& StaffPrefix);

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
	                              const FString& SenderName,
	                              const FString& StaffPrefix);

	/**
	 * Handle /mod mutereason.
	 * Updates the reason stored on an existing active mute without lifting it.
	 * Usage: /mod mutereason <player> <new_reason>
	 */
	void HandleMuteReasonCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName,
	                              const FString& StaffPrefix);

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

	/** Handle /mod freeze.  Toggles movement freeze for a player.
	 *  Usage: /mod freeze <player> */
	void HandleFreezeCommand(const TArray<FString>& Args,
	                          const FString& ChannelId,
	                          const FString& SenderName);

	/** Handle /admin clearchat.  Flushes in-game chat and notifies Discord.
	 *  Usage: /admin clearchat [reason] */
	void HandleClearChatCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	// ── Admin panel ───────────────────────────────────────────────────────────

	/**
	 * Handle /admin panel.
	 * Posts an interactive embed with action-row buttons to ChannelId (or
	 * AdminPanelChannelId when configured).  The response is sent as a
	 * non-ephemeral type-4 interaction callback so the panel is visible
	 * to all users in the channel.
	 *
	 * AuthorId is the Discord user snowflake used for per-user rate-limiting.
	 * Pass an empty string to bypass the rate-limit (e.g. auto-post on startup).
	 */
	void HandlePanelCommand(const FString& ChannelId,
	                        const FString& InteractionId,
	                        const FString& InteractionToken,
	                        const TArray<FString>& MemberRoles,
	                        const FString& SenderName,
	                        const FString& AuthorId = FString());

	/**
	 * Handle MESSAGE_COMPONENT (type 3) interactions whose custom_id starts
	 * with "panel:".  Direct-action buttons produce an immediate ephemeral
	 * response; input-action buttons open a multi-field modal.
	 */
	void HandlePanelButtonInteraction(const TSharedPtr<FJsonObject>& InteractionObj);

	/**
	 * Handle MODAL_SUBMIT (type 5) interactions whose custom_id starts with
	 * "panel_modal:".  Extracts field values, executes the requested action,
	 * and responds ephemerally with the result.
	 */
	void HandlePanelModalSubmit(const TSharedPtr<FJsonObject>& InteractionObj);

	/** Build the JSON "data" object for the panel interaction response.
	 *  Includes the embed (title, description, colour, timestamp, dashboard fields)
	 *  and three action rows of buttons. */
	TSharedPtr<FJsonObject> BuildPanelData() const;

	// ── Panel action executors ────────────────────────────────────────────────
	// Each function performs the core action and returns a human-readable
	// result string for the ephemeral interaction reply.  The caller is
	// responsible for posting to ModerationLogChannelId when appropriate.

	/** Kick a player by name/PUID.  Returns the result message. */
	FString ExecutePanelKick(const FString& PlayerArg, const FString& Reason,
	                         const FString& SenderName, const FString& StaffPrefix);

	/** Permanently ban a player by name/PUID.  Returns the result message. */
	FString ExecutePanelBan(const FString& PlayerArg, const FString& Reason,
	                        const FString& SenderName, const FString& StaffPrefix);

	/** Temporarily ban a player.  Returns the result message. */
	FString ExecutePanelTempBan(const FString& PlayerArg, const FString& DurationArg,
	                             const FString& Reason, const FString& SenderName,
	                             const FString& StaffPrefix);

	/** Issue a warning to a player.  Returns the result message. */
	FString ExecutePanelWarn(const FString& PlayerArg, const FString& Reason,
	                         const FString& SenderName, const FString& StaffPrefix);

	/** Mute a player.  Returns the result message. */
	FString ExecutePanelMute(const FString& PlayerArg, const FString& DurationArg,
	                         const FString& Reason, const FString& SenderName,
	                         const FString& StaffPrefix);

	/** Broadcast an in-game announcement.  Returns the result message. */
	FString ExecutePanelAnnounce(const FString& Message, const FString& SenderName);

	/** Return page-1 of the active ban list as a formatted string. */
	FString ExecutePanelBanList() const;

	/** Return the list of currently connected players as a formatted string. */
	FString ExecutePanelPlayers() const;

	/** Return the list of online staff members as a formatted string. */
	FString ExecutePanelStaffList() const;

	/** Reload BanBridge config and return a result message. */
	FString ExecutePanelReloadConfig(const FString& SenderName);

	/** Remove an active ban by player name/PUID.  Returns the result message. */
	FString ExecutePanelUnban(const FString& PlayerArg, const FString& SenderName, const FString& StaffPrefix);

	/** Remove an active mute by player name/PUID.  Returns the result message. */
	FString ExecutePanelUnmute(const FString& PlayerArg, const FString& SenderName, const FString& StaffPrefix);

	/** Look up a player's ban status and return a formatted result string. */
	FString ExecutePanelBanCheck(const FString& PlayerArg) const;
	FString ExecutePanelWarnList(const FString& PlayerArg) const;
	FString ExecutePanelMuteCheck(const FString& PlayerArg) const;
	FString ExecutePanelMuteList() const;
	FString ExecutePanelHistory(const FString& PlayerArg) const;
	FString ExecutePanelAppeals() const;

	/** Toggle movement freeze for a player.  Returns the result message. */
	FString ExecutePanelFreeze(const FString& PlayerArg, const FString& SenderName);

	/** Flush in-game chat and notify staff.  Returns the result message. */
	FString ExecutePanelClearChat(const FString& Reason, const FString& SenderName);

	// ── State ─────────────────────────────────────────────────────────────────

	/** Loaded config (populated in Initialize()). */
	FBanBridgeConfig Config;

	/**
	 * ServerName from FDiscordBridgeConfig, cached at Initialize() and on
	 * config reload to avoid reading the config file from disk every panel refresh.
	 */
	FString CachedDiscordServerName;

	/**
	 * BanEventsChannelId from FDiscordBridgeConfig, cached at Initialize() and on
	 * config reload so PostModerationLog() never hits the disk in the fallback path.
	 */
	FString CachedBanEventsChannelId;

	/** Injected Discord provider — nullptr until SetProvider() is called. */
	IDiscordBridgeProvider* CachedProvider = nullptr;

	/** Handle for the INTERACTION_CREATE subscription; valid while CachedProvider is set. */
	FDelegateHandle InteractionDelegateHandle;

	/** Handle for the UBanAppealRegistry::OnBanAppealSubmitted subscription. */
	FDelegateHandle AppealSubmittedDelegateHandle;

	/** Handle for UBanDatabase::OnBanAdded. Active for the lifetime of the subsystem
	 *  so that bans issued via the REST API or in-game commands are mirrored to the
	 *  bot's ModerationLogChannelId (the Discord-bot command handler already does its
	 *  own posting, so the handler skips entries while a bot interaction is in flight). */
	FDelegateHandle BanAddedHandle;

	/**
	 * Handle for UBanDatabase::OnBanRemoved.
	 * Mirrors REST API / BanEnforcer expiry unbans to the moderation log channel
	 * so that staff see all unban events, not just those triggered by Discord
	 * slash commands.  The handler skips while a bot interaction is in flight
	 * (the unban command handler already posts its own message in that case).
	 */
	FDelegateHandle BanRemovedHandle;

	/**
	 * Handle for UPlayerWarningRegistry::OnWarningAdded.
	 * Routes in-game /warn (BanChatCommands) and chat-filter auto-warns
	 * (DiscordBridgeSubsystem) into the per-player moderation thread without
	 * direct coupling between those systems and BanDiscordSubsystem.
	 * The handler is guarded by PendingInteractionToken so Discord slash /warn
	 * (which already calls PostToPlayerModerationThread directly) does not
	 * cause duplicate thread messages.
	 */
	FDelegateHandle WarnAddedHandle;

	/**
	 * Handle for UMuteRegistry::OnPlayerMuted (instance delegate).
	 * Routes in-game /mute (BanChatCommands) to per-player moderation threads.
	 * Guarded by PendingInteractionToken — Discord slash /mod mute already
	 * calls PostToPlayerModerationThread directly.
	 */
	FDelegateHandle MutedEventHandle;

	/**
	 * Handle for UMuteRegistry::OnPlayerUnmuted (instance delegate).
	 * Routes in-game /unmute (BanChatCommands) to per-player moderation threads.
	 * Guarded by PendingInteractionToken — Discord slash /mod unmute already
	 * calls PostToPlayerModerationThread directly.
	 */
	FDelegateHandle UnmutedEventHandle;

	/**
	 * Weak pointer to the UMuteRegistry that owns MutedEventHandle and
	 * UnmutedEventHandle.  Stored at bind time so Deinitialize() can reliably
	 * remove the handles even when GetGameInstance() returns null during
	 * world teardown.
	 */
	TWeakObjectPtr<UMuteRegistry> BoundMuteRegistry;

	/**
	 * Handle for FBanDiscordNotifier::OnPlayerKickedLogged.
	 * Routes in-game /kick (BanChatCommands) to per-player moderation threads.
	 * The delegate is only broadcast when a Uid is supplied to NotifyPlayerKicked(),
	 * which Discord slash handlers intentionally omit — so no deduplication guard
	 * is needed here; double-posting is structurally impossible.
	 */
	FDelegateHandle KickLoggedHandle;

	/** Handle for the AStaffChatCommand::OnInGameStaffChat subscription.
	 *  Active when CachedProvider is set and StaffChatChannelId is non-empty. */
	FDelegateHandle StaffChatDelegateHandle;

	/** Handle for the raw Discord MESSAGE_CREATE subscription used to relay
	 *  messages from StaffChatChannelId to in-game staff. */
	FDelegateHandle RawMessageDelegateHandle;

	/**
	 * Cache of player thread IDs: "PlayerUID" → Discord thread channel ID.
	 * Populated lazily when PostToPlayerModerationThread creates a new thread.
	 * Not persisted across restarts (threads are reused by name-search when missing).
	 */
	TMap<FString, FString> PlayerThreadIdCache;

	/**
	 * Rate-limit tracker for /admin panel: Discord user ID → last post timestamp.
	 * Prevents a single staff member from spamming AdminPanelChannelId within
	 * PanelRateLimitSeconds (60 s).
	 */
	TMap<FString, FDateTime> LastPanelPostByUser;

	/** Minimum seconds between /admin panel posts from the same Discord user. */
	static constexpr float PanelRateLimitSeconds = 60.f;

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

	/** Broadcast a moderation notice to in-game chat (all connected players). */
	void BroadcastInGameModerationNotice(const FString& Message) const;

	/** Send a moderation notice to a specific connected player (by compound UID). */
	void SendInGameModerationNoticeToUid(const FString& Uid, const FString& Message) const;

	/** FGameModeEvents::GameModePostLoginEvent delegate for rejoin reminders. */
	FDelegateHandle PostLoginHandle;
};
