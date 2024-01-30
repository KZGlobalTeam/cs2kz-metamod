#pragma once

class CPlayer_MovementServices;
class CPlayer_ObserverServices;
class CCSPlayer_ItemServices;

#include "cbasemodelentity.h"


class CBasePlayerPawn : public CBaseModelEntity
{
public:
	DECLARE_SCHEMA_CLASS(CBasePlayerPawn);

	SCHEMA_FIELD(CPlayer_MovementServices*, m_pMovementServices)
	SCHEMA_FIELD(CHandle< CBasePlayerController >, m_hController)
	SCHEMA_FIELD(CCSPlayer_ItemServices*, m_pItemServices)
	SCHEMA_FIELD(CPlayer_ObserverServices*, m_pObserverServices)

	void CommitSuicide(bool bExplode, bool bForce)
	{
		CALL_VIRTUAL(void, offsets::CommitSuicide, this, bExplode, bForce);
	}
};
