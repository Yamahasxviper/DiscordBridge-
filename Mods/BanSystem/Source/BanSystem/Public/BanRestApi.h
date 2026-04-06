// Copyright Yamahasxviper. All Rights Reserved.
// Direct port of Tools/BanSystem/src/apiServer.ts

#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "BanRestApi.generated.h"

class IHttpRouter;

// Opaque implementation struct — defined in BanRestApi.cpp.
// Keeps TArray<FHttpRouteHandle> (an HTTPServer-module type) out of this
// public UHT-processed header so that MSVC / UHT never needs to instantiate
// TArray<FHttpRouteHandle> from the Public include path where the HTTPServer
// module headers are not available (PrivateDependencyModuleNames only).
struct FBanRestApiRoutes;

DECLARE_LOG_CATEGORY_EXTERN(LogBanRestApi, Log, All);

/**
 * UBanRestApi
 *
 * Local HTTP REST management API for the ban system.
 * Direct port of the Express server in Tools/BanSystem/src/apiServer.ts.
 *
 * Routes (all return JSON):
 *   GET    /health             – liveness probe
 *   GET    /bans               – list active bans
 *   GET    /bans/all           – list all bans (including expired)
 *   GET    /bans/check/:uid    – check if PLATFORM:rawId is currently banned
 *   POST   /bans               – create a ban
 *   DELETE /bans/:uid          – remove ban by compound UID
 *   DELETE /bans/id/:id        – remove ban by integer row ID
 *   POST   /bans/prune         – delete expired temporary bans
 *   POST   /bans/backup        – create a database backup
 *
 * POST /bans body:
 *   {
 *     "playerUID":       "00020aed06f0a6958c3c067fb4b73d51",  // required
 *     "platform":        "EOS",                               // required: EOS|UNKNOWN
 *     "playerName":      "SomePlayer",          // optional
 *     "reason":          "Griefing",            // optional
 *     "bannedBy":        "admin",               // optional
 *     "durationMinutes": 0                      // optional; 0 or omit = permanent
 *   }
 *
 * Configure the port in DefaultBanSystem.ini:
 *   [BanSystem]
 *   RestApiPort=3000
 *
 * Set RestApiPort=0 to disable the REST API.
 */
UCLASS()
class BANSYSTEM_API UBanRestApi : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

private:
    void StartServer();
    void StopServer();
    void RegisterRoutes();

    TSharedPtr<IHttpRouter> Router;
    TSharedPtr<FBanRestApiRoutes> Routes;   // holds TArray<FHttpRouteHandle>
    int32 ApiPort = 3000;
};
