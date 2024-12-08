#include "kz.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "checkpoint/kz_checkpoint.h"
#include "db/kz_db.h"
#include "hud/kz_hud.h"
#include "jumpstats/kz_jumpstats.h"
#include "language/kz_language.h"
#include "mode/kz_mode.h"
#include "noclip/kz_noclip.h"
#include "option/kz_option.h"
#include "quiet/kz_quiet.h"
#include "spec/kz_spec.h"
#include "goto/kz_goto.h"
#include "style/kz_style.h"
#include "telemetry/kz_telemetry.h"
#include "timer/kz_timer.h"
#include "tip/kz_tip.h"
#include "trigger/kz_trigger.h"

#include "sdk/datatypes.h"
#include "sdk/entity/cbasetrigger.h"
#include "vprof.h"
#include "steam/isteamgameserver.h"
#include "tier0/memdbgon.h"

extern CSteamGameServerAPIContext g_steamAPI;
static_global ConVar *sv_standable_normal;
static_global ConVar *sv_walkable_normal;

void KZPlayer::Init()
{
	MovementPlayer::Init();
	this->hideLegs = false;

	// TODO: initialize every service.
	delete this->checkpointService;
	delete this->jumpstatsService;
	delete this->languageService;
	delete this->databaseService;
	delete this->quietService;
	delete this->hudService;
	delete this->specService;
	delete this->timerService;
	delete this->optionService;
	delete this->noclipService;
	delete this->tipService;
	delete this->telemetryService;
	delete this->triggerService;

	this->checkpointService = new KZCheckpointService(this);
	this->jumpstatsService = new KZJumpstatsService(this);
	this->databaseService = new KZDatabaseService(this);
	this->languageService = new KZLanguageService(this);
	this->noclipService = new KZNoclipService(this);
	this->quietService = new KZQuietService(this);
	this->hudService = new KZHUDService(this);
	this->specService = new KZSpecService(this);
	this->gotoService = new KZGotoService(this);
	this->timerService = new KZTimerService(this);
	this->optionService = new KZOptionService(this);
	this->tipService = new KZTipService(this);
	this->telemetryService = new KZTelemetryService(this);
	this->triggerService = new KZTriggerService(this);

	KZ::mode::InitModeService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	// Reset services that should not persist across player sessions.
	this->languageService->Reset();
	this->tipService->Reset();
	this->modeService->Reset();
	this->optionService->Reset();
	this->checkpointService->Reset();
	this->noclipService->Reset();
	this->quietService->Reset();
	this->jumpstatsService->Reset();
	this->hudService->Reset();
	this->timerService->Reset();
	this->specService->Reset();
	this->triggerService->Reset();

	g_pKZModeManager->SwitchToMode(this, KZOptionService::GetOptionStr("defaultMode", KZ_DEFAULT_MODE), true, true);
	g_pKZStyleManager->ClearStyles(this, true);
	CSplitString styles(KZOptionService::GetOptionStr("defaultStyles"), ",");
	FOR_EACH_VEC(styles, i)
	{
		g_pKZStyleManager->AddStyle(this, styles[i]);
	}
}

void KZPlayer::OnPlayerActive()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	// Mode/Styles stuff must be here for convars to be properly replicated.
	g_pKZModeManager->SwitchToMode(this, this->modeService->GetModeName(), true, true);
	g_pKZStyleManager->RefreshStyles(this);

	this->optionService->OnPlayerActive();
}

void KZPlayer::OnAuthorized()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnAuthorized();
	this->databaseService->SetupClient();
}

META_RES KZPlayer::GetPlayerMaxSpeed(f32 &maxSpeed)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	return this->modeService->GetPlayerMaxSpeed(maxSpeed);
}

void KZPlayer::OnPhysicsSimulate()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnPhysicsSimulate();
	this->triggerService->OnPhysicsSimulate();
	this->modeService->OnPhysicsSimulate();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPhysicsSimulate();
	}
	this->noclipService->HandleMoveCollision();
	this->EnableGodMode();
	this->UpdatePlayerModelAlpha();
}

void KZPlayer::OnPhysicsSimulatePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnPhysicsSimulatePost();
	this->triggerService->OnPhysicsSimulatePost();
	this->telemetryService->OnPhysicsSimulatePost();
	this->modeService->OnPhysicsSimulatePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPhysicsSimulatePost();
	}
	this->timerService->OnPhysicsSimulatePost();
	if (this->specService->GetSpectatedPlayer())
	{
		KZHUDService::DrawPanels(this->specService->GetSpectatedPlayer(), this);
	}
	else if (this->IsAlive())
	{
		KZHUDService::DrawPanels(this, this);
	}
}

void KZPlayer::OnProcessUsercmds(void *cmds, int numcmds)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnProcessUsercmds(cmds, numcmds);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessUsercmds(cmds, numcmds);
	}
}

void KZPlayer::OnProcessUsercmdsPost(void *cmds, int numcmds)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnProcessUsercmdsPost(cmds, numcmds);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessUsercmdsPost(cmds, numcmds);
	}
}

void KZPlayer::OnSetupMove(PlayerCommand *pc)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnSetupMove(pc);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMove(pc);
	}
}

void KZPlayer::OnSetupMovePost(PlayerCommand *pc)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnSetupMovePost(pc);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnSetupMovePost(pc);
	}
}

void KZPlayer::OnProcessMovement()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnProcessMovement();
	KZ::mode::ApplyModeSettings(this);

	this->DisableTurnbinds();
	this->modeService->OnProcessMovement();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessMovement();
	}

	this->triggerService->OnProcessMovement();
	this->jumpstatsService->OnProcessMovement();
	this->checkpointService->TpHoldPlayerStill();
}

void KZPlayer::OnProcessMovementPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->triggerService->OnProcessMovementPost();

	this->jumpstatsService->UpdateJump();
	this->modeService->OnProcessMovementPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnProcessMovementPost();
	}
	this->jumpstatsService->OnProcessMovementPost();
	MovementPlayer::OnProcessMovementPost();
}

void KZPlayer::OnPlayerMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPlayerMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPlayerMove();
	}
}

void KZPlayer::OnPlayerMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPlayerMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPlayerMovePost();
	}
}

void KZPlayer::OnCheckParameters()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckParameters();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckParameters();
	}
}

void KZPlayer::OnCheckParametersPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckParametersPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckParametersPost();
	}
}

void KZPlayer::OnCanMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanMove();
	}
}

void KZPlayer::OnCanMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanMovePost();
	}
}

void KZPlayer::OnFullWalkMove(bool &ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFullWalkMove(ground);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFullWalkMove(ground);
	}
}

void KZPlayer::OnFullWalkMovePost(bool ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFullWalkMovePost(ground);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFullWalkMovePost(ground);
	}
}

void KZPlayer::OnMoveInit()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnMoveInit();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnMoveInit();
	}
}

void KZPlayer::OnMoveInitPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnMoveInitPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnMoveInitPost();
	}
}

void KZPlayer::OnCheckWater()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckWater();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckWater();
	}
}

void KZPlayer::OnWaterMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWaterMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWaterMove();
	}
}

void KZPlayer::OnWaterMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWaterMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWaterMovePost();
	}
}

void KZPlayer::OnCheckWaterPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckWaterPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckWaterPost();
	}
}

void KZPlayer::OnCheckVelocity(const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckVelocity(a3);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckVelocity(a3);
	}
}

void KZPlayer::OnCheckVelocityPost(const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckVelocityPost(a3);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckVelocityPost(a3);
	}
}

void KZPlayer::OnDuck()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnDuck();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnDuck();
	}
}

void KZPlayer::OnDuckPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnDuckPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnDuckPost();
	}
}

void KZPlayer::OnCanUnduck()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanUnduck();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanUnduck();
	}
}

void KZPlayer::OnCanUnduckPost(bool &ret)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanUnduckPost(ret);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCanUnduckPost(ret);
	}
}

void KZPlayer::OnLadderMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnLadderMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnLadderMove();
	}
}

void KZPlayer::OnLadderMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnLadderMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnLadderMovePost();
	}
}

void KZPlayer::OnCheckJumpButton()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckJumpButton();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckJumpButton();
	}
}

void KZPlayer::OnCheckJumpButtonPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckJumpButtonPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckJumpButtonPost();
	}
}

void KZPlayer::OnJump()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnJump();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnJump();
	}
}

void KZPlayer::OnJumpPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnJumpPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnJumpPost();
	}
}

void KZPlayer::OnAirMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnAirMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirMove();
	}
	this->jumpstatsService->OnAirMove();
}

void KZPlayer::OnAirMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnAirMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnAirMovePost();
	}
	this->jumpstatsService->OnAirMovePost();
}

void KZPlayer::OnFriction()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFriction();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFriction();
	}
}

void KZPlayer::OnFrictionPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFrictionPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFrictionPost();
	}
}

void KZPlayer::OnWalkMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWalkMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWalkMove();
	}
}

void KZPlayer::OnWalkMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWalkMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnWalkMovePost();
	}
}

void KZPlayer::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnTryPlayerMove(pFirstDest, pFirstTrace);
	}
	this->jumpstatsService->OnTryPlayerMove();
}

void KZPlayer::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	}
	this->jumpstatsService->OnTryPlayerMovePost();
}

void KZPlayer::OnCategorizePosition(bool bStayOnGround)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCategorizePosition(bStayOnGround);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCategorizePosition(bStayOnGround);
	}
}

void KZPlayer::OnCategorizePositionPost(bool bStayOnGround)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCategorizePositionPost(bStayOnGround);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCategorizePositionPost(bStayOnGround);
	}
}

void KZPlayer::OnFinishGravity()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFinishGravity();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFinishGravity();
	}
}

void KZPlayer::OnFinishGravityPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFinishGravityPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnFinishGravityPost();
	}
}

void KZPlayer::OnCheckFalling()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckFalling();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckFalling();
	}
}

void KZPlayer::OnCheckFallingPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckFallingPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnCheckFallingPost();
	}
}

void KZPlayer::OnPostPlayerMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostPlayerMove();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostPlayerMove();
	}
}

void KZPlayer::OnPostPlayerMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostPlayerMovePost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostPlayerMovePost();
	}
}

void KZPlayer::OnPostThink()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostThink();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostThink();
	}
	MovementPlayer::OnPostThink();
}

void KZPlayer::OnPostThinkPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostThinkPost();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnPostThinkPost();
	}
}

void KZPlayer::OnStartTouchGround()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->jumpstatsService->EndJump();
	this->timerService->OnStartTouchGround();
	this->modeService->OnStartTouchGround();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnStartTouchGround();
	}
}

void KZPlayer::OnStopTouchGround()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->triggerService->OnStopTouchGround();
	this->timerService->OnStopTouchGround();
	this->modeService->OnStopTouchGround();
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnStopTouchGround();
	}
	this->jumpstatsService->AddJump();
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
	this->timerService->OnChangeMoveType(oldMoveType);
	this->modeService->OnChangeMoveType(oldMoveType);
	FOR_EACH_VEC(this->styleServices, i)
	{
		this->styleServices[i]->OnChangeMoveType(oldMoveType);
	}
}

void KZPlayer::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->lastTeleportTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->jumpstatsService->InvalidateJumpstats("Teleported");
	this->modeService->OnTeleport(origin, angles, velocity);
	this->timerService->OnTeleport(origin, angles, velocity);
}

void KZPlayer::DisableTurnbinds()
{
	CCSPlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}

	// holding both doesn't do anything. xor(1, 1) == 0
	bool usingTurnbinds = (this->IsButtonPressed(IN_TURNLEFT) ^ this->IsButtonPressed(IN_TURNRIGHT));
	QAngle angles;
	this->GetAngles(&angles);
	if (usingTurnbinds)
	{
		angles.y = this->lastValidYaw;
		// NOTE(GameChaos): Using SetAngles, which uses Teleport makes player movement really weird
		g_pKZUtils->SnapViewAngles(pawn, angles);
		if (!this->oldUsingTurnbinds)
		{
			this->languageService->PrintChat(true, false, "Turnbinds Disabled");
		}
	}
	else
	{
		this->lastValidYaw = angles.y;
	}
	this->oldUsingTurnbinds = usingTurnbinds;
}

void KZPlayer::EnableGodMode()
{
	CCSPlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	if (pawn->m_bTakesDamage())
	{
		pawn->m_bTakesDamage(false);
	}
}

void KZPlayer::UpdatePlayerModelAlpha()
{
	CCSPlayerPawn *pawn = this->GetPlayerPawn();
	if (!pawn)
	{
		return;
	}
	Color ogColor = pawn->m_clrRender();
	if (this->hideLegs && pawn->m_clrRender().a() == 255)
	{
		pawn->m_clrRender(Color(255, 255, 255, 254));
	}
	else if (!this->hideLegs && pawn->m_clrRender().a() != 255)
	{
		pawn->m_clrRender(Color(255, 255, 255, 255));
	}
}

bool KZPlayer::JustTeleported()
{
	return g_pKZUtils->GetServerGlobals()->curtime - this->lastTeleportTime < KZ_RECENT_TELEPORT_THRESHOLD;
}

void KZPlayer::ToggleHideLegs()
{
	this->hideLegs = !this->hideLegs;
	this->optionService->SetPreferenceBool("hideLegs", this->hideLegs);
}

void KZPlayer::PlayErrorSound()
{
	utils::PlaySoundToClient(this->GetPlayerSlot(), MV_SND_ERROR);
}

void KZPlayer::TouchTriggersAlongPath(const Vector &start, const Vector &end, const bbox_t &bounds)
{
	this->triggerService->TouchTriggersAlongPath(start, end, bounds);
}

void KZPlayer::UpdateTriggerTouchList()
{
	this->triggerService->UpdateTriggerTouchList();
}

void KZPlayer::OnChangeTeamPost(i32 team)
{
	this->timerService->OnPlayerJoinTeam(team);
}

CUtlString KZPlayer::ComputeCvarValueFromModeStyles(const char *name)
{
	ConVarHandle cvarHandle = g_pCVar->FindConVar(name);
	if (!cvarHandle.IsValid())
	{
		assert(0);
		META_CONPRINTF("Failed to find %s!\n", name);
		return "";
	}

	if (!name)
	{
		assert(0);
		return "";
	}

	ConVar *cvar = g_pCVar->GetConVar(cvarHandle);
	assert(cvar);
	CVValue_t *cvarValue = cvar->m_cvvDefaultValue;
	CUtlString valueStr;

	switch (cvar->m_eVarType)
	{
		case EConVarType_Bool:
		{
			valueStr.Format("%s", cvarValue->m_bValue ? "true" : "false");
			break;
		}
		case EConVarType_Int16:
		{
			valueStr.Format("%i", cvarValue->m_i16Value);
			break;
		}
		case EConVarType_Int32:
		{
			valueStr.Format("%i", cvarValue->m_i32Value);
			break;
		}
		case EConVarType_Int64:
		{
			valueStr.Format("%lli", cvarValue->m_i64Value);
			break;
		}
		case EConVarType_UInt16:
		{
			valueStr.Format("%u", cvarValue->m_u16Value);
			break;
		}
		case EConVarType_UInt32:
		{
			valueStr.Format("%u", cvarValue->m_u32Value);
			break;
		}
		case EConVarType_UInt64:
		{
			valueStr.Format("%llu", cvarValue->m_u64Value);
			break;
		}

		case EConVarType_Float32:
		{
			valueStr.Format("%f", cvarValue->m_flValue);
			break;
		}

		case EConVarType_Float64:
		{
			valueStr.Format("%lf", cvarValue->m_dbValue);
			break;
		}

		case EConVarType_String:
		{
			valueStr.Format("%s", cvarValue->m_szValue);
			break;
		}

		default:
			assert(0);
			break;
	}

	for (int i = 0; i < MODECVAR_COUNT; i++)
	{
		if (!V_stricmp(KZ::mode::modeCvarNames[i], name))
		{
			valueStr.Format("%s", this->modeService->GetModeConVarValues()[i]);
			break;
		}
	}

	FOR_EACH_VEC(this->styleServices, i)
	{
		if (this->styleServices[i]->GetTweakedConvarValue(name))
		{
			valueStr.Format("%s", this->styleServices[i]->GetTweakedConvarValue(name));
		}
	}
	return valueStr;
}
