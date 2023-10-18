#include "kz.h"
#include "utils/utils.h"
#include "igameevents.h"
#include "tier0/memdbgon.h"

static const Vector NULL_VECTOR = Vector(0, 0, 0);

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
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
	if (!pawn) return;
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
	if (!pawn) return;
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
	if (!pawn) return;
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
	if (m_checkpoints.Count() <= 0)
	{
		utils::PrintChat(this->GetPawn(), "No checkpoints available.");
		return;
	}
	const Checkpoint cp = m_checkpoints[m_currentCpIndex];
	this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
}

void KZPlayer::TpToPrevCp()
{
	if (m_checkpoints.Count() <= 0)
	{
		utils::PrintChat(this->GetPawn(), "No checkpoints available.");
		return;
	}
	m_currentCpIndex = MAX(0, m_currentCpIndex - 1);
	const Checkpoint cp = m_checkpoints[m_currentCpIndex];
	this->Teleport(&cp.origin, &cp.angles, &NULL_VECTOR);
}

void KZPlayer::TpToNextCp()
{
	if (m_checkpoints.Count() <= 0)
	{
		utils::PrintChat(this->GetPawn(), "No checkpoints available.");
		return;
	}
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

void KZPlayer::OnStopProcessMovement()
{
	KZ::HUD::DrawSpeedPanel(this);
	MovementPlayer::OnStopProcessMovement();
}

void KZPlayer::ToggleHide()
{
	this->hideOtherPlayers = !this->hideOtherPlayers;
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	this->m_currentCpIndex = 0;
	this->hideOtherPlayers = false;
	this->m_checkpoints.Purge();
}