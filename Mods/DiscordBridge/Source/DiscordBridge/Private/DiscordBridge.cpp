// Copyright Coffee Stain Studios. All Rights Reserved.

#include "DiscordBridge.h"
#include "Modules/ModuleManager.h"
#include "Patching/NativeHookManager.h"
#include "FGLocalPlayer.h"

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
	// UFGLocalPlayer::PlayerAdded() calls RequestPublicPlayerAddress(), which
	// fires an HTTP GET to https://api.ipify.org/?format=json to look up the
	// machine's public IP for the client watermark UI.  On a dedicated server
	// there is no watermark, but the game still creates a UFGLocalPlayer for the
	// EOS service account, so the request is made unconditionally.  Because the
	// response never arrives (the server has no listener waiting for it), the
	// request stays in-flight for the entire session.  At shutdown, this causes:
	//
	//   LogHttp Warning: [FHttpManager::Shutdown] Unbinding delegates for 1
	//       outstanding Http Requests:
	//       verb=[GET] url=[https://api.ipify.org/?format=json]
	//   LogHttp Warning: Sleeping 0.500s to wait for 1 outstanding Http Requests.
	//
	// RequestPublicPlayerAddress() is a private, non-virtual method of
	// UFGLocalPlayer.  The friend declaration in AccessTransformers.ini:
	//
	//   Friend=(Class="UFGLocalPlayer", FriendClass="FDiscordBridgeModule")
	//
	// gives FDiscordBridgeModule access to private members of UFGLocalPlayer,
	// allowing SUBSCRIBE_METHOD to form a pointer to the function and patch it
	// directly.  This is more targeted than the previous approach of intercepting
	// every IHttpRequest::ProcessRequest() call and filtering by URL.
	if ( IsRunningDedicatedServer() )
	{
		SUBSCRIBE_METHOD( UFGLocalPlayer::RequestPublicPlayerAddress,
			[]( auto& Scope, UFGLocalPlayer* Self )
			{
				// On a dedicated server the watermark UI is absent; suppress
				// the ipify request entirely to avoid log noise at shutdown.
				Scope.Override();
				UE_LOG( LogDiscordBridge, Log,
					TEXT( "DiscordBridge: Suppressed RequestPublicPlayerAddress on "
					      "dedicated server (watermark feature is client-only)." ) );
			} );
	}
}

void FDiscordBridgeModule::ShutdownModule()
{
}

IMPLEMENT_MODULE(FDiscordBridgeModule, DiscordBridge)

