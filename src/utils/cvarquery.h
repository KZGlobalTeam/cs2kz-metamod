#pragma once
#include "common.h"
#include <playerslot.h>
#include <functional>

// Queries the value of a convar on a client: CSVCMsg_GetCvarValue out, CCLCMsg_RespondCvarValue back.
namespace cvarquery
{
	enum class Status
	{
		ValueIntact = 0, // It got the value fine.
		CvarNotFound = 1,
		NotACvar = 2,     // There's a ConCommand, but it's not a ConVar.
		CvarProtected = 3 // The cvar was marked with FCVAR_SERVER_CAN_NOT_QUERY.
	};

	using Callback = std::function<void(CPlayerSlot slot, Status status, const char *name, const char *value)>;

	// The hooks that feed this live in hooks.cpp.
	void Shutdown();

	// The callback runs once, when the client answers; a client that never answers never calls back.
	bool Query(CPlayerSlot slot, const char *cvarName, Callback callback);

	void OnClientDisconnect(CPlayerSlot slot);

	// Every convar a client reports: the full userinfo set on connect, and each setinfo afterwards.
	using ChangeCallback = void (*)(CPlayerSlot slot, const char *name, const char *value);
	void SetChangeCallback(ChangeCallback callback);

	void OnCvarValueResponse(CPlayerSlot slot, i32 cookie, Status status, const char *name, const char *value);
	void OnClientConVar(CPlayerSlot slot, const char *name, const char *value);
} // namespace cvarquery
