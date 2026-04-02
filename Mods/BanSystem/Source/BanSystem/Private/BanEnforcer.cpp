// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/enforcer.ts

#include "BanEnforcer.h"
#include "BanDatabase.h"

#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameSession.h"
#include "Engine/GameInstance.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY(LogBanEnforcer);

// ─────────────────────────────────────────────────────────────────────────────
//  USubsystem lifecycle
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::Initialize(FSubsystemCollectionBase& Collection)
{
    Collection.InitializeDependency<UBanDatabase>();
    Super::Initialize(Collection);

    // Hook PreLogin so banned players are rejected before they ever join.
    // This replaces the polling loop in Tools/BanSystem/src/enforcer.ts.
    PreLoginHandle = FGameModeEvents::GameModePreLoginEvent.AddUObject(
        this, &UBanEnforcer::OnPreLogin);

    UE_LOG(LogBanEnforcer, Log, TEXT("BanEnforcer: login enforcement active"));
}

void UBanEnforcer::Deinitialize()
{
    FGameModeEvents::GameModePreLoginEvent.Remove(PreLoginHandle);
    PreLoginHandle.Reset();
    Super::Deinitialize();
}

// ─────────────────────────────────────────────────────────────────────────────
//  Enforcement
// ─────────────────────────────────────────────────────────────────────────────

void UBanEnforcer::OnPreLogin(AGameModeBase* /*GameMode*/,
                               const FUniqueNetIdRepl& UniqueId,
                               FString& ErrorMessage)
{
    // If another system already rejected this login, don't overwrite.
    if (!ErrorMessage.IsEmpty()) return;

    if (!UniqueId.IsValid()) return;

    UBanDatabase* DB = GetGameInstance()->GetSubsystem<UBanDatabase>();
    if (!DB) return;

    // Build the compound UID.  The FUniqueNetId type name is "STEAM" or "EOS"
    // on the CSS dedicated server; we upper-case it to match the stored format.
    const FString Platform = UniqueId->GetType().ToString().ToUpper();
    const FString RawId    = UniqueId->ToString();
    const FString Uid      = UBanDatabase::MakeUid(Platform, RawId);

    FBanEntry Entry;
    if (DB->IsCurrentlyBanned(Uid, Entry))
    {
        ErrorMessage = Entry.GetKickMessage();
        UE_LOG(LogBanEnforcer, Log,
            TEXT("BanEnforcer: rejected %s (%s) — %s"),
            *RawId, *Platform, *ErrorMessage);
    }
}
