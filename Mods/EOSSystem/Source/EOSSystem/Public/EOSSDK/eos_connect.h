// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_connect.h — delegates to the real EOS SDK eos_connect.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
// Delegating here prevents C2011 conflicts for EOS_EExternalCredentialType
// when BanSystem includes both EOSSystem and EOSDirectSDK headers.

#pragma once

// Delegate to the real EOS SDK's eos_connect.h via the EOSSDK module include path.
#include <eos_connect.h>
