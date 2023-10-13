#include "kz.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

static const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::OnStartTouchGround()
{

}
void KZPlayer::HandleMoveCollision()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (pawn->m_lifeState() != LIFE_ALIVE)
	{
		DisableNoclip();
		return;
	}
	if (this->inNoclip)
	{
		if (pawn->m_MoveType() != MOVETYPE_NOCLIP)
		{
			utils::SetEntityMoveType(pawn, MOVETYPE_NOCLIP);
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_STANDARD)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_STANDARD;
			utils::EntityCollisionRulesChanged(pawn);
		}
	}
	else
	{
		if (pawn->m_MoveType() == MOVETYPE_NOCLIP)
		{
			utils::SetEntityMoveType(pawn, MOVETYPE_WALK);
		}
		if (pawn->m_Collision().m_CollisionGroup() != KZ_COLLISION_GROUP_NOTRIGGER)
		{
			pawn->m_Collision().m_CollisionGroup() = KZ_COLLISION_GROUP_NOTRIGGER;
			utils::EntityCollisionRulesChanged(pawn);
		}
	}
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	Color ogColor = pawn->m_clrRender();
	if (pawn->m_clrRender().a() != 254)
	{
		pawn->m_clrRender(Color(255, 255, 255, 254));
	}
	else
	{
		pawn->m_clrRender(Color(255, 255, 255, 255));
	}
}

void KZPlayer::ToggleNoclip()
{
	this->inNoclip = !this->inNoclip;
}

void KZPlayer::DisableNoclip()
{
	this->inNoclip = false;
}

void KZPlayer::SetCheckpoint()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	u32 flags = pawn->m_fFlags();
	if (!(flags & FL_ONGROUND))
	{
		return;
	}
	
	Checkpoint cp = {};
	this->GetOrigin(&cp.origin),
	this->GetAngles(&cp.angles),
	m_checkpoints.AddToTail(cp);
	// newest checkpoints aren't deleted after using prev cp.
	m_currentCpIndex = m_checkpoints.Count() - 1;
}

void KZPlayer::TpToCheckpoint()
{
	const Checkpoint cp = m_checkpoints[m_currentCpIndex];
	this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
}

void KZPlayer::TpToPrevCp()
{
	m_currentCpIndex = MAX(0, m_currentCpIndex - 1);
	const Checkpoint cp = m_checkpoints[m_currentCpIndex];
	this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
}

void KZPlayer::TpToNextCp()
{
	m_currentCpIndex = MIN(m_currentCpIndex + 1, m_checkpoints.Count() - 1);
	const Checkpoint cp = m_checkpoints[m_currentCpIndex];
	this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
}

void KZPlayer::OnStartProcessMovement()
{
	MovementPlayer::OnStartProcessMovement();
	this->EnableGodMode();
	this->HandleMoveCollision();
}
