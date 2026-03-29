// Copyright Yamahasxviper. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Engine/EngineTypes.h"
#include "Tickable.h"
#include "EOSTypes.h"
#include "EOSSDK/eos_sdk.h"
#include "EOSSystemSubsystem.generated.h"

UCLASS()
class EOSSYSTEM_API UEOSSystemSubsystem : public UGameInstanceSubsystem, public FTickableGameObject
{
    GENERATED_BODY()
public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    // FTickableGameObject
    virtual void    Tick(float DeltaTime)     override;
    virtual TStatId GetStatId()         const override;
    virtual bool    IsTickable()        const override { return bPlatformReady; }
    virtual bool    IsTickableInEditor()const override { return false; }

    UFUNCTION(BlueprintPure, Category="EOS") bool    IsEOSReady()      const { return bPlatformReady; }
    UFUNCTION(BlueprintPure, Category="EOS") FString GetProductId()    const;
    UFUNCTION(BlueprintPure, Category="EOS") FString GetDeploymentId() const;
    UFUNCTION(BlueprintCallable, Category="EOS") void SetLogLevel(const FString& Level);

    EOS_HPlatform GetPlatformHandle() const { return PlatformHandle; }

    UPROPERTY(BlueprintAssignable, Category="EOS")
    FOnEOSInitialized OnEOSInitialized;

private:
    EOS_HPlatform PlatformHandle     = nullptr;
    bool          bInitialisedByUs   = false;
    bool          bPlatformReady     = false;
    FString       CacheDirStorage;
    FString       EncKeyStorage;

    static void EOS_CALL OnEOSLog(const EOS_LogMessage* Msg);
    static EOS_ELogLevel MapLogLevel(const FString& Level);
};
