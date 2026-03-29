// Copyright Yamahasxviper. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "OnlineSubsystemTypes.h"  // FUniqueNetId

#if WITH_EOS_SDK

// EOS SDK core types: EOS_ProductUserId, EOS_EpicAccountId,
// EOS_ProductUserId_IsValid, EOS_EpicAccountId_IsValid
#if defined(EOS_PLATFORM_BASE_FILE_NAME)
#include EOS_PLATFORM_BASE_FILE_NAME
#endif
#include "eos_common.h"

// EOSShared — LexToString(EOS_ProductUserId) → FString
#include "EOSShared.h"

// ─────────────────────────────────────────────────────────────────────────────
//  IUniqueNetIdEOS
// ─────────────────────────────────────────────────────────────────────────────

/**
 * Stub interface for OnlineSubsystemEOS V1 identity types.
 *
 * Mirrors the API surface of Unreal Engine's IUniqueNetIdEOS so that mods
 * can include this header and call GetProductUserId() / GetEpicAccountId()
 * on a V1 FUniqueNetId without requiring the full OnlineSubsystemEOS client
 * plugin to be present.  On Satisfactory dedicated servers the real EOS
 * client plugin is never loaded; this stub keeps dependent code compilable.
 *
 * Usage:
 *   #include "OnlineSubsystemEOSTypesPublic.h"
 *
 *   if (UniqueId.IsV1())
 *   {
 *       const TSharedPtr<const FUniqueNetId> Ptr = UniqueId.GetUniqueNetId();
 *       if (Ptr.IsValid() && Ptr->GetType() == TEXT("EOS"))
 *       {
 *           const IUniqueNetIdEOS* EosId =
 *               static_cast<const IUniqueNetIdEOS*>(Ptr.Get());
 *           EOS_ProductUserId Puid = EosId->GetProductUserId();
 *           if (EOS_ProductUserId_IsValid(Puid))
 *               FString PuidStr = LexToString(Puid);
 *       }
 *   }
 */
class ONLINESUBSYSTEMEOS_API IUniqueNetIdEOS : public FUniqueNetId
{
public:
	virtual ~IUniqueNetIdEOS() = default;

	/**
	 * Returns the EOS Product User ID (PUID) embedded in this identity.
	 * May return an invalid handle if the player has no PUID.
	 */
	virtual EOS_ProductUserId GetProductUserId() const = 0;

	/**
	 * Returns the EOS Epic Account ID embedded in this identity.
	 * May return an invalid handle for accounts without an Epic login.
	 */
	virtual EOS_EpicAccountId GetEpicAccountId() const = 0;
};

#endif  // WITH_EOS_SDK
