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
	return dynamic_cast<CCSPlayerPawn *>(controller->m_hPawn().Get());
}

void MovementPlayer::GetOrigin(Vector *origin)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;

	*origin = this->GetController()->m_hPawn().Get()->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
}

void MovementPlayer::SetOrigin(const Vector& origin)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	CALL_VIRTUAL(void, offsets::Teleport, pawn, origin, NULL, NULL);
}

void MovementPlayer::GetVelocity(Vector *velocity)
{
	CCSPlayerController *controller = this->GetController();
	if (!controller) return;
	CHandle<CCSPlayerPawn> pawnHandle = controller->m_hPawn();
	if (!pawnHandle || !pawnHandle.Get()) return;

	*velocity = this->GetController()->m_hPawn().Get()->m_vecAbsVelocity();
}

void MovementPlayer::SetVelocity(const Vector& velocity)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	CALL_VIRTUAL(void, offsets::Teleport, pawn, NULL, NULL, velocity);
}

void MovementPlayer::GetAngles(QAngle *angles)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;

	*angles = this->GetController()->m_hPawn().Get()->v_angle();
}

void MovementPlayer::SetAngles(const QAngle &angles)
{
	CBasePlayerPawn *pawn = this->GetPawn();
	if (!pawn) return;
	CALL_VIRTUAL(void, offsets::Teleport, pawn, NULL, angles, NULL);
}

TurnState MovementPlayer::GetTurning()
{
	QAngle currentAngle = this->moveData_Pre.m_vecViewAngles;
	bool turning = this->oldAngles.y != currentAngle.y;
	if (!turning) return TURN_NONE;
	if (currentAngle.y < this->oldAngles.y - 180
		|| currentAngle.y > this->oldAngles.y && currentAngle.y < this->oldAngles.y + 180) return TURN_LEFT;
	return TURN_RIGHT;
}