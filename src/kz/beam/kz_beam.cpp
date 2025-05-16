#include "kz_beam.h"
#include "kz/spec/kz_spec.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/language/kz_language.h"
#include "kz/option/kz_option.h"

#include "utils/simplecmds.h"
#include "utils/ctimer.h"

using namespace KZ::beam;

static_global class KZOptionServiceEventListener_Beam : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		player->beamService->OnPlayerPreferencesLoaded();
	}
} optionEventListener;

void KZBeamService::OnPlayerPreferencesLoaded()
{
	this->SetBeamType(this->player->optionService->GetPreferenceInt("desiredBeamType"));
	this->playerBeamOffset = this->player->optionService->GetPreferenceVector("beamOffset", KZBeamService::defaultOffset);
}

SCMD(kz_beam, SCFL_MISC | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	u8 newDesiredBeamType = (player->beamService->desiredBeamType + 1) % KZBeamService::BEAM_COUNT;
	if (args->ArgC() >= 2)
	{
		if (KZ_STREQI(args->Arg(1), "off"))
		{
			newDesiredBeamType = KZBeamService::BEAM_NONE;
		}
		else if (KZ_STREQI(args->Arg(1), "feet"))
		{
			newDesiredBeamType = KZBeamService::BEAM_FEET;
		}
		else if (KZ_STREQI(args->Arg(1), "ground"))
		{
			newDesiredBeamType = KZBeamService::BEAM_GROUND;
		}
		else
		{
			player->languageService->PrintChat(true, false, "Beam Command Usage");
		}
	}
	else
	{
		player->beamService->SetBeamType(newDesiredBeamType);
	}
	player->beamService->buffer.Clear();
	switch (player->beamService->desiredBeamType)
	{
		case KZBeamService::BEAM_NONE:
		{
			player->languageService->PrintChat(true, false, "Beam Changed (None)");
			break;
		}
		case KZBeamService::BEAM_FEET:
		{
			player->languageService->PrintChat(true, false, "Beam Changed (Feet)");
			break;
		}
		case KZBeamService::BEAM_GROUND:
		{
			player->languageService->PrintChat(true, false, "Beam Changed (Ground)");
			break;
		}
	}
	player->optionService->SetPreferenceInt("desiredBeamType", player->beamService->desiredBeamType);
	return MRES_HANDLED;
}

SCMD(kz_beamoffset, SCFL_MISC | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (args->ArgC() < 4 || !utils::IsNumeric(args->Arg(1)) || !utils::IsNumeric(args->Arg(2)) || !utils::IsNumeric(args->Arg(3)))
	{
		player->languageService->PrintChat(true, false, "Beam Offset Command Usage");
		player->languageService->PrintChat(true, false, "Current Beam Offset", player->beamService->playerBeamOffset.x,
										   player->beamService->playerBeamOffset.y, player->beamService->playerBeamOffset.z);
	}
	player->beamService->playerBeamOffset = Vector(atof(args->Arg(1)), atof(args->Arg(2)), atof(args->Arg(3)));

	player->optionService->SetPreferenceVector("beamOffset", player->beamService->playerBeamOffset);
	return MRES_HANDLED;
}

static_function CBaseModelEntity *CreateGrenadeEnt(i32 team)
{
	CBaseModelEntity *beamEnt = utils::CreateEntityByName<CBaseModelEntity>("smokegrenade_projectile");
	beamEnt->m_iTeamNum(team);
	beamEnt->DispatchSpawn();
	beamEnt->m_clrRender({0, 0, 0, 0});
	beamEnt->m_nRenderMode(10);
	beamEnt->m_nActualMoveType = MOVETYPE_NONE;
	beamEnt->m_MoveType = MOVETYPE_NONE;
	beamEnt->m_flGravityScale = 0.0f;

	return beamEnt;
}

void KZBeamService::Update()
{
	KZPlayer *newTarget = this->player->IsAlive() ? this->player : this->player->specService->GetSpectatedPlayer();

	if (this->target != newTarget)
	{
		this->buffer.Clear();
		this->target = newTarget;
	}

	if (!this->target || !this->target->GetPlayerPawn())
	{
		this->playerBeam.moving = false;
		return;
	}

	if (this->target->noclipService->JustNoclipped() && !this->noclipTick)
	{
		this->noclipTick = g_pKZUtils->GetServerGlobals()->tickcount + originHistorySize;
	}

	if ((this->target->GetPlayerPawn()->m_fFlags & FL_ONGROUND && this->target->GetMoveType() == MOVETYPE_WALK)
		|| this->target->GetMoveType() == MOVETYPE_LADDER)
	{
		this->noclipTick = 0;
		this->canResumeBeam = true;
	}

	bool shouldStopBeam =
		this->target->GetPlayerPawn()->m_fFlags & FL_ONGROUND || this->target->GetMoveType() != MOVETYPE_WALK || !this->target->IsAlive();
	if (shouldStopBeam && this->beamStopTick <= 0)
	{
		// Add a few trailing ticks just so the beam doesn't end abruptly right before landing.
		this->beamStopTick = g_pKZUtils->GetServerGlobals()->tickcount + originHistorySize + 4;
	}

	if (!shouldStopBeam && this->canResumeBeam)
	{
		this->beamStopTick = 0;
	}

	if ((this->beamStopTick && g_pKZUtils->GetServerGlobals()->tickcount > this->beamStopTick)
		|| this->noclipTick && g_pKZUtils->GetServerGlobals()->tickcount > this->noclipTick || !this->desiredBeamType)
	{
		this->playerBeam.moving = false;
	}
	else
	{
		this->UpdatePlayerBeam();
	}
	BeamOrigin origin;
	this->target->GetOrigin(&origin);
	if (this->desiredBeamType == BEAM_GROUND)
	{
		origin.z = this->target->takeoffGroundOrigin.z;
	}
	origin.forceRecreate = this->teleportedThisTick;
	this->buffer.Write(origin);
	this->teleportedThisTick = false;
}

void KZBeamService::UpdatePlayerBeam()
{
	if (this->buffer.GetReadAvailable() < originHistorySize)
	{
		return;
	}
	if (!this->playerBeam.handle.Get())
	{
		this->playerBeam.handle = CreateGrenadeEnt(CS_TEAM_CT);
	}
	CBaseModelEntity *ent = static_cast<CBaseModelEntity *>(playerBeam.handle.Get());
	BeamOrigin origin;
	this->buffer.Peek(&origin, originHistorySize - 1);
	origin += this->playerBeamOffset;

	if (origin == playerBeam.lastOrigin)
	{
		playerBeam.moving = false;
	}
	else
	{
		if (!playerBeam.moving || origin.forceRecreate)
		{
			g_pKZUtils->RemoveEntity(playerBeam.handle.Get());
			playerBeam.handle = CreateGrenadeEnt(ent->m_iTeamNum())->GetRefEHandle();
			playerBeam.moving = true;
		}
		playerBeam.lastOrigin = origin;
		ent->Teleport(&origin, nullptr, &vec3_origin);
	}
}

void KZBeamService::Reset()
{
	this->playerBeam = {};
	this->playerBeamOffset = KZBeamService::defaultOffset;
	this->target = {};
	this->buffer.Clear();
	this->desiredBeamType = {};
	this->beamStopTick = 0;
	this->canResumeBeam = {};
	this->noclipTick = 0;
	this->teleportedThisTick = false;
	FOR_EACH_VEC(this->instantBeams, i)
	{
		KZ::beam::InstantBeam *beam = &this->instantBeams[i];
		if (beam->handle.Get())
		{
			g_pKZUtils->RemoveEntity(beam->handle.Get());
		}
	}
	this->instantBeams.RemoveAll();
}

void KZBeamService::UpdateBeams()
{
	static CConVarRef<float> sv_grenade_trajectory_prac_trailtime("sv_grenade_trajectory_prac_trailtime");
	if (sv_grenade_trajectory_prac_trailtime.Get() != 4.0f)
	{
		sv_grenade_trajectory_prac_trailtime.Set(4.0f);
	}
	for (i32 i = 0; i < MAXPLAYERS + 1; i++)
	{
		KZPlayer *player = g_pKZPlayerManager->ToPlayer(i);
		if (!player || !player->IsInGame())
		{
			continue;
		}
		player->beamService->Update();
		player->beamService->UpdateInstantBeams();
	}
}

void KZBeamService::AddInstantBeam(const Vector &start, const Vector &end, u32 lifetime)
{
	KZ::beam::InstantBeam *beam = this->instantBeams.AddToTailGetPtr();
	CBaseModelEntity *ent = CreateGrenadeEnt(CS_TEAM_T);
	beam->handle = ent->GetRefEHandle();
	beam->start = start;
	beam->end = end;
	beam->tickRemaining = lifetime;
	beam->totalTicks = lifetime;
	ent->Teleport(&start, nullptr, &vec3_origin);
}

void KZBeamService::UpdateInstantBeams()
{
	FOR_EACH_VEC(this->instantBeams, i)
	{
		KZ::beam::InstantBeam *beam = &this->instantBeams[i];
		if (!beam->handle.Get())
		{
			this->instantBeams.Remove(i);
			i--;
			continue;
		}

		if (beam->tickLingered > beam->maxLingerTicks)
		{
			g_pKZUtils->RemoveEntity(beam->handle.Get());
			this->instantBeams.Remove(i);
			i--;
			continue;
		}
		else if (beam->tickRemaining >= 0)
		{
			Vector current = Lerp((f32)beam->tickRemaining / (f32)beam->totalTicks, beam->end, beam->start);
			static_cast<CBaseEntity *>(beam->handle.Get())->Teleport(&current, nullptr, &vec3_origin);
			beam->tickRemaining--;
		}
		else
		{
			beam->tickLingered++;
		}
	}
}
