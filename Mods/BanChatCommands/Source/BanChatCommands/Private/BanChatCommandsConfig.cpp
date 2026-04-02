// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsConfig.h"

const UBanChatCommandsConfig* UBanChatCommandsConfig::Get()
{
    return GetDefault<UBanChatCommandsConfig>();
}

bool UBanChatCommandsConfig::IsAdmin(const FString& Steam64Id) const
{
    if (Steam64Id.IsEmpty()) return false;
    for (const FString& AdminId : AdminSteam64Ids)
    {
        if (AdminId.Equals(Steam64Id, ESearchCase::CaseSensitive))
            return true;
    }
    return false;
}
