#pragma once
#include "cbaseentity.h"
#include "cbaseplayercontroller.h"
#ifndef IDA_IGNORE

class CTeam : public CBaseEntity
{
public:
	DECLARE_SCHEMA_CLASS_ENTITY(CTeam)

	// The scoreboard lists a team's players from here, not from the team number of each controller.
	// This is the m_aPlayers of clients and demos, the schema's own m_aPlayers holds pawns.
	SCHEMA_FIELD_POINTER(CUtlVector<CHandle<CBasePlayerController>>, m_aPlayerControllers)

	static CTeam *Find(i32 teamNum)
	{
		if (!GameEntitySystem())
		{
			return nullptr;
		}
		EntityInstanceByClassIter_t iter(NULL, "cs_team_manager");
		for (CEntityInstance *ent = iter.First(); ent; ent = iter.Next())
		{
			CTeam *team = static_cast<CTeam *>(ent);
			if (team->m_iTeamNum() == teamNum)
			{
				return team;
			}
		}
		return nullptr;
	}

	// A team change moves the controller between the lists. Writing the team number directly has to do the same.
	static void MovePlayer(CBasePlayerController *controller, i32 fromTeam, i32 toTeam)
	{
		CHandle<CBasePlayerController> handle(controller);
		CTeam *from = CTeam::Find(fromTeam);
		if (from && from->m_aPlayerControllers()->FindAndRemove(handle))
		{
			from->m_aPlayerControllers.NetworkStateChanged();
		}
		CTeam *to = CTeam::Find(toTeam);
		if (to && !to->m_aPlayerControllers()->HasElement(handle))
		{
			to->m_aPlayerControllers()->AddToTail(handle);
			to->m_aPlayerControllers.NetworkStateChanged();
		}
	}
};
#endif
