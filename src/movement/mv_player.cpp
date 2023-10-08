#include "movement.h"
#include "tier0/memdbgon.h"

void MovementPlayer::OnProcessMovement()
{
}

void MovementPlayer::OnStartDucking()
{
}

void MovementPlayer::OnStopDucking()
{
}

void MovementPlayer::OnStartTouchGround()
{
}

void MovementPlayer::OnStopTouchGround()
{
}

void MovementPlayer::OnChangeMoveType()
{
}

void MovementPlayer::OnPlayerJump()
{
}

void MovementPlayer::OnAirAccelerate()
{
}

CCSPlayerController *MovementPlayer::GetController()
{
	return dynamic_cast<CCSPlayerController *>(g_pEntitySystem->GetBaseEntity(CEntityIndex(this->index)));
}

CCSPlayerPawn *MovementPlayer::GetPawn()
{
	CCSPlayerController *controller = this->GetController();
	if (!controller) return nullptr;
	return dynamic_cast<CCSPlayerPawn *>(controller->m_hPawn.Get());
}

void MovementPlayer::GetOrigin(Vector *origin)
{
	CCSPlayerController *controller = this->GetController();
	if (!controller) return;
	CBasePlayerPawn *pawn = controller->m_hPawn.Get();
	if (!pawn) return;

	*origin = this->GetController()->m_hPawn.Get()->m_pSceneNode->m_vecAbsOrigin;
}

void MovementPlayer::SetOrigin(const Vector& origin)
{
	// We need to call NetworkStateChanged here because it's a networked field, but the technology isn't there yet...
	// TODO
}

void MovementPlayer::GetVelocity(Vector *velocity)
{
	CCSPlayerController *controller = this->GetController();
	if (!controller) return;
	CBasePlayerPawn *pawn = controller->m_hPawn.Get();
	if (!pawn) return;

	*velocity = this->GetController()->m_hPawn.Get()->m_vecAbsVelocity;
}

void MovementPlayer::SetVelocity(const Vector& velocity)
{
	// We need to call NetworkStateChanged here because it's a networked field, but the technology isn't there yet...
	// TODO
}