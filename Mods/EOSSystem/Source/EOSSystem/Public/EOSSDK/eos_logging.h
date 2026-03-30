// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_logging.h — delegates to the real EOS SDK eos_logging.h.
//
// EOSSystem now lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOSSDK headers available via angle-bracket includes.
//
// NOTE: Do NOT add a named include guard matching the real SDK's guard here.
// Defining EOS_LOGGING_H before #include <eos_logging.h> causes the real
// header's own guard check to see it already set and skip all content —
// leaving EOS_LOG_Fatal, EOS_ELogLevel, EOS_LC_AllCategories, etc. undefined.
// #pragma once is sufficient to prevent this wrapper from being processed twice.

#pragma once

// Delegate to the real EOS SDK's eos_logging.h.
#include <eos_logging.h>

// Convenience alias: the real EOS SDK names the all-categories constant
// EOS_LC_AllCategories; EOSSystemSubsystem.cpp references EOS_LC_ALL_CATEGORIES.
#ifndef EOS_LC_ALL_CATEGORIES
#  define EOS_LC_ALL_CATEGORIES EOS_LC_AllCategories
#endif
