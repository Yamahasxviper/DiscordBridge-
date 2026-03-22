// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BanDiscordConfig.generated.h"

/**
 * UBanDiscordConfig
 *
 * Configuration for BanSystem → Discord notification messages.
 * Loaded from  Config/DefaultBanSystem.ini  under the section:
 *   [BanSystem.BanDiscordConfig]
 *
 * To enable notifications:
 *   1. Set BanNotificationChannelId to your Discord channel snowflake ID.
 *   2. Ensure a mod that implements IBanDiscordNotificationProvider
 *      (e.g. DiscordBridge) is installed and calls
 *      USteamBanSubsystem::SetNotificationProvider() /
 *      UEOSBanSubsystem::SetNotificationProvider().
 *
 * Supported placeholders in message templates:
 *   %PlayerId%  — Steam64 ID or EOS Product User ID of the affected player
 *   %Reason%    — Human-readable ban reason
 *   %BannedBy%  — Admin name / identifier who issued the ban
 */
UCLASS(Config = BanSystem, DefaultConfig)
class BANSYSTEM_API UBanDiscordConfig : public UObject
{
    GENERATED_BODY()

public:
    /**
     * Discord channel snowflake ID to post ban/unban notifications in.
     * Leave empty (default) to disable Discord notifications entirely.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Ban System|Discord")
    FString BanNotificationChannelId;

    /** Message sent when a Steam64 ID is banned.
     *  Placeholders: %PlayerId%, %Reason%, %BannedBy% */
    UPROPERTY(Config, EditAnywhere, Category = "Ban System|Discord")
    FString SteamBanMessage = TEXT(":hammer: **Steam Ban** | `%PlayerId%` banned by `%BannedBy%`. Reason: %Reason%");

    /** Message sent when a Steam64 ID ban is removed.
     *  Placeholders: %PlayerId% */
    UPROPERTY(Config, EditAnywhere, Category = "Ban System|Discord")
    FString SteamUnbanMessage = TEXT(":white_check_mark: **Steam Unban** | `%PlayerId%` has been unbanned.");

    /** Message sent when an EOS Product User ID is banned.
     *  Placeholders: %PlayerId%, %Reason%, %BannedBy% */
    UPROPERTY(Config, EditAnywhere, Category = "Ban System|Discord")
    FString EOSBanMessage = TEXT(":hammer: **EOS Ban** | `%PlayerId%` banned by `%BannedBy%`. Reason: %Reason%");

    /** Message sent when an EOS Product User ID ban is removed.
     *  Placeholders: %PlayerId% */
    UPROPERTY(Config, EditAnywhere, Category = "Ban System|Discord")
    FString EOSUnbanMessage = TEXT(":white_check_mark: **EOS Unban** | `%PlayerId%` has been unbanned.");
};
