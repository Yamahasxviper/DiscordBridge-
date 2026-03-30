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

// ─── PUID wrapper ──────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSProductUserId
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS")
    FString Value;

    bool   IsValid()    const { return Value.Len() == 32; }
    FString ToString()  const { return Value; }
};

// ─── Session info ─────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSSessionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Sessions") FString SessionName;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sessions") FString SessionId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sessions") int32   MaxPlayers     = 0;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sessions") int32   CurrentPlayers = 0;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sessions") bool    bIsActive      = false;
};

// ─── Sanction info ────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSSanctionInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Sanctions") FString Action;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sanctions") FString TimePlaced;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Sanctions") FString ReferenceId;
};

// ─── Stat info ────────────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSStatInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Stats") FString Name;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Stats") int32   Value = 0;
};

// ─── Achievement info ─────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSAchievementInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Achievements") FString AchievementId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Achievements") float   Progress   = 0.f;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Achievements") int64   UnlockTime = 0;
};

// ─── Leaderboard record ───────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSLeaderboardRecord
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Leaderboards") FString UserId;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Leaderboards") int32   Rank        = 0;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Leaderboards") int32   Score       = 0;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Leaderboards") FString DisplayName;
};

// ─── Storage file info ────────────────────────────────────────────────────
USTRUCT(BlueprintType)
struct EOSSYSTEM_API FEOSFileInfo
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, Category="EOS|Storage") FString Filename;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Storage") int32   FileSizeBytes = 0;
    UPROPERTY(BlueprintReadWrite, Category="EOS|Storage") FString MD5Hash;
};

// ─── Delegates ────────────────────────────────────────────────────────────
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSInitialized,         bool,                            bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSanctionsQueried,    const FString&,                  PUID,       const TArray<FEOSSanctionInfo>&, Sanctions);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSSessionUpdated,      const FString&,                  SessionName,bool,                           bSuccess);
// FOnEOSPlayerRegistered carries the PlayerController so subscribers (e.g.
// BanEnforcementSubsystem) can act on the matching player directly without
// needing a world scan + FBanIdResolver re-resolve.  Controller may be nullptr
// when RegisterPlayerPUID() is called manually (non-PostLogin path).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPlayerRegistered,    const FString&,                  PUID,       APlayerController*,             Controller);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSPlayerUnregistered,  const FString&,                  PUID);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSStatIngested,        const FString&,                  StatName,   bool,                           bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSAchievementUnlocked, const FString&,                  Id,         bool,                           bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSReportSent,          const FString&,                  ReportedPUID,bool,                          bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSMetricsSession,      const FString&,                  PUID,       bool,                           bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSLeaderboardQueried,  const FString&,                  LeaderboardId, bool,                        bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSStorageQueried,      const FString&,                  Filename,   bool,                           bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSOwnershipQueried,    const FString&,                  EpicAccountId, bool,                        bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSFriendsQueried,      const FString&,                  EpicAccountId, bool,                        bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPresenceSet,         const FString&,                  EpicAccountId, bool,                        bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSUserInfoQueried,     const FString&,                  EpicAccountId, bool,                        bSuccess);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FOnEOSPUIDLookupComplete,  const FString&,                  ExternalId,  const FString&,                PUID);
// Fires when a reverse PUID lookup completes: PUID → its linked external accounts
// Each linked external account fires one broadcast: (PUID, ExternalAccountId, Type)
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
