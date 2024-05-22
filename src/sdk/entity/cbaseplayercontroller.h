#pragma once

#include "ehandle.h"
#include "cbaseentity.h"
#include "ccsplayerpawn.h"
#include "utils/interfaces.h"

class CCSPlayerPawn;

enum class PlayerConnectedState : uint32_t
{
	PlayerNeverConnected = 0xFFFFFFFF,
	PlayerConnected = 0x0,
	PlayerConnecting = 0x1,
	PlayerReconnecting = 0x2,
	PlayerDisconnecting = 0x3,
	PlayerDisconnected = 0x4,
	PlayerReserved = 0x5,
};

class CBasePlayerController : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerController);

	SCHEMA_FIELD(uint64, m_steamID)
	SCHEMA_FIELD(bool, m_bIsHLTV)
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPawn)
	SCHEMA_FIELD_POINTER(char, m_iszPlayerName)

	CBasePlayerPawn *GetCurrentPawn()
	{
		return m_hPawn.Get();
	}

	const char *GetPlayerName()
	{
		return m_iszPlayerName();
	}

	int GetPlayerSlot()
	{
		return entindex() - 1;
	}

	void SetPawn(CCSPlayerPawn *pawn)
	{
		g_pKZUtils->SetPawn(this, pawn, true, false);
	}
};
