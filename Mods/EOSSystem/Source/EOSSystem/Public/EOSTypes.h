// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EOSSDK/eos_sdk.h"
#include "GameFramework/PlayerController.h"
#include "EOSTypes.generated.h"

// ─── Result wrapper ────────────────────────────────────────────────────────
UENUM(BlueprintType)
enum class EEOSResult : uint8
{
    Success         UMETA(DisplayName="Success"),
    Failure         UMETA(DisplayName="Failure"),
    NotFound        UMETA(DisplayName="Not Found"),
    AlreadyExists   UMETA(DisplayName="Already Exists"),
    InvalidParams   UMETA(DisplayName="Invalid Parameters"),
    NotReady        UMETA(DisplayName="Not Ready"),
    Unexpected      UMETA(DisplayName="Unexpected"),
};

// ─── External account type ─────────────────────────────────────────────────
// Blueprint-friendly mirror of EOS_EExternalAccountType.
// Values match the EOS C SDK enum exactly so they can be cast directly:
//   static_cast<EOS_EExternalAccountType>(EEOSExternalAccountType::Steam)
// Full mapping (EOS SDK 1.15.x):
//   Epic=0, Steam=1, PSN=2, XBL=3, Discord=4, GOG=5, Nintendo=6,
//   Uplay=7, OpenID=8, Apple=9, Google=10, Oculus=11, Itchio=12, Amazon=13
UENUM(BlueprintType)
enum class EEOSExternalAccountType : uint8
{
    Epic        = 0   UMETA(DisplayName="Epic Games"),
    Steam       = 1   UMETA(DisplayName="Steam"),
    PSN         = 2   UMETA(DisplayName="PlayStation Network"),
    XBL         = 3   UMETA(DisplayName="Xbox Live"),
    Discord     = 4   UMETA(DisplayName="Discord"),
    GOG         = 5   UMETA(DisplayName="GOG"),
    Nintendo    = 6   UMETA(DisplayName="Nintendo"),
    Uplay       = 7   UMETA(DisplayName="Ubisoft Connect"),
    OpenID      = 8   UMETA(DisplayName="OpenID"),
    Apple       = 9   UMETA(DisplayName="Apple"),
    Google      = 10  UMETA(DisplayName="Google"),
    Oculus      = 11  UMETA(DisplayName="Oculus/Meta"),
    Itchio      = 12  UMETA(DisplayName="itch.io"),
    Amazon      = 13  UMETA(DisplayName="Amazon"),
};

// ─── Delegates ────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSInitialized,           bool,           bSuccess);
// FOnEOSPlayerRegistered carries the PlayerController so subscribers (e.g.
// BanEnforcementSubsystem) can act on the matching player directly without
// needing a world scan + FBanIdResolver re-resolve.  Controller may be nullptr
// when RegisterPlayerPUID() is called manually (non-PostLogin path).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerRegistered,      const FString&, PUID,            APlayerController*, Controller);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSPlayerUnregistered,    const FString&, PUID);
// Forward lookup: fires with (ExternalAccountId, PUID) when LookupPUIDByExternalAccount* completes.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPUIDLookupComplete,    const FString&, ExternalId,      const FString&,     PUID);
// Reverse lookup: fires with (PUID, ExternalAccountId, Type) once per linked account.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_ThreeParams(FOnEOSReverseLookupComplete, const FString&, PUID, const FString&, ExternalAccountId, EEOSExternalAccountType, AccountType);

// ─── External account info (returned from reverse lookup cache) ───────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSExternalAccountInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Connect") FString                  AccountId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Connect") EEOSExternalAccountType  AccountType    = EEOSExternalAccountType::Epic;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Connect") FString                  DisplayName;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Connect") int64                    LastLoginTime  = -1;
};
