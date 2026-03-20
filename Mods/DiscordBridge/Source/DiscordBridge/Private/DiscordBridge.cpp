// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridge.h"
#include "Modules/ModuleManager.h"

DEFINE_LOG_CATEGORY(LogDiscordBridge);

void FDiscordBridgeModule::StartupModule()
{
	// ── Ban enforcement ────────────────────────────────────────────────────────
	// Platform-ID and name-based ban enforcement is handled by
	// UDiscordBridgeSubsystem::OnPostLogin, which is bound to
	// FGameModeEvents::GameModePostLoginEvent in InitializeServer().
	//
	// A previous implementation attempted to hook AFGGameMode::Login via
	// SUBSCRIBE_METHOD_VIRTUAL to reject banned players at the pre-login stage
	// (before APlayerController creation).  That approach caused a Fatal crash
	// on server startup because the CSS binary's AFGGameMode::Login compiled to
	// fewer machine-code bytes than funchook requires to place its trampoline
	// ("Too short instructions" error in NativeHookManager.cpp:61).
	//
	// The available SML hooks (UModNetworkHandler::OnClientInitialJoin_Server
	// and OnWelcomePlayer) fire before APlayerController exists and carry no
	// platform-ID information (Steam64 / EOS PUID), so they cannot be used for
	// platform-ID ban enforcement.  The earliest reliable source of a player's
	// platform ID is the UOnlineIntegrationControllerComponent, which receives
	// the ID via a Server RPC from the client after PostLogin (typically <1 s).
	//
	// OnPostLogin ban enforcement strategy:
	//   1. Steam64 bans: checked immediately at PostLogin via PS->GetUniqueId(),
	//      which is safe to call without EOS being fully operational.
	//   2. EOS PUID bans: checked immediately when EOS is ready; when EOS is
	//      still initialising a 60 s deferred-retry timer runs.  NotifyPlayer-
	//      Joined (and the Discord "joined" message) are withheld until the ban
	//      result is known, preventing banned players from being announced as
	//      joined or interacting with the server during the resolution window.
	//   3. Name-based bans: checked immediately at PostLogin.

	// ── ipify.org public-IP request suppression ────────────────────────────────
	// The UFGLocalPlayer::RequestPublicPlayerAddress hook that was previously
	// installed here has been moved to CSSCompatStubs/EOSShared (see
	// Mods/CSSCompatStubs/Source/EOSShared/Private/EOSSharedModule.cpp).
	//
	// CSSCompatStubs loads at PostConfigInit — before DiscordBridge's Default
	// loading phase — so the suppression is active before the engine creates the
	// EOS service-account UFGLocalPlayer instance.  Moving the hook to
	// CSSCompatStubs makes it a general-purpose server-compat patch that benefits
	// any server mod depending on CSSCompatStubs, not just DiscordBridge.
}

void FDiscordBridgeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDiscordBridgeModule, DiscordBridge)

