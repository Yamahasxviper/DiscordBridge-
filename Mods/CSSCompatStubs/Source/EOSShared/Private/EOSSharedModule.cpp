// Copyright Coffee Stain Studios. All Rights Reserved.

#include "EOSShared.h"
#include "Modules/ModuleManager.h"
#include "Patching/NativeHookManager.h"
#include "FGLocalPlayer.h"

DEFINE_LOG_CATEGORY_STATIC(LogEOSSharedCompat, Log, All);

void FEOSSharedCompatModule::StartupModule()
{
	// ── ipify.org public-IP request suppression ───────────────────────────────
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
	// UFGLocalPlayer.  SUBSCRIBE_METHOD resolves its address at runtime; the
	// friend declaration in Config/AccessTransformers.ini:
	//
	//   Friend=(Class="UFGLocalPlayer", FriendClass="FEOSSharedCompatModule")
	//
	// gives FEOSSharedCompatModule access to private members of UFGLocalPlayer
	// so that &UFGLocalPlayer::RequestPublicPlayerAddress is accessible here.
	//
	// Scope.Cancel() is the correct cancellation call for void functions; see
	// Mods/DiscordBridge/Docs/12-NativeHooking.md §TCallScope API.
	// (Scope.Override() is only valid for non-void return types.)
	//
	// CSSCompatStubs loads at PostDefault — after SML — so SUBSCRIBE_METHOD
	// (which uses SML's NativeHookManager / funchook) is available, and the
	// hook is still installed before the engine creates any local-player
	// objects (UFGLocalPlayer is created during world initialisation, which
	// happens after all module StartupModules have completed).
	if (IsRunningDedicatedServer())
	{
		PublicIpHookHandle = SUBSCRIBE_METHOD(UFGLocalPlayer::RequestPublicPlayerAddress,
			[](TCallScope<void(*)(UFGLocalPlayer*)>& Scope, UFGLocalPlayer* Self)
			{
				// Watermark feature is client-only; cancel the ipify request on servers.
				Scope.Cancel();
				UE_LOG(LogEOSSharedCompat, Log,
					TEXT("CSSCompatStubs: Suppressed RequestPublicPlayerAddress on "
					     "dedicated server (watermark is client-only)."));
			});
	}
}

void FEOSSharedCompatModule::ShutdownModule()
{
	if (PublicIpHookHandle.IsValid())
	{
		UNSUBSCRIBE_METHOD(UFGLocalPlayer::RequestPublicPlayerAddress, PublicIpHookHandle);
		PublicIpHookHandle.Reset();
	}
}

IMPLEMENT_MODULE(FEOSSharedCompatModule, EOSShared)
