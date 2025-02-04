#include "kz.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "mode/kz_mode.h"
// #include "mode/kz_mode_vnl.h" // Include the header for KZVanillaModeService
#include "language/kz_language.h"
#include "hud/kz_hud.h"
#include "jumpstats/kz_jumpstats.h"


//#include "mode/kz_mode_ckz.h" // Include this if using KZClassicModeService

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
	delete this->modeService;
	delete this->languageService;
	delete this->hudService;
	delete this->jumpstatsService;

	this->languageService = new KZLanguageService(this);
	this->hudService = new KZHUDService(this);
	this->jumpstatsService = new KZJumpstatsService(this);
	KZ::mode::InitModeService(this);
}

void KZPlayer::Reset()
{
	MovementPlayer::Reset();

	// Reset services that should not persist across player sessions.
	this->languageService->Reset();
	this->modeService->Reset();
	this->hudService->Reset();
	this->jumpstatsService->Reset();

	g_pKZModeManager->SwitchToMode(this, "CKZ", true, true);
}

void KZPlayer::OnPlayerActive()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	// Mode/Styles stuff must be here for convars to be properly replicated.
	g_pKZModeManager->SwitchToMode(this, "CKZ", true, true);
}

void KZPlayer::OnAuthorized()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnAuthorized();
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
	this->modeService->OnPhysicsSimulate();
	this->UpdatePlayerModelAlpha();
}

KZPlayer *KZPlayer::GetSpectatedPlayer()
{
    if (!this->IsAlive())
    {
        CCSPlayerController* controller = this->GetController();
        if (!controller)
        {
            return nullptr;
        }
        
        CBasePlayerPawn* observerPawn = controller->m_hObserverPawn();
        if (!observerPawn)
        {
            return nullptr;
        }
        
        CPlayer_ObserverServices* obsService = observerPawn->m_pObserverServices;
        if (!obsService || !obsService->m_hObserverTarget().IsValid())
        {
            return nullptr;
        }

        CCSPlayerPawn* pawn = this->GetPlayerPawn();
        CBasePlayerPawn* target = (CBasePlayerPawn*)obsService->m_hObserverTarget().Get();
        
        // If the player is spectating their own corpse, consider that as not spectating anyone
        return target == pawn ? nullptr : g_pKZPlayerManager->ToPlayer(target);
    }
    return nullptr;
}

void KZPlayer::OnPhysicsSimulatePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnPhysicsSimulatePost();
	this->modeService->OnPhysicsSimulatePost();
	if (this->GetSpectatedPlayer())
	{
		KZHUDService::DrawPanels(this->GetSpectatedPlayer(), this);
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
}

void KZPlayer::OnProcessUsercmdsPost(void *cmds, int numcmds)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnProcessUsercmdsPost(cmds, numcmds);
}

void KZPlayer::OnSetupMove(PlayerCommand *pc)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnSetupMove(pc);
}

void KZPlayer::OnSetupMovePost(PlayerCommand *pc)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnSetupMovePost(pc);
}

void KZPlayer::OnProcessMovement()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	MovementPlayer::OnProcessMovement();
	KZ::mode::ApplyModeSettings(this);

	this->DisableTurnbinds();
	this->jumpstatsService->OnProcessMovement();
	this->modeService->OnProcessMovement();
}

void KZPlayer::OnProcessMovementPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnProcessMovementPost();
	this->jumpstatsService->OnProcessMovementPost();
	MovementPlayer::OnProcessMovementPost();
}

void KZPlayer::OnPlayerMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPlayerMove();
}

void KZPlayer::OnPlayerMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPlayerMovePost();
}

void KZPlayer::OnCheckParameters()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckParameters();
}

void KZPlayer::OnCheckParametersPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckParametersPost();
}

void KZPlayer::OnCanMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanMove();
}

void KZPlayer::OnCanMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanMovePost();
}

void KZPlayer::OnFullWalkMove(bool &ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFullWalkMove(ground);
}

void KZPlayer::OnFullWalkMovePost(bool ground)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFullWalkMovePost(ground);
}

void KZPlayer::OnMoveInit()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnMoveInit();
}

void KZPlayer::OnMoveInitPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnMoveInitPost();
}

void KZPlayer::OnCheckWater()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckWater();
}

void KZPlayer::OnWaterMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWaterMove();
}

void KZPlayer::OnWaterMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWaterMovePost();
}

void KZPlayer::OnCheckWaterPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckWaterPost();
}

void KZPlayer::OnCheckVelocity(const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckVelocity(a3);
}

void KZPlayer::OnCheckVelocityPost(const char *a3)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckVelocityPost(a3);
}

void KZPlayer::OnDuck()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnDuck();
}

void KZPlayer::OnDuckPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnDuckPost();
}

void KZPlayer::OnCanUnduck()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanUnduck();
}

void KZPlayer::OnCanUnduckPost(bool &ret)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCanUnduckPost(ret);
}

void KZPlayer::OnLadderMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnLadderMove();
}

void KZPlayer::OnLadderMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnLadderMovePost();
}

void KZPlayer::OnCheckJumpButton()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckJumpButton();
}

void KZPlayer::OnCheckJumpButtonPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckJumpButtonPost();
}

void KZPlayer::OnJump()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnJump();
}

void KZPlayer::OnJumpPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnJumpPost();
}

void KZPlayer::OnAirMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnAirMove();
	this->jumpstatsService->OnAirMove();
}

void KZPlayer::OnAirMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnAirMovePost();
	this->jumpstatsService->OnAirMovePost();
}

void KZPlayer::OnFriction()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFriction();
}

void KZPlayer::OnFrictionPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFrictionPost();
}

void KZPlayer::OnWalkMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWalkMove();
}

void KZPlayer::OnWalkMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnWalkMovePost();
}

void KZPlayer::OnTryPlayerMove(Vector *pFirstDest, trace_t *pFirstTrace)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnTryPlayerMove(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMove();
}

void KZPlayer::OnTryPlayerMovePost(Vector *pFirstDest, trace_t *pFirstTrace)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnTryPlayerMovePost(pFirstDest, pFirstTrace);
	this->jumpstatsService->OnTryPlayerMovePost();
}

void KZPlayer::OnCategorizePosition(bool bStayOnGround)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCategorizePosition(bStayOnGround);
}

void KZPlayer::OnCategorizePositionPost(bool bStayOnGround)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCategorizePositionPost(bStayOnGround);
}

void KZPlayer::OnFinishGravity()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFinishGravity();
}

void KZPlayer::OnFinishGravityPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnFinishGravityPost();
}

void KZPlayer::OnCheckFalling()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckFalling();
}

void KZPlayer::OnCheckFallingPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnCheckFallingPost();
}

void KZPlayer::OnPostPlayerMove()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostPlayerMove();
}

void KZPlayer::OnPostPlayerMovePost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostPlayerMovePost();
}

void KZPlayer::OnPostThink()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostThink();
	MovementPlayer::OnPostThink();
}

void KZPlayer::OnPostThinkPost()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnPostThinkPost();
}

void KZPlayer::OnStartTouchGround()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnStartTouchGround();
	this->jumpstatsService->EndJump();
}

void KZPlayer::OnStopTouchGround()
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnStopTouchGround();
	this->jumpstatsService->AddJump();
}

void KZPlayer::OnChangeMoveType(MoveType_t oldMoveType)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->modeService->OnChangeMoveType(oldMoveType);
	this->jumpstatsService->OnChangeMoveType(oldMoveType);
}

void KZPlayer::OnTeleport(const Vector *origin, const QAngle *angles, const Vector *velocity)
{
	VPROF_BUDGET(__func__, "CS2KZ");
	this->lastTeleportTime = g_pKZUtils->GetServerGlobals()->curtime;
	this->modeService->OnTeleport(origin, angles, velocity);
	this->jumpstatsService->InvalidateJumpstats("Teleported");
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
}

void KZPlayer::PlayErrorSound()
{
	utils::PlaySoundToClient(this->GetPlayerSlot(), MV_SND_ERROR);
}

void KZPlayer::TouchTriggersAlongPath(const Vector &start, const Vector &end, const bbox_t &bounds)
{
	// Implementation if needed
}

void KZPlayer::UpdateTriggerTouchList()
{
	// Implementation if needed
}

void KZPlayer::OnChangeTeamPost(i32 team)
{
	// Implementation if needed
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

	return valueStr;
}
