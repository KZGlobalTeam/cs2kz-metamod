#pragma once
#include "cbaseplayercontroller.h"
#include "sdk/services.h"
#include "random.h"

enum OverlapState : uint8
{
	PERFECT = 0x1,
	OVERLAP = 0x2,
	UNDERLAP = 0x3,
};

struct OverlapBuffer
{
	CUtlVector<OverlapState> sequences;
	uint32 current;
};

struct CStrafeStats
{
	OverlapBuffer buffer;
	uint32 overlaps[16];
	uint32 underlaps[16];
};
#ifndef IDA_IGNORE
class CCSPlayerController : public CBasePlayerController
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CCSPlayerController);
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hPlayerPawn);
	SCHEMA_FIELD(CHandle<CCSPlayerPawnBase>, m_hObserverPawn);
	SCHEMA_FIELD_POINTER_OFFSET(CStrafeStats, m_nNonSuspiciousHitStreak, 4)
	SCHEMA_FIELD(GameTime_t, m_LastTimePlayerWasDisconnectedForPawnsRemove)
	SCHEMA_FIELD(int32, m_DesiredObserverMode)
	SCHEMA_FIELD(CHandle<CCSPlayerPawn>, m_hDesiredObserverTarget)
	SCHEMA_FIELD(CUtlSymbolLarge, m_szClan)
	SCHEMA_FIELD(int32, m_iCompetitiveRanking)
	SCHEMA_FIELD(int8, m_iCompetitiveRankType)
	SCHEMA_FIELD(CCSPlayerController_InventoryServices *, m_pInventoryServices)

	void SetClan(const char *clan)
	{
		if (!clan)
		{
			return;
		}
		this->m_szClan(clan);
	}

	CStrafeStats *GetCStrafeStats()
	{
		return m_nNonSuspiciousHitStreak();
	}

	CCSPlayerPawn *GetPlayerPawn()
	{
		return m_hPlayerPawn.Get();
	}

	CCSPlayerPawnBase *GetObserverPawn()
	{
		return m_hObserverPawn.Get();
	}

	void ChangeTeam(int iTeam)
	{
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("ControllerChangeTeam"), this, iTeam);
	}

	void SwitchTeam(int iTeam)
	{
		if (!IsController())
		{
			return;
		}

		if (iTeam <= CS_TEAM_SPECTATOR)
		{
			ChangeTeam(iTeam);
		}
		else
		{
			g_pKZUtils->SwitchTeam(this, iTeam);
		}
	}

	// Respawns the player if the player is not alive, does nothing otherwise.
	void Respawn()
	{
		CCSPlayerPawn *pawn = GetPlayerPawn();
		if (!pawn || pawn->IsAlive())
		{
			return;
		}

		SetPawn(pawn);
		if (this->m_iTeamNum() != CS_TEAM_CT && this->m_iTeamNum() != CS_TEAM_T)
		{
			SwitchTeam(RandomInt(CS_TEAM_T, CS_TEAM_CT));
		}
		CALL_VIRTUAL(void, g_pGameConfig->GetOffset("ControllerRespawn"), this);
	}
};
#endif
