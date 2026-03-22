// Copyright Coffee Stain Studios. All Rights Reserved.

#include "SMLWebSocket.h"
#include "Modules/ModuleManager.h"

// OpenSSL global init/cleanup — only on the two Satisfactory dedicated-server platforms.
// UE's Slate/InputCore declares `namespace UI {}` at global scope.  OpenSSL's
// ossl_typ.h (line 144) also declares `typedef struct ui_st UI` at global scope.
// On MSVC this produces error C2365 ("redefinition; previous definition was
// 'namespace'") because a C++ namespace and a typedef cannot share the same
// name in the same scope.  `#undef UI` only removes a preprocessor macro and
// has no effect on a C++ namespace declaration, so the conflict remains.
//
// The correct fix is to redirect the OpenSSL typedef to a different name while
// ossl_typ.h is being processed.  By defining UI as a macro that expands to
// UI_OSSLRenamed, the typedef becomes `typedef struct ui_st UI_OSSLRenamed`
// which does not conflict with `namespace UI {}`.  push/pop_macro ensures the
// macro state is properly saved and restored around the OpenSSL includes so
// that post-include code that refers to `namespace UI` continues to work.
//
// THIRD_PARTY_INCLUDES_START/END suppress MSVC warnings (e.g. C4191, C4996)
// that are emitted by OpenSSL's own headers and would otherwise be treated as
// errors under UBT's /WX flag.
#if PLATFORM_WINDOWS || PLATFORM_LINUX
THIRD_PARTY_INCLUDES_START
#pragma push_macro("UI")
#define UI UI_OSSLRenamed
#include "openssl/ssl.h"
#include "openssl/err.h"
#include "openssl/crypto.h"
#pragma pop_macro("UI")
THIRD_PARTY_INCLUDES_END
#endif

void FSMLWebSocketModule::StartupModule()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	// OpenSSL 1.1.0+ auto-initializes; for older versions we call the explicit init.
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	SSL_library_init();
	SSL_load_error_strings();
	OpenSSL_add_all_algorithms();
#else
	// Explicit opt-in init (safe to call multiple times; reference-counted internally).
	OPENSSL_init_ssl(OPENSSL_INIT_LOAD_SSL_STRINGS | OPENSSL_INIT_LOAD_CRYPTO_STRINGS, nullptr);
#endif
#endif // PLATFORM_WINDOWS || PLATFORM_LINUX
}

void FSMLWebSocketModule::ShutdownModule()
{
#if PLATFORM_WINDOWS || PLATFORM_LINUX
	// OpenSSL 1.1.0+ handles cleanup automatically; nothing to do here.
#if OPENSSL_VERSION_NUMBER < 0x10100000L
	ERR_free_strings();
	EVP_cleanup();
#endif
#endif // PLATFORM_WINDOWS || PLATFORM_LINUX
}

IMPLEMENT_MODULE(FSMLWebSocketModule, SMLWebSocket)
