// Copyright Yamahasxviper. All Rights Reserved.

// Minimal module implementation.
// OnlineSubsystemEOS is a compile-time stub for Satisfactory server builds
// that provides the IUniqueNetIdEOS interface (and related EOS identity types)
// normally found in the engine's OnlineSubsystemEOS client plugin.
// All public API lives in Public/OnlineSubsystemEOSTypesPublic.h; this file
// only satisfies UBT's requirement that every Runtime module produce a DLL.

#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, OnlineSubsystemEOS)
