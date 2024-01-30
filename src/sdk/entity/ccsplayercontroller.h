#pragma once

#include "cbaseplayercontroller.h"

class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_SCHEMA_CLASS(CCSPlayerController);
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPlayerPawn);

	void ChangeTeam(int iTeam)
	{
		CALL_VIRTUAL(void, offsets::ControllerChangeTeam, this, iTeam);
	}

	void SwitchTeam(int iTeam)
	{
		if (!IsController())
			return;

		if (iTeam == CS_TEAM_SPECTATOR)
		{
			ChangeTeam(iTeam);
		}
		else
		{
			g_pKZUtils->SwitchTeam(this, iTeam);
		}
	}

	void Respawn()
	{
		CCSPlayerPawn *pPawn = m_hPlayerPawn.Get();
		if (!pPawn || pPawn->IsAlive())
			return;

		SetPawn(pPawn);
		CALL_VIRTUAL(void, offsets::ControllerRespawn, this);
	}
};
