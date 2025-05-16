#include "kz_measure.h"
#include "utils/simplecmds.h"
#include "kz/beam/kz_beam.h"
#include "kz/language/kz_language.h"

#define KZ_MEASURE_TIMEOUT       60.0f
#define KZ_MEASURE_MAX_DISTANCE  32768.0f
#define KZ_MEASURE_COOLDOWN      0.5f
#define KZ_MEASURE_BEAM_LIFETIME 32
#define KZ_MEASURE_MIN_DIST      0.01f
#define KZ_MEASURE_SOUND_START   "UI.PlayerPing"
#define KZ_MEASURE_SOUND_END     "UI.PlayerPingUrgent"

void KZMeasureService::TryMeasure()
{
	if (this->startPos.IsValid())
	{
		if (!this->EndMeasure(KZ_MEASURE_MIN_DIST))
		{
			this->MeasureBlock();
		}
	}
	else
	{
		this->StartMeasure();
	}
}

void KZMeasureService::StartMeasure()
{
	player->measureService->startPos = player->measureService->GetLookAtPos();
	player->measureService->startPosSetTime = g_pKZUtils->GetServerGlobals()->curtime;
	if (!player->measureService->startPos.IsValid())
	{
		player->languageService->PrintChat(true, false, "Measure - Non Solid Position");
		return;
	}
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_MEASURE_SOUND_START);
	player->languageService->PrintChat(true, false, "Measure - Start Position Set");
}

bool KZMeasureService::EndMeasure(f32 minDistThreshold)
{
	if (g_pKZUtils->GetServerGlobals()->curtime - this->lastMeasureTime < KZ_MEASURE_COOLDOWN)
	{
		this->player->languageService->PrintChat(true, false, "Command Cooldown");
		return true;
	}
	if (!this->startPos.IsValid())
	{
		this->player->languageService->PrintChat(true, false, "Measure - Points Not Set");
		return true;
	}
	KZMeasureService::MeasurePos endPos = this->GetLookAtPos();
	if (!endPos.IsValid())
	{
		this->player->languageService->PrintChat(true, false, "Measure - Non Solid Position");
		return true;
	}

	f32 horizontalDist = (endPos.origin - this->startPos.origin).Length2D();
	f32 verticalDist = endPos.origin.z - this->startPos.origin.z;
	f32 effectiveDist = KZMeasureService::GetEffectiveDistance(this->startPos.origin, endPos.origin);
	// Wait, are we trying to measure a block?
	if (minDistThreshold >= 0.0f && horizontalDist <= minDistThreshold && verticalDist <= minDistThreshold)
	{
		this->startPos.Invalidate();
		return false;
	}
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_MEASURE_SOUND_END);
	this->player->languageService->PrintChat(true, false, "Measure - Result", horizontalDist, effectiveDist, verticalDist);
	this->lastMeasureTime = g_pKZUtils->GetServerGlobals()->curtime;

	Vector beamStart, beamEnd;
	KZMeasureService::CalculateAdjustedBeamOrigins(this->startPos.origin, endPos.origin, beamStart, beamEnd);
	this->player->beamService->AddInstantBeam(beamStart, beamEnd, KZ_MEASURE_BEAM_LIFETIME);

	this->startPos.Invalidate();
	return true;
}

void KZMeasureService::MeasureBlock()
{
	auto start = this->GetLookAtPos();
	if (!start.IsValid())
	{
		this->player->languageService->PrintChat(true, false, "Measure - Non Solid Position");
		return;
	}
	QAngle angles;
	VectorAngles(start.normal, angles);
	auto end = this->GetLookAtPos(&start.origin, &angles);
	if (!end.IsValid())
	{
		this->player->languageService->PrintChat(true, false, "Measure - Non Solid Position");
		return;
	}
	if ((start.normal + end.normal).Length() > EPSILON)
	{
		this->player->languageService->PrintChat(true, false, "Measure - Blocks Not Aligned");
		return;
	}
	u32 block = roundf((start.origin - end.origin).Length2D());
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_MEASURE_SOUND_END);
	this->player->languageService->PrintChat(true, false, "Measure - Block Result", block);

	Vector beamStart, beamEnd;
	KZMeasureService::CalculateAdjustedBeamOrigins(start.origin, end.origin, beamStart, beamEnd);
	this->player->beamService->AddInstantBeam(beamStart, beamEnd, KZ_MEASURE_BEAM_LIFETIME);
}

KZMeasureService::MeasurePos KZMeasureService::GetLookAtPos(const Vector *overrideOrigin, const QAngle *overrideAngles)
{
	Vector origin;
	CCSPlayerPawnBase *pawn = static_cast<CCSPlayerPawnBase *>(this->player->GetCurrentPawn());
	if (overrideOrigin)
	{
		origin = *overrideOrigin;
	}
	else
	{
		if (!pawn)
		{
			return {vec3_invalid, vec3_invalid};
		}
		this->player->GetEyeOrigin(&origin);
	}
	QAngle angles = overrideAngles ? *overrideAngles : pawn->m_angEyeAngles();

	Vector forward;
	AngleVectors(angles, &forward);
	Vector endPos = origin + forward * KZ_MEASURE_MAX_DISTANCE;
	trace_t tr;
	bbox_t bounds({vec3_origin, vec3_origin});
	CTraceFilterPlayerMovementCS filter;
	g_pKZUtils->InitPlayerMovementTraceFilter(filter, pawn, pawn->m_Collision().m_collisionAttribute().m_nInteractsWith(),
											  COLLISION_GROUP_PLAYER_MOVEMENT);
	g_pKZUtils->InitGameTrace(&tr);
	g_pKZUtils->TracePlayerBBox(origin, endPos, bounds, &filter, tr);
	if (tr.DidHit())
	{
		return {tr.m_vEndPos, tr.m_vHitNormal};
	}
	return {vec3_invalid, vec3_invalid};
}

void KZMeasureService::OnPhysicsSimulatePost()
{
	if (g_pKZUtils->GetServerGlobals()->curtime - this->startPosSetTime > KZ_MEASURE_TIMEOUT && this->startPos.IsValid())
	{
		this->startPos.Invalidate();
		this->player->languageService->PrintChat(true, false, "Measure - Start Position Timeout");
	}
}

f32 KZMeasureService::GetEffectiveDistance(Vector pointA, Vector pointB)
{
	f32 Ax = Min(pointA[0], pointB[0]) + 16.0f;
	f32 Bx = Max(pointA[0], pointB[0]) - 16.0f;
	f32 Ay = Min(pointA[1], pointB[1]) + 16.0f;
	f32 By = Max(pointA[1], pointB[1]) - 16.0f;

	if (Bx < Ax)
	{
		Ax = Bx;
	}
	if (By < Ay)
	{
		Ay = By;
	}

	return sqrt(pow(Ax - Bx, 2.0f) + pow(Ay - By, 2.0f)) + 32.0f;
}

void KZMeasureService::CalculateAdjustedBeamOrigins(const Vector &start, const Vector &end, Vector &adjustedStart, Vector &adjustedEnd)
{
	// The beam starts ~21% into the distance and ends 96.875% in (if it's drawn over 32 ticks),
	// so we need to offset the start and end position accordingly:
	/*
		old_start = new_start + (new_end - new_start) * 0.21
		old_end = new_start + (new_end - new_start) * 0.96875
		old_start = new_start * 0.9 + new_end * 0.21
		old_end = new_end * 0.96875 + new_start * 0.03125

		new_start = (old_start - new_end * 0.21) / 0.79										(1)
		old_end = new_end * 0.96875 + (old_start - new_end * 0.21) / 0.79 * 0.03125
		old_end = new_end * (0.96875 - 0.03125/0.9*0.21) + old_start * 0.03125/0.9

		new_end = (old_end - old_start * 0.03125 / 0.79) / (0.96875 - 0.03125 / 0.79 * 0.21) (2)
	*/

	adjustedEnd = (end - start * 0.03125 / 0.79) / (0.96875 - 0.03125 / 0.79 * 0.21);
	adjustedStart = (start - adjustedEnd * 0.21) / 0.79;
}

SCMD(kz_measure, SCFL_MEASURE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->measureService->TryMeasure();
	return MRES_SUPERCEDE;
}

SCMD(kz_measurestart, SCFL_MEASURE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->measureService->StartMeasure();
	return MRES_SUPERCEDE;
}

SCMD(kz_measureend, SCFL_MEASURE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->measureService->EndMeasure();
	return MRES_SUPERCEDE;
}

SCMD(kz_measureblock, SCFL_MEASURE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->measureService->MeasureBlock();
	return MRES_SUPERCEDE;
}
