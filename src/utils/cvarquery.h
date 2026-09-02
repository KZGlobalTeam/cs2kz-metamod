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

	bool Init();
	void Shutdown();

	// Returns true if the query was successfully sent. The callback runs once, when the client
	// answers; a client that never answers never calls back.
	bool Query(CPlayerSlot slot, const char *cvarName, Callback callback);

	void OnClientDisconnect(CPlayerSlot slot);
} // namespace cvarquery
