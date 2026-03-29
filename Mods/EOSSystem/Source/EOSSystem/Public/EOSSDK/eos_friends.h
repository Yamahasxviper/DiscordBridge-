// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_friends.h — delegates to the real EOS SDK eos_friends.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C3431/C5257 conflicts when EOSShared.h (included
// via EOSDirectSDK) tries to redeclare EOS_EFriendsStatus as a scoped enum
// after our unscoped definition was processed first.

#pragma once

// Delegate to the real EOS SDK's eos_friends.h via the EOSSDK module include path.
#include <eos_friends.h>
