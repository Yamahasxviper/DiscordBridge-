// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "BanIdResolver.h"  // FResolvedBanId

/**
 * FBanPlayerLookup
 *
 * Stateless utility that resolves a currently-connected player's platform IDs
 * from their in-game display name.
 *
 * This is the bridge between "an admin types a player's name" and the
 * underlying Steam64 / EOS PUID ban system.
 *
 * Iteration uses the standard SML pattern:
 *   TPlayerControllerIterator<APlayerController>::ServerAll(World)
 * which visits every PlayerController on the listen/dedicated server, not
 * including local spectators or offline entries.
 *
 * Name matching is case-insensitive and supports both:
 *   - Exact match  (bRequireExactMatch = true)
 *   - Prefix/contains match  (bRequireExactMatch = false, default)
 *
 * Multiple results are handled as follows:
 *   - If exactly one match is found, it is returned.
 *   - If more than one match is found for a partial query, the function
 *     returns false and populates OutAmbiguousMatches so the caller can
 *     display a disambiguation list to the admin.
 *
 * Usage — inside a ChatCommandInstance:
 * @code
 *   FResolvedBanId   Ids;
 *   FString          ExactName;
 *   TArray<FString>  Ambiguous;
 *
 *   if (!FBanPlayerLookup::FindPlayerByName(GetWorld(), Args[0], Ids, ExactName, Ambiguous))
 *   {
 *       if (Ambiguous.Num() > 1)
 *           Sender->SendChatMessage(TEXT("Multiple matches: ") + FString::Join(Ambiguous, TEXT(", ")));
 *       else
 *           Sender->SendChatMessage(TEXT("Player not found: ") + Args[0]);
 *       return EExecutionStatus::BAD_ARGUMENTS;
 *   }
 *   // Use Ids.Steam64Id / Ids.EOSProductUserId
 * @endcode
 */
class BANSYSTEM_API FBanPlayerLookup
{
public:
    /**
     * Find a connected player by display name.
     *
     * @param World              The game world to search.  Must be non-null.
     * @param NameQuery          Player name to search for (case-insensitive).
     *                           Matched as a substring unless bRequireExactMatch is true.
     * @param OutIds             Populated with the player's resolved platform IDs on success.
     * @param OutPlayerName      Populated with the player's exact display name on success.
     * @param OutAmbiguousNames  Populated with all matching names when more than one player
     *                           matches the query and bRequireExactMatch is false.
     * @param bRequireExactMatch When true, only exact (case-insensitive) name matches succeed.
     *                           When false (default), a substring match is used and multiple
     *                           results trigger an ambiguity error.
     *
     * @return true when exactly one player was found and their IDs were successfully resolved.
     *         false when no player was found, multiple players matched (ambiguous), or the
     *         found player has no resolvable platform identity.
     */
    static bool FindPlayerByName(
        UWorld*           World,
        const FString&    NameQuery,
        FResolvedBanId&   OutIds,
        FString&          OutPlayerName,
        TArray<FString>&  OutAmbiguousNames,
        bool              bRequireExactMatch = false);

    /**
     * Enumerate all currently-connected players and their resolved IDs.
     *
     * Useful for /banlist lookups or admins who want to see all active names.
     *
     * @param World     The game world to search.
     * @return Array of (PlayerName, ResolvedBanId) pairs, one per connected player.
     */
    static TArray<TPair<FString, FResolvedBanId>> GetAllConnectedPlayers(UWorld* World);
};
