// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_auth.h — delegates to the real EOS SDK eos_auth.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C3431/C5257 conflicts when EOSShared.h (included
// via EOSDirectSDK) tries to redeclare EOS_EAuthTokenType / EOS_ELoginCredentialType
// as scoped enums after our unscoped definitions were processed first.

#pragma once

// Delegate to the real EOS SDK's eos_auth.h via the EOSSDK module include path.
#include <eos_auth.h>
