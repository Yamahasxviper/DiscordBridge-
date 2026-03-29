// Copyright Yamahasxviper. All Rights Reserved.
//
// Minimal module implementation for EOSIdHelper.
// All functional code lives in the public header (EOSIdHelper.h) as an inline
// function.  This file only satisfies UBT's requirement that every Runtime
// module produce a loadable DLL.

#include "EOSIdHelper.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, EOSIdHelper)
