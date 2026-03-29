// Copyright Yamahasxviper. All Rights Reserved.

// Minimal module implementation.
// FGOnlineHelpers is a header-only compile-time dependency that provides inline
// helper stubs for CSS FactoryGame/EOS types not exposed through the standard
// Satisfactory modding headers.  All functionality lives in Public headers;
// this file just satisfies UBT's requirement that every Runtime module produce
// a loadable DLL via IMPLEMENT_MODULE.

#include "Online/FGOnlineHelpers.h"
#include "Modules/ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, FGOnlineHelpers)
