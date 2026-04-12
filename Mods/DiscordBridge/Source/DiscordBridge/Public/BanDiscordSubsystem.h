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
 * BanChatCommands features to Discord staff via plain-text commands prefixed with "!".
 *
 * Admin Commands (require AdminRoleId)
 * ─────────────────────────────────────
 *  !ban <PUID|name> [reason]               – Permanently ban a player.
 *  !tempban <PUID|name> <min> [reason]     – Temporarily ban a player.
 *  !unban <PUID>                           – Remove an active ban by PUID.
 *  !unbanname <name>                       – Remove a ban by display name.
 *  !banname <name> [reason]                – Ban by display name (EOS + IP).
 *  !bancheck <PUID|name>                   – Check a player's ban status.
 *  !banreason <PUID|name> <new reason...>  – Edit the reason on an active ban.
 *  !banlist [page]                         – List all active bans (10 per page).
 *  !linkbans <UID1> <UID2>                 – Link two UIDs so a ban blocks both.
 *  !unlinkbans <UID1> <UID2>               – Remove a link between two UIDs.
 *  !extend <PUID|name> <minutes>           – Extend a temporary ban.
 *  !duration <PUID|name>                   – Show remaining time on a tempban.
 *  !playerhistory <name|PUID|IP>           – Show known session records.
 *  !warn <PUID|name> <reason...>           – Issue a formal warning.
 *  !warnings <PUID|name>                   – List all warnings for a player.
 *  !clearwarns <PUID|name>                 – Remove all warnings for a player.
 *  !clearwarn <warning_id>                 – Remove a single warning by ID.
 *  !note <PUID|name> <text...>             – Add a private admin note.
 *  !notes <PUID|name>                      – List all admin notes for a player.
 *  !reason <UID>                           – Show the ban reason for a UID.
 *  !mutereason <PUID|name> <new reason...> – Update the reason on an active mute.
 *   *  !appeals                               – List pending ban appeals (max 10).
 *  !dismissappeal <id>                    – Delete a ban appeal by integer ID.
!reloadconfig                           – Reload BanBridge config from disk.
 *
 * Moderator Commands (require ModeratorRoleId or AdminRoleId)
 * ────────────────────────────────────────────────────────────
 *  !kick <PUID|name> [reason]              – Kick a connected player.
 *  !modban <PUID|name> [reason]            – Temporary ban (ModBanDurationMinutes).
 *  !mute <PUID|name> [minutes] [reason]    – Mute a player's in-game chat.
 *  !unmute <PUID|name>                     – Lift a mute from a player.
 *  !tempmute <PUID|name> <minutes>         – Apply a timed mute.
 *  !tempunmute <PUID|name>                 – Lift a timed mute (fails for indefinite mutes).
 *  !mutecheck <PUID|name>                  – Check mute status and expiry.
 *  !mutelist                               – List all currently muted players.
 *  !announce <message...>                  – Broadcast to all in-game players.
 *  !stafflist                              – Show online admins/moderators.
 *  !staffchat <message...>                 – Send a message to online staff only.
 *
 * Authorisation
 * ─────────────
 *  Admin commands require AdminRoleId.  Moderator commands accept AdminRoleId
 *  or ModeratorRoleId.  Both roles are configured in DefaultBanBridge.ini.
 *  If BanCommandChannelId is set, commands are only accepted from that channel.
 *
 * Player target resolution
 * ────────────────────────
 *  1. 32-character hex string → raw EOS PUID → "EOS:<lower>"
 *  2. "EOS:<32hex>"           → compound UID (normalised to lowercase)
 *  3. Anything else           → substring name lookup in PlayerSessionRegistry.
 *
 * Integration
 * ───────────
 *  DiscordBridgeSubsystem calls SetProvider(this) during its own Initialize()
 *  and SetProvider(nullptr) during Deinitialize().
 *
 * Configuration
 * ─────────────
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
	// ── Message routing ───────────────────────────────────────────────────────

	/** Subscribed to the provider's raw MESSAGE_CREATE stream via SetProvider(). */
	void OnRawDiscordMessage(const TSharedPtr<FJsonObject>& MessageObj);

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
	 * Handle !ban and !tempban.
	 * Usage: !ban <PUID|name> [reason]
	 *        !tempban <PUID|name> <minutes> [reason]
	 */
	void HandleBanCommand(const TArray<FString>& Args,
	                      const FString& ChannelId,
	                      const FString& SenderName,
	                      bool bTemporary);

	/** Handle !unban. Usage: !unban <PUID> */
	void HandleUnbanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName);

	/** Handle !unbanname. Usage: !unbanname <name_substring> */
	void HandleUnbanNameCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle !banname. Usage: !banname <name_substring> [reason] */
	void HandleBanNameCommand(const TArray<FString>& Args,
	                           const FString& ChannelId,
	                           const FString& SenderName);

	/** Handle !bancheck. Usage: !bancheck <PUID|name> */
	void HandleBanCheckCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !banreason. Usage: !banreason <PUID|name> <new reason...> */
	void HandleBanReasonCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle !banlist. Usage: !banlist [page] */
	void HandleBanListCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !linkbans / !unlinkbans. Usage: !linkbans <UID1> <UID2> */
	void HandleLinkBansCommand(const TArray<FString>& Args,
	                            const FString& ChannelId,
	                            const FString& SenderName,
	                            bool bLink);

	/** Handle !extend. Usage: !extend <PUID|name> <minutes> */
	void HandleExtendBanCommand(const TArray<FString>& Args,
	                             const FString& ChannelId,
	                             const FString& SenderName);

	/** Handle !duration. Usage: !duration <PUID|name> */
	void HandleDurationCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !playerhistory. Usage: !playerhistory <name|PUID|IP> */
	void HandlePlayerHistoryCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !warn. Usage: !warn <PUID|name> <reason...> */
	void HandleWarnCommand(const TArray<FString>& Args,
	                       const FString& ChannelId,
	                       const FString& SenderName);

	/** Handle !warnings. Usage: !warnings <PUID|name> */
	void HandleWarningsCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !clearwarns. Usage: !clearwarns <PUID|name> */
	void HandleClearWarnsCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/** Handle !clearwarn. Usage: !clearwarn <warning_id> */
	void HandleClearWarnByIdCommand(const TArray<FString>& Args,
	                                 const FString& ChannelId,
	                                 const FString& SenderName);

	/** Handle !note. Usage: !note <PUID|name> <text...> */
	void HandleNoteCommand(const TArray<FString>& Args,
	                       const FString& ChannelId,
	                       const FString& SenderName);

	/** Handle !notes. Usage: !notes <PUID|name> */
	void HandleNotesCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !reason. Usage: !reason <UID> */
	void HandleReasonCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !reloadconfig. Reloads BanBridge config from disk. */
	void HandleReloadConfigCommand(const FString& ChannelId, const FString& SenderName);

	/** Handle !appealapprove <id>. Approves an appeal: unbans the player and deletes the appeal. Requires AdminRoleId. */
	void HandleAppealApproveCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle !appealdeny <id>. Denies an appeal: deletes it without unbanning. Requires AdminRoleId. */
	void HandleAppealDenyCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/**
	 * Handle !appeals.
	 * Lists all pending ban appeals from BanAppealRegistry (at most 10, then "(more)").
	 * Format:
	 *   :scales: **Pending Ban Appeals (N):**
	 *   `#1` uid=Discord:123... | contact: … | submitted: 2026-01-01 | reason: …
	 * Requires AdminRoleId.
	 */
	void HandleAppealsCommand(const FString& ChannelId);

	/**
	 * Handle !dismissappeal <id>.
	 * Deletes the appeal with the given integer ID from BanAppealRegistry.
	 * Replies with success or failure.
	 * Requires AdminRoleId.
	 */
	void HandleDismissAppealCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle !kick. Usage: !kick <PUID|name> [reason] */
	void HandleKickCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle !modban. Usage: !modban <PUID|name> [reason] */
	void HandleModBanCommand(const TArray<FString>& Args,
	                          const FString& ChannelId,
	                          const FString& SenderName);

	/** Handle !mute / !unmute. Usage: !mute <PUID|name> [minutes] [reason] */
	void HandleMuteCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName, bool bMute);

	/** Handle !tempmute. Usage: !tempmute <PUID|name> <minutes> */
	void HandleTempMuteCommand(const TArray<FString>& Args,
	                            const FString& ChannelId,
	                            const FString& SenderName);

	/** Handle !mutecheck. Usage: !mutecheck <PUID|name> */
	void HandleMuteCheckCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !mutelist. Lists all currently muted players. */
	void HandleMuteListCommand(const FString& ChannelId);

	/**
	 * Handle !tempunmute.
	 * Lifts a *timed* mute from a player.  Fails with an error when the mute
	 * is indefinite (use !unmute for those).
	 * Usage: !tempunmute <PUID|name>
	 */
	void HandleTempUnmuteCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/**
	 * Handle !mutereason.
	 * Updates the reason stored on an existing active mute without lifting it.
	 * Usage: !mutereason <PUID|name> <new reason...>
	 */
	void HandleMuteReasonCommand(const TArray<FString>& Args,
	                              const FString& ChannelId,
	                              const FString& SenderName);

	/** Handle !announce. Usage: !announce <message...> */
	void HandleAnnounceCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/** Handle !stafflist. Lists names of online admins/moderators. */
	void HandleStaffListCommand(const FString& ChannelId);

	/** Handle !staffchat. Usage: !staffchat <message...> */
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

	/** Handle !playtime. Usage: !playtime <PUID|name> */
	void HandlePlaytimeCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !say. Usage: !say <message...> — broadcasts as [ADMIN] in-game */
	void HandleSayCommand(const TArray<FString>& Args,
	                      const FString& ChannelId,
	                      const FString& SenderName);

	/** Handle !poll. Usage: !poll <question> | <optionA> | <optionB> [...] */
	void HandlePollCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !scheduleban. Usage: !scheduleban <player|PUID> <delay> [banDuration] [reason...] */
	void HandleScheduleBanCommand(const TArray<FString>& Args,
	                               const FString& ChannelId,
	                               const FString& SenderName);

	/** Handle !qban. Usage: !qban <templateSlug> <player|PUID> */
	void HandleQBanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName);

	/** Handle !reputation. Usage: !reputation <player|PUID> */
	void HandleReputationCommand(const TArray<FString>& Args, const FString& ChannelId);

	/** Handle !bulkban. Usage: !bulkban <PUID1> ... -- <reason> */
	void HandleBulkBanCommand(const TArray<FString>& Args,
	                           const FString& ChannelId,
	                           const FString& SenderName);

	// ── State ─────────────────────────────────────────────────────────────────

	/** Loaded config (populated in Initialize()). */
	FBanBridgeConfig Config;

	/** Injected Discord provider — nullptr until SetProvider() is called. */
	IDiscordBridgeProvider* CachedProvider = nullptr;

	/** Handle for the raw-message subscription; valid while CachedProvider is set. */
	FDelegateHandle RawMessageDelegateHandle;

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
	 * Empty when handling chat (!) commands — Respond() skips the follow-up.
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
