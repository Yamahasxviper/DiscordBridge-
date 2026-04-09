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
 * A GameInstance-level subsystem that exposes the BanSystem ban database to
 * Discord administrators via plain-text commands prefixed with "!".
 *
 * Commands
 * ────────
 *  !ban <PUID|name> [reason]             – Permanently ban a player.
 *  !tempban <PUID|name> <minutes> [reason] – Temporarily ban a player.
 *  !unban <PUID>                         – Remove an active ban.
 *  !bancheck <PUID|name>                 – Check a player's ban status.
 *  !banlist [page]                       – List all active bans (10 per page).
 *  !playerhistory <name|PUID>            – Show known session records for a player.
 *  !kick <PUID|name> [reason]            – Kick a connected player.
 *  !mute <PUID|name> [minutes] [reason]  – Mute a player (requires BanChatCommands).
 *  !unmute <PUID|name>                   – Unmute a player (requires BanChatCommands).
 *  !warn <PUID|name> <reason...>         – Issue a warning (requires BanSystem).
 *  !announce <message...>                – Broadcast a message to all in-game players.
 *
 * Authorisation
 * ─────────────
 *  Only Discord members whose role list contains AdminRoleId
 *  (configured in DefaultBanBridge.ini) may run these commands.
 *  If BanCommandChannelId is set, commands are only accepted from that channel.
 *
 * Player target resolution
 * ────────────────────────
 *  1. 32-character hex string → raw EOS Product User ID → "EOS:<lower>"
 *  2. "EOS:<32hex>"           → compound UID (normalised to lowercase)
 *  3. Anything else           → substring name lookup in the PlayerSessionRegistry
 *                              (covers both currently-connected players and
 *                               players seen in previous sessions).
 *
 * Integration
 * ───────────
 *  DiscordBridgeSubsystem calls SetProvider(this) during its own Initialize()
 *  and SetProvider(nullptr) during Deinitialize(), following the same pattern
 *  as UTicketSubsystem.
 *
 * Configuration
 * ─────────────
 *  Edit Mods/DiscordBridge/Config/DefaultBanBridge.ini and set AdminRoleId to
 *  your Discord admin role snowflake ID.  Optionally set BanCommandChannelId to
 *  restrict commands to a single channel.
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

	// ── Authorisation / extraction helpers ───────────────────────────────────

	/** Extract the list of Discord role snowflakes from the message's member object. */
	static TArray<FString> ExtractMemberRoles(const TSharedPtr<FJsonObject>& MessageObj);

	/** Extract the sender's display name (nick > global_name > username). */
	static FString ExtractSenderName(const TSharedPtr<FJsonObject>& MessageObj);

	/** Returns true when the member's role list contains Config.AdminRoleId. */
	bool IsAdminMember(const TArray<FString>& Roles) const;

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

	/**
	 * Handle !unban.
	 * Usage: !unban <PUID>
	 */
	void HandleUnbanCommand(const TArray<FString>& Args,
	                        const FString& ChannelId,
	                        const FString& SenderName);

	/**
	 * Handle !bancheck.
	 * Usage: !bancheck <PUID|name>
	 */
	void HandleBanCheckCommand(const TArray<FString>& Args, const FString& ChannelId);

	/**
	 * Handle !banlist.
	 * Usage: !banlist [page]
	 */
	void HandleBanListCommand(const TArray<FString>& Args, const FString& ChannelId);

	/**
	 * Handle !playerhistory.
	 * Usage: !playerhistory <name|PUID>
	 */
	void HandlePlayerHistoryCommand(const TArray<FString>& Args, const FString& ChannelId);

	void HandleKickCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);
	void HandleMuteCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName, bool bMute);
	void HandleWarnCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);
	void HandleAnnounceCommand(const TArray<FString>& Args, const FString& ChannelId, const FString& SenderName);

	/**
	 * Post a moderation log message to ModerationLogChannelId (if configured).
	 * Call after every successful ban/unban/kick/mute/unmute/warn action.
	 */
	void PostModerationLog(const FString& Message) const;

	// ── State ─────────────────────────────────────────────────────────────────

	/** Loaded config (populated in Initialize()). */
	FBanBridgeConfig Config;

	/** Injected Discord provider — nullptr until SetProvider() is called. */
	IDiscordBridgeProvider* CachedProvider = nullptr;

	/** Handle for the raw-message subscription; valid while CachedProvider is set. */
	FDelegateHandle RawMessageDelegateHandle;
};
