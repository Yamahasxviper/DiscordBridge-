// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/enforcer.ts
// (PreLogin/PostLogin hooks replace the external DS-API polling loop)

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanEnforcer.generated.h"

DECLARE_LOG_CATEGORY_EXTERN(LogBanEnforcer, Log, All);

/**
 * UBanEnforcer
 *
 * Enforces bans at player login.  Direct port of the Enforcer class in
 * Tools/BanSystem/src/enforcer.ts, adapted for a UE server-side mod:
 *
 *   Tools/BanSystem approach: external service polls the DS HTTPS API
 *                              every N seconds and kicks banned players.
 *
 *   UE mod approach:          hook PreLogin (fires before the player
 *                              joins) to reject the connection outright
 *                              with the ban reason.  No polling needed.
 *
 * Supports both Steam and EOS player IDs.  The compound UID format
 * ("STEAM:xxx" / "EOS:xxx") matches the database schema exactly.
 */
UCLASS()
class BANSYSTEM_API UBanEnforcer : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    void OnPreLogin(AGameModeBase* GameMode,
                    const FUniqueNetIdRepl& UniqueId,
                    FString& ErrorMessage);

    FDelegateHandle PreLoginHandle;
};
