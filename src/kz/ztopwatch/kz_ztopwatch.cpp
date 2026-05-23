#include "kz_ztopwatch.h"
#include "kz/language/kz_language.h"
#include "kz/timer/kz_timer.h"
#include "kz/mode/kz_mode.h"

#include "utils/simplecmds.h"
#include "utils/utils.h"
#include "sdk/entity/cparticlesystem.h"
#include "sdk/tracefilter.h"
#include "sdk/navphysicsinterface.h"
#include "entitykeyvalues.h"

#define KZ_ZONE_SW_MAX_TRACE_DIST 256.0f
static_function void PrintUsage(KZPlayer *player);

static_global class : public KZTimerServiceEventListener
{
	virtual void OnTimerStartPost(KZPlayer *player, u32 courseGUID) override
	{
		player->ztopwatchService->OnTimerStart();
	}

	virtual void OnTimerStopped(KZPlayer *player, u32 courseGUID) override
	{
		player->ztopwatchService->OnTimerStop();
	}

	virtual void OnTimerEndPost(KZPlayer *player, u32 courseGUID, f32 time, u32 teleportsUsed) override
	{
		player->ztopwatchService->OnTimerStop();
	}
}

timerEventListener;

void KZZtopwatchService::Init()
{
	KZTimerService::RegisterEventListener(&timerEventListener);
}

static_function CEntityHandle CreateZoneLine(const Vector &start, const Vector &end, const Color &color)
{
	CParticleSystem *line = utils::CreateEntityByName<CParticleSystem>("info_particle_system");
	CEntityKeyValues *pKeyValues = new CEntityKeyValues();
	pKeyValues->SetString("effect_name", KZ_ZONE_SW_LINE_PARTICLE);
	pKeyValues->SetVector("origin", start);
	pKeyValues->SetInt("tint_cp", 16);
	pKeyValues->SetColor("tint_cp_color", color);
	pKeyValues->SetInt("data_cp", 1);
	pKeyValues->SetVector("data_cp_value", end);
	pKeyValues->SetBool("start_active", true);
	line->m_iTeamNum(CUSTOM_PARTICLE_SYSTEM_TEAM);
	line->DispatchSpawn(pKeyValues);
	return line->GetRefEHandle();
}

bool KZZtopwatchService::GetLookAtPos(Vector &outPos)
{
	CCSPlayerPawnBase *pawn = static_cast<CCSPlayerPawnBase *>(this->player->GetCurrentPawn());
	if (!pawn)
	{
		return false;
	}

	Vector origin;
	this->player->GetEyeOrigin(&origin);
	QAngle angles = pawn->v_angle();

	Vector forward;
	AngleVectors(angles, &forward);
	Vector endPos = origin + forward * KZ_ZONE_SW_MAX_TRACE_DIST;

	trace_t tr;
	CTraceFilterPlayerMovementCS filter(pawn);
	INavPhysicsInterface::TraceLine(origin, endPos, &filter, &tr);

	outPos = tr.DidHit() ? tr.m_vEndPos + tr.m_vHitNormal * 0.0625f : tr.m_vEndPos;
	return true;
}

bool KZZtopwatchService::SetZonePoint(Zone &zone, bool isPoint1)
{
	Vector pos;
	GetLookAtPos(pos);
	if (isPoint1)
	{
		zone.point1 = pos;
		zone.point1Active = true;
	}
	else
	{
		zone.point2 = pos;
		zone.point2Active = true;
	}
	if (zone.IsValid())
	{
		zone.SetMinsMaxs();
	}
	return true;
}

void KZZtopwatchService::RemoveZoneVisual(Zone &zone)
{
	for (int i = 0; i < Zone::NUM_EDGES; i++)
	{
		if (zone.edges[i].Get())
		{
			g_pKZUtils->RemoveEntity(zone.edges[i].Get());
			zone.edges[i] = {};
		}
	}
}

void KZZtopwatchService::UpdateZoneVisual(Zone &zone, const Color &color)
{
	RemoveZoneVisual(zone);
	if (!zone.IsValid())
	{
		return;
	}

	const Vector &mn = zone.mins;
	const Vector &mx = zone.maxs;

	// 8 corners of the AABB
	Vector corners[8] = {
		Vector(mn.x, mn.y, mn.z), // 0 bottom
		Vector(mx.x, mn.y, mn.z), // 1
		Vector(mx.x, mx.y, mn.z), // 2
		Vector(mn.x, mx.y, mn.z), // 3
		Vector(mn.x, mn.y, mx.z), // 4 top
		Vector(mx.x, mn.y, mx.z), // 5
		Vector(mx.x, mx.y, mx.z), // 6
		Vector(mn.x, mx.y, mx.z), // 7
	};

	// 12 edges: 4 bottom, 4 top, 4 vertical
	const int edgePairs[12][2] = {
		{0, 1}, {1, 2}, {2, 3}, {3, 0}, // bottom face
		{4, 5}, {5, 6}, {6, 7}, {7, 4}, // top face
		{0, 4}, {1, 5}, {2, 6}, {3, 7}, // verticals
	};

	for (int i = 0; i < Zone::NUM_EDGES; i++)
	{
		zone.edges[i] = CreateZoneLine(corners[edgePairs[i][0]], corners[edgePairs[i][1]], color);
	}
}

void KZZtopwatchService::PlaceNextPoint()
{
	if (!this->enabled)
	{
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Disabled");
		return;
	}
	const char *pointPhrases[] = {
		"Zone Stopwatch - Point Name - Start 1",
		"Zone Stopwatch - Point Name - Start 2",
		"Zone Stopwatch - Point Name - End 1",
		"Zone Stopwatch - Point Name - End 2",
	};

	Zone *zone = (this->nextPoint < 2) ? &this->startZone : &this->endZone;
	bool isPoint1 = (this->nextPoint % 2 == 0);
	const Color &color = (this->nextPoint < 2) ? Color(0, 255, 0, 255) : Color(255, 0, 0, 255);

	if (!this->SetZonePoint(*zone, isPoint1))
	{
		return;
	}

	std::string pointName = this->player->languageService->PrepareMessage(pointPhrases[this->nextPoint]);
	this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Point Set", pointName.c_str());
	this->UpdateZoneVisual(*zone, color);

	this->nextPoint = (this->nextPoint + 1) % 4;
}

void KZZtopwatchService::ToggleStartOnJump()
{
	this->startOnJump = !this->startOnJump;
	if (this->startOnJump)
	{
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Start On Jump Enabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Start On Jump Disabled");
	}
}

void KZZtopwatchService::ToggleStopOnLand()
{
	this->stopOnLand = !this->stopOnLand;
	if (this->stopOnLand)
	{
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Stop On Land Enabled");
	}
	else
	{
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Stop On Land Disabled");
	}
}

void KZZtopwatchService::ResetZones()
{
	this->startZone.Reset();
	this->endZone.Reset();
	this->startTime = 0.0f;
	this->nextPoint = 0;
	this->RemoveZoneVisual(this->startZone);
	this->RemoveZoneVisual(this->endZone);
	this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Reset");
}

void KZZtopwatchService::Toggle()
{
	this->enabled = !this->enabled;
	if (this->enabled)
	{
		if (this->startZone.IsValid())
		{
			this->UpdateZoneVisual(this->startZone, Color(0, 255, 0, 255));
		}
		if (this->endZone.IsValid())
		{
			this->UpdateZoneVisual(this->endZone, Color(255, 0, 0, 255));
		}
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Enabled");
		PrintUsage(this->player);
	}
	else
	{
		this->RemoveZoneVisual(this->startZone);
		this->RemoveZoneVisual(this->endZone);
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Disabled");
	}
}

void KZZtopwatchService::Reset()
{
	this->startZone.Reset();
	this->endZone.Reset();
	this->startTime = 0.0f;
	this->enabled = false;
	this->startOnJump = false;
	this->stopOnLand = false;
	this->nextPoint = 0;
	this->RemoveZoneVisual(this->startZone);
	this->RemoveZoneVisual(this->endZone);
}

bool KZZtopwatchService::IsPlayerInZone(const Zone &zone)
{
	if (!zone.IsValid())
	{
		return false;
	}

	Vector origin = this->player->currentMoveData->m_vecAbsOrigin;
	bbox_t bounds;
	this->player->GetBBoxBounds(&bounds);

	Vector playerMins = origin + bounds.mins;
	Vector playerMaxs = origin + bounds.maxs;

	// clang-format off
	return playerMins.x <= zone.maxs.x && playerMaxs.x >= zone.mins.x
		&& playerMins.y <= zone.maxs.y && playerMaxs.y >= zone.mins.y
		&& playerMins.z <= zone.maxs.z && playerMaxs.z >= zone.mins.z;
	// clang-format on
}

void KZZtopwatchService::TryStartTimer()
{
	if (!this->startZone.IsValid())
	{
		return;
	}
	if (this->IsPlayerInZone(this->startZone))
	{
		this->startTime = g_pKZUtils->GetGlobals()->curtime;
	}
}

void KZZtopwatchService::TryStopTimer()
{
	if (this->startTime == 0.0f || !this->endZone.IsValid())
	{
		return;
	}
	if (this->IsPlayerInZone(this->endZone))
	{
		f32 elapsed = g_pKZUtils->GetGlobals()->curtime - this->startTime;
		this->startTime = 0.0f;
		this->player->languageService->PrintChat(true, false, "Zone Stopwatch - Time", utils::FormatTime(elapsed).Get());
	}
}

void KZZtopwatchService::OnTimerStart()
{
	if (!this->enabled)
	{
		return;
	}
	this->RemoveZoneVisual(this->startZone);
	this->RemoveZoneVisual(this->endZone);
}

void KZZtopwatchService::OnTimerStop()
{
	if (!this->enabled)
	{
		return;
	}
	if (this->startZone.IsValid())
	{
		this->UpdateZoneVisual(this->startZone, Color(0, 255, 0, 255));
	}
	if (this->endZone.IsValid())
	{
		this->UpdateZoneVisual(this->endZone, Color(255, 0, 0, 255));
	}
}

void KZZtopwatchService::OnProcessMovementPost()
{
	if (!this->enabled || this->player->timerService->GetTimerRunning())
	{
		return;
	}
	if (!this->startZone.IsValid() || !this->endZone.IsValid())
	{
		return;
	}
	if (!this->player->modeService->CanTouchTimerZone())
	{
		return;
	}
	if (!this->startOnJump)
	{
		this->TryStartTimer();
	}
	if (this->stopOnLand)
	{
		if (this->player->GetPlayerPawn()->m_fFlags() & FL_ONGROUND)
		{
			this->TryStopTimer();
		}
	}
	else
	{
		this->TryStopTimer();
	}
}

void KZZtopwatchService::OnJump()
{
	if (!this->enabled || !this->startOnJump)
	{
		return;
	}
	this->TryStartTimer();
}

// ============
//   COMMANDS
// ============

static_function void PrintUsage(KZPlayer *player)
{
	player->languageService->PrintChat(true, false, "Zone Stopwatch - Usage");
	player->languageService->PrintConsole(false, false, "Zone Stopwatch - Usage - Console");
}

static_function void PrintStatus(KZPlayer *player)
{
	KZZtopwatchService *sw = player->ztopwatchService;
	const char *pointPhrases[] = {
		"Zone Stopwatch - Point Name - Start 1",
		"Zone Stopwatch - Point Name - Start 2",
		"Zone Stopwatch - Point Name - End 1",
		"Zone Stopwatch - Point Name - End 2",
	};
	bool pointSet[] = {sw->startZone.point1Active, sw->startZone.point2Active, sw->endZone.point1Active, sw->endZone.point2Active};
	std::string strOn = player->languageService->PrepareMessage("Zone Stopwatch - Status On");
	std::string strOff = player->languageService->PrepareMessage("Zone Stopwatch - Status Off");
	std::string strSet = player->languageService->PrepareMessage("Zone Stopwatch - Status Set");
	std::string strUnset = player->languageService->PrepareMessage("Zone Stopwatch - Status Unset");
	player->languageService->PrintChat(true, false, "Zone Stopwatch - Status Header", sw->enabled ? strOn.c_str() : strOff.c_str());
	for (int i = 0; i < 4; i++)
	{
		std::string pointLabel = player->languageService->PrepareMessage(pointPhrases[i]);
		std::string nextStr = (sw->nextPoint == i) ? player->languageService->PrepareMessage("Zone Stopwatch - Status Next") : "";
		player->languageService->PrintChat(true, false, "Zone Stopwatch - Status Point", pointLabel.c_str(),
										   pointSet[i] ? strSet.c_str() : strUnset.c_str(), nextStr.c_str());
	}
	player->languageService->PrintChat(true, false, "Zone Stopwatch - Status Options", sw->startOnJump ? strOn.c_str() : strOff.c_str(),
									   sw->stopOnLand ? strOn.c_str() : strOff.c_str());
	PrintUsage(player);
}

SCMD(kz_ztopwatch, SCFL_MISC)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (player->timerService->GetTimerRunning())
	{
		player->languageService->PrintChat(true, false, "Zone Stopwatch - Timer Running");
		return MRES_SUPERCEDE;
	}
	if (args->ArgC() < 2)
	{
		player->ztopwatchService->Toggle();
		return MRES_SUPERCEDE;
	}
	const char *sub = args->Arg(1);
	if (KZ_STREQI(sub, "place"))
	{
		player->ztopwatchService->PlaceNextPoint();
	}
	else if (KZ_STREQI(sub, "clear"))
	{
		player->ztopwatchService->ResetZones();
	}
	else if (KZ_STREQI(sub, "startjump") || KZ_STREQI(sub, "sj"))
	{
		player->ztopwatchService->ToggleStartOnJump();
	}
	else if (KZ_STREQI(sub, "stopland") || KZ_STREQI(sub, "sl"))
	{
		player->ztopwatchService->ToggleStopOnLand();
	}
	else if (KZ_STREQI(sub, "status"))
	{
		PrintStatus(player);
	}
	else
	{
		PrintUsage(player);
	}
	return MRES_SUPERCEDE;
}

static_global SCmdLink kz_zw_link("kz_zw", "kz_ztopwatch");
