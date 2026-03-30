// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_logging.h — delegates to the real EOS SDK eos_logging.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.

#pragma once

#ifndef EOS_LOGGING_H
#define EOS_LOGGING_H

// Delegate to the real EOS SDK's eos_logging.h.
#include <eos_logging.h>

// Convenience alias: the real EOS SDK names the all-categories constant
// EOS_LC_AllCategories; EOSSystemSubsystem.cpp references EOS_LC_ALL_CATEGORIES.
#ifndef EOS_LC_ALL_CATEGORIES
#  define EOS_LC_ALL_CATEGORIES EOS_LC_AllCategories
#endif

#endif // EOS_LOGGING_H
