#include "kz.h"
#include "utils/utils.h"

#include "tier0/memdbgon.h"

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

void KZPlayer::OnStartProcessMovement()
{
	this->EnableGodMode();
	this->HandleMoveCollision();
}
