// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "EOSSDK/eos_sdk.h"
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
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam (FOnEOSPlayerRegistered,    const FString&,                  PUID);
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
