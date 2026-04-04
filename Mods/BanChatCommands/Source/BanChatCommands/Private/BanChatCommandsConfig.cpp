// Copyright Yamahasxviper. All Rights Reserved.

#include "BanChatCommandsConfig.h"
#include "BanDatabase.h"

const UBanChatCommandsConfig* UBanChatCommandsConfig::Get()
{
    return GetDefault<UBanChatCommandsConfig>();
}

bool UBanChatCommandsConfig::IsAdminUid(const FString& Uid) const
{
    if (Uid.IsEmpty()) return false;

    FString Platform, RawId;
    UBanDatabase::ParseUid(Uid, Platform, RawId);

    if (Platform == TEXT("STEAM"))
    {
        for (const FString& Id : AdminSteam64Ids)
            if (Id.Equals(RawId, ESearchCase::CaseSensitive)) return true;
    }
    else if (Platform == TEXT("EOS"))
    {
        // EOS PUIDs are hex strings — compare case-insensitively.
        for (const FString& Id : AdminEosPUIDs)
            if (Id.Equals(RawId, ESearchCase::IgnoreCase)) return true;
    }
    return false;
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
