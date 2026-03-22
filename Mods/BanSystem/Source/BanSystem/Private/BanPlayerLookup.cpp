// Copyright Yamahasxviper. All Rights Reserved.

#include "BanPlayerLookup.h"

#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "FGPlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogBanPlayerLookup, Log, All);

// ─────────────────────────────────────────────────────────────────────────────
//  FBanPlayerLookup::FindPlayerByName
// ─────────────────────────────────────────────────────────────────────────────
bool FBanPlayerLookup::FindPlayerByName(
    UWorld*           World,
    const FString&    NameQuery,
    FResolvedBanId&   OutIds,
    FString&          OutPlayerName,
    TArray<FString>&  OutAmbiguousNames,
    bool              bRequireExactMatch)
{
    OutAmbiguousNames.Reset();

    if (!World || NameQuery.IsEmpty())
    {
        UE_LOG(LogBanPlayerLookup, Warning,
            TEXT("FindPlayerByName: called with null World or empty NameQuery."));
        return false;
    }

    // Collect all matching (Name, UniqueNetId) pairs.
    // We defer resolution until we know we have a single unambiguous match
    // to avoid unnecessary work on large servers.
    struct FCandidate
    {
        FString           PlayerName;
        FUniqueNetIdRepl  UniqueId;
    };
    TArray<FCandidate> Candidates;

    // TPlayerControllerIterator visits every PlayerController on the server,
    // mirroring the SML /list command pattern.
    for (TPlayerControllerIterator<APlayerController>::ServerAll It(World); It; ++It)
    {
        APlayerController* Controller = *It;
        if (!Controller || !Controller->PlayerState) continue;

        const FString PlayerName = Controller->PlayerState->GetPlayerName();
        bool bMatches;
        if (bRequireExactMatch)
            bMatches = PlayerName.Equals(NameQuery, ESearchCase::IgnoreCase);
        else
            bMatches = PlayerName.Contains(NameQuery, ESearchCase::IgnoreCase);

        if (bMatches)
        {
            AFGPlayerState* FGState = Cast<AFGPlayerState>(Controller->PlayerState);
            if (!FGState) continue;

            FCandidate& C  = Candidates.AddDefaulted_GetRef();
            C.PlayerName   = PlayerName;
            C.UniqueId     = FGState->GetUniqueNetId();
        }
    }

    if (Candidates.IsEmpty())
    {
        UE_LOG(LogBanPlayerLookup, Verbose,
            TEXT("FindPlayerByName: no online player matches '%s'."), *NameQuery);
        return false;
    }

    if (Candidates.Num() > 1)
    {
        // Ambiguous partial match — return all names so the caller can display them
        for (const FCandidate& C : Candidates)
            OutAmbiguousNames.Add(C.PlayerName);

        UE_LOG(LogBanPlayerLookup, Verbose,
            TEXT("FindPlayerByName: '%s' is ambiguous (%d matches)."),
            *NameQuery, Candidates.Num());
        return false;
    }

    // Exactly one match — resolve their platform IDs
    const FCandidate& Match = Candidates[0];
    OutIds        = FBanIdResolver::Resolve(Match.UniqueId);
    OutPlayerName = Match.PlayerName;

    if (!OutIds.IsValid())
    {
        UE_LOG(LogBanPlayerLookup, Warning,
            TEXT("FindPlayerByName: found player '%s' but could not resolve any platform ID."),
            *OutPlayerName);
        return false;
    }

    UE_LOG(LogBanPlayerLookup, Verbose,
        TEXT("FindPlayerByName: resolved '%s' → Steam='%s' PUID='%s'"),
        *OutPlayerName, *OutIds.Steam64Id, *OutIds.EOSProductUserId);

    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
//  FBanPlayerLookup::GetAllConnectedPlayers
// ─────────────────────────────────────────────────────────────────────────────
TArray<TPair<FString, FResolvedBanId>> FBanPlayerLookup::GetAllConnectedPlayers(UWorld* World)
{
    TArray<TPair<FString, FResolvedBanId>> Result;
    if (!World) return Result;

    for (TPlayerControllerIterator<APlayerController>::ServerAll It(World); It; ++It)
    {
        APlayerController* Controller = *It;
        if (!Controller || !Controller->PlayerState) continue;

        FString        Name   = Controller->PlayerState->GetPlayerName();
        AFGPlayerState* FGState = Cast<AFGPlayerState>(Controller->PlayerState);
        if (!FGState) continue;
        FResolvedBanId Ids    = FBanIdResolver::Resolve(FGState->GetUniqueNetId());
        Result.Emplace(MoveTemp(Name), MoveTemp(Ids));
    }
    return Result;
}
