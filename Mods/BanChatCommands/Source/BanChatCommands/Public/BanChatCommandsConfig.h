// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "BanChatCommandsConfig.generated.h"

/**
 * UBanChatCommandsConfig
 *
 * Per-server configuration for BanChatCommands.
 *
 * RECOMMENDED: manage the admin list in the persistent override file:
 *   Saved/BanChatCommands/BanChatCommands.ini
 * That file is never touched by mod updates or Alpakit dev deploys.
 * BanChatCommands writes the current admin list there on every server start
 * so your configuration survives any wipe of the mod directory.
 *
 * Example Saved/BanChatCommands/BanChatCommands.ini:
 *
 *   [/Script/BanChatCommands.BanChatCommandsConfig]
 *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
 *
 * When the list is empty, admin commands (/ban, /tempban, /unban,
 * /bancheck, /banlist) can only be run from the server console.
 * /whoami is always available to all players regardless of this setting.
 *
 * Note: On CSS Dedicated Server, all players are identified by their EOS
 * Product User ID regardless of their launch platform (Steam, Epic, etc.).
 * Use /whoami in-game to find the 32-character hex EOS PUID for any player.
 */
UCLASS(Config = BanChatCommands, meta = (DisplayName = "Ban Chat Commands"))
class BANCHATCOMMANDS_API UBanChatCommandsConfig : public UObject
{
    GENERATED_BODY()

public:
    /**
     * EOS Product User IDs (32-character hex strings) of server administrators
     * who may run ban commands in chat.  Use /whoami in-game to find your EOS PUID.
     * Add one entry per line in BanChatCommands.ini using the +AdminEosPUIDs= syntax.
     * If this list is empty, only the server console can run admin commands.
     *
     * Example:
     *   +AdminEosPUIDs=00020aed06f0a6958c3c067fb4b73d51
     */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands")
    TArray<FString> AdminEosPUIDs;

    // ─────────────────────────────────────────────────────────────────────
    //  Chat message colours (hex colour code, e.g. #FF0000 for red)
    //
    //  Used for every response message sent by BanChatCommands.
    //  Set values as a 6-digit hex colour code in the ini file.
    //  The leading # is optional but recommended for readability.
    //
    //  Quick colour reference:
    //    Red       #FF0000
    //    Green     #00FF00
    //    Blue      #0000FF
    //    Yellow    #FFFF00
    //    Cyan      #00FFFF
    //    Magenta   #FF00FF
    //    White     #FFFFFF
    //    Orange    #FF8000
    //    Purple    #8000FF
    // ─────────────────────────────────────────────────────────────────────

    /** Colour for error / permission-denied messages. Default: red. */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands|Colors")
    FString ChatColorError = TEXT("#FF0000");

    /** Colour for success / action-completed messages. Default: green. */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands|Colors")
    FString ChatColorSuccess = TEXT("#00FF00");

    /** Colour for warning / ambiguous-result messages. Default: yellow. */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands|Colors")
    FString ChatColorWarning = TEXT("#FFFF00");

    /** Colour for informational / neutral messages. Default: white. */
    UPROPERTY(Config, BlueprintReadOnly, Category = "BanChatCommands|Colors")
    FString ChatColorInfo = TEXT("#FFFFFF");

    /** Returns the singleton config instance. */
    static const UBanChatCommandsConfig* Get();

    /**
     * Returns true when the compound UID ("EOS:xxx") belongs to a configured
     * server administrator.  Comparison is case-insensitive for EOS PUIDs.
     */
    bool IsAdminUid(const FString& Uid) const;
};
