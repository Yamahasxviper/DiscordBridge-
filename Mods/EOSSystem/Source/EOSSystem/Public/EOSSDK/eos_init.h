// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_init.h — EOS SDK initialization interface, written from scratch using
// only public EOS SDK documentation (https://dev.epicgames.com/docs).
// No UE EOSSDK module, no EOSShared, and no CSS FactoryGame headers are
// referenced anywhere in this file.
//
// INCLUDE GUARD NOTE
// ──────────────────
// EOS_INIT_H matches the named guard used by the CSS engine's EOSSDK
// eos_init.h.  Whichever version is processed first sets the guard and
// prevents the other from re-defining EOS_Initialize_ThreadAffinity and
// EOS_InitializeOptions — eliminating the C2371 redefinition errors that
// occur when BanSystem's translation units include both EOSSystem and
// EOSDirectSDK (engine EOSSDK) headers.

#pragma once

#ifndef EOS_INIT_H
#define EOS_INIT_H

#include "eos_common.h"  // provides EOS_EResult (moved from eos_base.h)
#include "eos_logging.h"
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ─────────────────────────────────────────────────────────────────────────────
//  Memory allocator function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────
typedef void* (EOS_CALL *EOS_AllocateMemoryFunc)(size_t SizeInBytes, size_t Alignment);
typedef void* (EOS_CALL *EOS_ReallocateMemoryFunc)(void* Pointer, size_t SizeInBytes, size_t Alignment);
typedef void  (EOS_CALL *EOS_ReleaseMemoryFunc)(void* Pointer);

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Initialize_ThreadAffinity
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_INITIALIZE_THREADAFFINITY_API_LATEST 1

typedef struct EOS_Initialize_ThreadAffinity
{
    /** API version: must be EOS_INITIALIZE_THREADAFFINITY_API_LATEST */
    int32_t   ApiVersion;
    /** Affinity mask for the network work thread */
    uint64_t  NetworkWork;
    /** Affinity mask for storage I/O threads */
    uint64_t  StorageIo;
    /** Affinity mask for WebSocket I/O threads */
    uint64_t  WebSocketIo;
    /** Affinity mask for P2P I/O threads */
    uint64_t  P2PIo;
    /** Affinity mask for HTTP request threads */
    uint64_t  HttpRequestIo;
    /** Affinity mask for RTC I/O threads */
    uint64_t  RTCIo;
} EOS_Initialize_ThreadAffinity;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_InitializeOptions
// ─────────────────────────────────────────────────────────────────────────────
#define EOS_INITIALIZE_API_LATEST 4

typedef struct EOS_InitializeOptions
{
    /** API version: must be EOS_INITIALIZE_API_LATEST */
    int32_t                              ApiVersion;
    /** Optional custom allocator; NULL = use CRT malloc */
    EOS_AllocateMemoryFunc               AllocateMemoryFunction;
    /** Optional custom reallocator; NULL = use CRT realloc */
    EOS_ReallocateMemoryFunc             ReallocateMemoryFunction;
    /** Optional custom deallocator; NULL = use CRT free */
    EOS_ReleaseMemoryFunc                ReleaseMemoryFunction;
    /** Product name string (required) */
    const char*                          ProductName;
    /** Product version string (required) */
    const char*                          ProductVersion;
    /** Reserved — must be NULL */
    void*                                Reserved;
    /** Optional thread affinity override; NULL = OS default */
    const EOS_Initialize_ThreadAffinity* OverrideThreadAffinity;
} EOS_InitializeOptions;

// ─────────────────────────────────────────────────────────────────────────────
//  EOS_Initialize / EOS_Shutdown function pointer typedefs
// ─────────────────────────────────────────────────────────────────────────────
typedef EOS_EResult (EOS_CALL *EOS_Initialize_t)(const EOS_InitializeOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_Shutdown_t)(void);

#ifdef __cplusplus
}
#endif

#endif // EOS_INIT_H
