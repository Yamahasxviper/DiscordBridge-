// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BanTypes.h"

/**
 * IBanNotificationProvider
 *
 * Abstract interface that receives ban and unban events from USteamBanSubsystem
 * and UEOSBanSubsystem.  Implement this interface in any mod or project that
 * needs to react to BanSystem events (e.g. post a Discord notification).
 *
 * BanSystem has zero compile-time knowledge of any provider — the coupling is
 * one-directional: the implementing mod depends on BanSystem; BanSystem never
 * depends on the implementing mod.
 *
 * Usage:
 *   1. Inherit IBanNotificationProvider in your subsystem or module class.
 *   2. Implement the four pure-virtual methods below.
 *   3. In your Initialize(), call:
 *        SteamBans->SetNotificationProvider(this);
 *        EOSBans->SetNotificationProvider(this);
 *   4. In your Deinitialize(), call:
 *        SteamBans->SetNotificationProvider(nullptr);
 *        EOSBans->SetNotificationProvider(nullptr);
 */
class IBanNotificationProvider
{
public:
	virtual ~IBanNotificationProvider() = default;

	/**
	 * Called by USteamBanSubsystem immediately after a Steam64 ID ban is
	 * written to the ban list and persisted to disk.
	 *
	 * @param Steam64Id  17-digit Steam64 decimal ID of the banned player.
	 * @param Entry      Full ban record (reason, expiry, banned-by, etc.).
	 */
	virtual void OnSteamPlayerBanned(const FString& Steam64Id, const FBanEntry& Entry) = 0;

	/**
	 * Called by USteamBanSubsystem immediately after a Steam64 ID ban is
	 * removed from the ban list and the change is persisted to disk.
	 *
	 * @param Steam64Id  17-digit Steam64 decimal ID of the unbanned player.
	 */
	virtual void OnSteamPlayerUnbanned(const FString& Steam64Id) = 0;

	/**
	 * Called by UEOSBanSubsystem immediately after an EOS Product User ID ban
	 * is written to the ban list and persisted to disk.
	 *
	 * @param EOSProductUserId  32-character lowercase hex EOS PUID of the banned player.
	 * @param Entry             Full ban record (reason, expiry, banned-by, etc.).
	 */
	virtual void OnEOSPlayerBanned(const FString& EOSProductUserId, const FBanEntry& Entry) = 0;

	/**
	 * Called by UEOSBanSubsystem immediately after an EOS Product User ID ban
	 * is removed from the ban list and the change is persisted to disk.
	 *
	 * @param EOSProductUserId  32-character lowercase hex EOS PUID of the unbanned player.
	 */
	virtual void OnEOSPlayerUnbanned(const FString& EOSProductUserId) = 0;
};
