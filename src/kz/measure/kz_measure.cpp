#include "kz_measure.h"
#include "utils/simplecmds.h"
#include "kz/beam/kz_beam.h"
#include "kz/language/kz_language.h"

#include "sdk/entity/cparticlesystem.h"
#include "sdk/tracefilter.h"
#include "entitykeyvalues.h"

#define KZ_MEASURE_TIMEOUT      60.0f
#define KZ_MEASURE_DURATION     4.0f
#define KZ_MEASURE_MAX_DISTANCE 32768.0f
#define KZ_MEASURE_COOLDOWN     0.5f
#define KZ_MEASURE_MIN_DIST     0.01f
#define KZ_MEASURE_SOUND_START  "UI.PlayerPing"
#define KZ_MEASURE_SOUND_END    "UI.PlayerPingUrgent"

static_function CEntityHandle CreateMeasureBeam(const Vector &start, const Vector &end)
{
	CParticleSystem *measurer = utils::CreateEntityByName<CParticleSystem>("info_particle_system");
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("effect_name", "particles/ui/annotation/ui_annotation_line_segment.vpcf");
	pKeyValues->SetVector("origin", start);
	pKeyValues->SetInt("tint_cp", 16);
	pKeyValues->SetColor("tint_cp_color", Color(255, 255, 255, 255));
	pKeyValues->SetInt("data_cp", 1);
	pKeyValues->SetVector("data_cp_value", end);
	pKeyValues->SetBool("start_active", true);
	measurer->m_iTeamNum(CUSTOM_PARTICLE_SYSTEM_TEAM);
	measurer->DispatchSpawn(pKeyValues);
	return measurer->GetRefEHandle();
}

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
	this->player->measureService->startPos = this->player->measureService->GetLookAtPos();
	this->player->measureService->startPosSetTime = g_pKZUtils->GetServerGlobals()->curtime;
	if (!this->player->measureService->startPos.IsValid())
	{
		this->player->languageService->PrintChat(true, false, "Measure - Non Solid Position");
		return;
	}
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_MEASURE_SOUND_START);
	this->player->languageService->PrintChat(true, false, "Measure - Start Position Set");
	if (this->measurerHandle.Get())
	{
		g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
	}

	this->measurerHandle = CreateMeasureBeam(this->player->measureService->startPos.origin + this->player->measureService->startPos.normal * 0.25f,
											 this->player->measureService->startPos.origin + this->player->measureService->startPos.normal * 0.25f);
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
		if (this->measurerHandle.Get())
		{
			g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
		}
		return false;
	}
	utils::PlaySoundToClient(this->player->GetPlayerSlot(), KZ_MEASURE_SOUND_END);
	this->player->languageService->PrintChat(true, false, "Measure - Result", horizontalDist, effectiveDist, verticalDist);
	this->lastMeasureTime = g_pKZUtils->GetServerGlobals()->curtime;

	if (this->measurerHandle.Get())
	{
		g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
	}

	this->measurerHandle = CreateMeasureBeam(this->player->measureService->startPos.origin + this->player->measureService->startPos.normal * 0.25f,
											 endPos.origin + endPos.normal * 0.25f);

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

	if (this->measurerHandle.Get())
	{
		g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
	}
	this->measurerHandle = CreateMeasureBeam(start.origin + start.normal * 0.25f, end.origin + end.normal * 0.25f);
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

	CTraceFilterPlayerMovementCS filter(pawn);

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
		if (this->measurerHandle.Get())
		{
			g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
			this->measurerHandle = {};
		}
	}
	if (this->measurerHandle.Get())
	{
		CParticleSystem *measurer = static_cast<CParticleSystem *>(this->measurerHandle.Get());
		if (measurer && g_pKZUtils->GetServerGlobals()->curtime - measurer->m_flStartTime().GetTime() > KZ_MEASURE_DURATION)
		{
			g_pKZUtils->RemoveEntity(this->measurerHandle.Get());
			this->measurerHandle = {};
		}
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
