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

    if (Platform == TEXT("EOS"))
    {
        // EOS PUIDs are hex strings — compare case-insensitively.
        for (const FString& Id : AdminEosPUIDs)
            if (Id.Equals(RawId, ESearchCase::IgnoreCase)) return true;
    }
    return false;
}
