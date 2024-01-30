#pragma once

#include "ehandle.h"
#include "cbaseentity.h"
#include "ccsplayerpawn.h"
#include "utils/interfaces.h"

class CCSPlayerPawn;

class CBasePlayerController : public CBaseEntity2
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerController);

	SCHEMA_FIELD(uint64, m_steamID)
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPawn)
	SCHEMA_FIELD_POINTER(char, m_iszPlayerName)

	void SetPawn(CCSPlayerPawn *pawn)
	{
		g_pKZUtils->SetPawn(this, pawn, true, false);
	}
};
