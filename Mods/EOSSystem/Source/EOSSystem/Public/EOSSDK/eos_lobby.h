// Copyright Yamahasxviper. All Rights Reserved.
//
// eos_lobby.h — delegates to the real EOS SDK eos_lobby.h.
//
// EOSSystem lists EOSSDK as a public dependency, making the engine's
// ThirdParty EOS C SDK headers available via angle-bracket includes.
// Delegating here prevents C2371/C2011 conflicts when BanSystem includes
// both EOSSystem and EOSDirectSDK headers in the same translation unit.

#pragma once

// Delegate to the real EOS SDK's eos_lobby.h via the EOSSDK module include path.
#include <eos_lobby.h>

// ─────────────────────────────────────────────────────────────────────────────
//  Lobby interface function pointer typedefs
//  These _t aliases are defined for completeness; the core DLL loader
//  (FEOSSDKLoader) currently only loads EOS_Platform_GetLobbyInterface.
//  The real EOS SDK eos_lobby.h declares the functions but does not provide
//  these typedefs, so we add them here after the real header is included.
// ─────────────────────────────────────────────────────────────────────────────

typedef void        (EOS_CALL *EOS_Lobby_CreateLobby_t)(EOS_HLobby Handle, const EOS_Lobby_CreateLobbyOptions* Options, void* ClientData, EOS_Lobby_OnCreateLobbyCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Lobby_DestroyLobby_t)(EOS_HLobby Handle, const EOS_Lobby_DestroyLobbyOptions* Options, void* ClientData, EOS_Lobby_OnDestroyLobbyCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Lobby_JoinLobby_t)(EOS_HLobby Handle, const EOS_Lobby_JoinLobbyOptions* Options, void* ClientData, EOS_Lobby_OnJoinLobbyCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Lobby_LeaveLobby_t)(EOS_HLobby Handle, const EOS_Lobby_LeaveLobbyOptions* Options, void* ClientData, EOS_Lobby_OnLeaveLobbyCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_Lobby_UpdateLobbyModification_t)(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyModificationOptions* Options, EOS_HLobbyModification* OutLobbyModificationHandle);
typedef void        (EOS_CALL *EOS_Lobby_UpdateLobby_t)(EOS_HLobby Handle, const EOS_Lobby_UpdateLobbyOptions* Options, void* ClientData, EOS_Lobby_OnUpdateLobbyCallback CompletionDelegate);
typedef void        (EOS_CALL *EOS_Lobby_KickMember_t)(EOS_HLobby Handle, const EOS_Lobby_KickMemberOptions* Options, void* ClientData, EOS_Lobby_OnKickMemberCallback CompletionDelegate);
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetBucketId_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetBucketIdOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetMaxMembers_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetMaxMembersOptions* Options);
typedef EOS_EResult (EOS_CALL *EOS_LobbyModification_SetPermissionLevel_t)(EOS_HLobbyModification Handle, const EOS_LobbyModification_SetPermissionLevelOptions* Options);
typedef void        (EOS_CALL *EOS_LobbyModification_Release_t)(EOS_HLobbyModification LobbyModificationHandle);
