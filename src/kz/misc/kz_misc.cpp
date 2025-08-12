#include "src/common.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "kz/kz.h"
#include "utils/simplecmds.h"
#include "utils/detours.h"

#include "kz/checkpoint/kz_checkpoint.h"
#include "kz/jumpstats/kz_jumpstats.h"
#include "kz/quiet/kz_quiet.h"
#include "kz/mode/kz_mode.h"
#include "kz/language/kz_language.h"
#include "kz/style/kz_style.h"
#include "kz/mappingapi/kz_mappingapi.h"
#include "kz/noclip/kz_noclip.h"
#include "kz/hud/kz_hud.h"
#include "kz/option/kz_option.h"
#include "kz/spec/kz_spec.h"
#include "kz/goto/kz_goto.h"
#include "kz/telemetry/kz_telemetry.h"
#include "kz/timer/kz_timer.h"
#include "kz/tip/kz_tip.h"
#include "kz/global/kz_global.h"

#include "sdk/gamerules.h"
#include "sdk/physics/gamesystem.h"
#include "sdk/entity/cbasetrigger.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/tracefilter.h"

#define RESTART_CHECK_INTERVAL 1800.0f
static_global CTimer<> *mapRestartTimer;

static_global class KZOptionServiceEventListener_Misc : public KZOptionServiceEventListener
{
	virtual void OnPlayerPreferencesLoaded(KZPlayer *player)
	{
		bool hideLegs = player->optionService->GetPreferenceBool("hideLegs", false);
		if (player->HidingLegs() != hideLegs)
		{
			player->ToggleHideLegs();
		}
	}
} optionEventListener;

SCMD(kz_hidelegs, SCFL_PLAYER | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->ToggleHideLegs();
	if (player->HidingLegs())
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Player Legs - Disable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Player Legs - Enable");
	}
	return MRES_SUPERCEDE;
}

SCMD(kz_hide, SCFL_PLAYER | SCFL_PREFERENCE)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	player->quietService->ToggleHide();
	if (player->quietService->hideOtherPlayers)
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Players - Disable");
	}
	else
	{
		player->languageService->PrintChat(true, false, "Quiet Option - Show Players - Enable");
	}
	return MRES_SUPERCEDE;
}

SCMD(kz_end, SCFL_MAP)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	// If the player specify a course name, we first check if it's valid or not.
	if (V_strlen(args->ArgS()) > 0)
	{
		const KZCourseDescriptor *course = KZ::course::GetCourse(args->ArgS(), false);

		if (!course || !course || !course->hasEndPosition)
		{
			player->languageService->PrintChat(true, false, "No End Position For Course", args->ArgS());
			return MRES_SUPERCEDE;
		}
	}

	bool shouldTeleport = false;
	Vector tpOrigin;
	QAngle tpAngles;
	if (player->timerService->GetCourse())
	{
		if (player->timerService->GetCourse()->hasEndPosition)
		{
			tpOrigin = player->timerService->GetCourse()->endPosition;
			tpAngles = player->timerService->GetCourse()->endAngles;
			shouldTeleport = true;
		}
		else
		{
			CUtlString courseName = player->timerService->GetCourse()->GetName();
			player->languageService->PrintChat(true, false, "No End Position For Course", courseName.Get());
			return MRES_SUPERCEDE;
		}
	}

	// If we have no active course the map only has one course, !r should send the player to the end of that course.
	else if (KZ::course::GetCourseCount() == 1)
	{
		const KZCourseDescriptor *descriptor = KZ::course::GetFirstCourse();
		if (descriptor->hasEndPosition)
		{
			tpOrigin = descriptor->endPosition;
			tpAngles = descriptor->endAngles;
			shouldTeleport = true;
		}
		else
		{
			CUtlString courseName = KZ::course::GetFirstCourse()->GetName();
			player->languageService->PrintChat(true, false, "No End Position For Course", courseName.Get());
		}
	}

	if (shouldTeleport)
	{
		player->timerService->TimerStop();
		if (player->GetPlayerPawn()->IsAlive())
		{
			if (player->noclipService->IsNoclipping())
			{
				player->noclipService->DisableNoclip();
				player->noclipService->HandleNoclip();
			}
		}
		else
		{
			KZ::misc::JoinTeam(player, CS_TEAM_CT, false);
		}
		player->Teleport(&tpOrigin, &tpAngles, &vec3_origin);
	}
	return MRES_SUPERCEDE;
}

SCMD(kz_restart, SCFL_TIMER | SCFL_MAP)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	const KZCourseDescriptor *startPosCourse = nullptr;
	// If the player specify a course name, we first check if it's valid or not.
	if (V_strlen(args->ArgS()) > 0)
	{
		startPosCourse = KZ::course::GetCourse(args->ArgS(), false);

		if (!startPosCourse || !startPosCourse || !startPosCourse->hasStartPosition)
		{
			player->languageService->PrintChat(true, false, "No Start Position For Course", args->ArgS());
			return MRES_SUPERCEDE;
		}
	}

	player->timerService->OnTeleportToStart();
	if (player->GetPlayerPawn()->IsAlive())
	{
		// Fix players spawning 500u under spawn positions.
		if (player->noclipService->IsNoclipping())
		{
			player->noclipService->DisableNoclip();
			player->noclipService->HandleNoclip();
		}
		player->GetPlayerPawn()->Respawn();
	}
	else
	{
		KZ::misc::JoinTeam(player, CS_TEAM_CT, false);
	}

	if (player->quietService->ShouldAutoHideWeapon())
	{
		player->quietService->HideCurrentWeapon(true);
	}
	if (startPosCourse)
	{
		player->Teleport(&startPosCourse->startPosition, &startPosCourse->startAngles, &vec3_origin);
		return MRES_SUPERCEDE;
	}

	// Then prioritize custom start position.
	if (player->checkpointService->HasCustomStartPosition())
	{
		player->checkpointService->TpToStartPosition();
		return MRES_SUPERCEDE;
	}

	// If we have no custom start position, we try to get the start position of the current course.
	if (player->timerService->GetCourse())
	{
		if (player->timerService->GetCourse()->hasStartPosition)
		{
			player->Teleport(&player->timerService->GetCourse()->startPosition, &player->timerService->GetCourse()->startAngles, &vec3_origin);
		}
		return MRES_SUPERCEDE;
	}

	// If we have no active course the map only has one course, !r should send the player to that course.
	if (KZ::course::GetCourseCount() == 1)
	{
		const KZCourseDescriptor *descriptor = KZ::course::GetFirstCourse();
		if (descriptor && descriptor->hasStartPosition)
		{
			player->Teleport(&descriptor->startPosition, &descriptor->startAngles, &vec3_origin);
			return MRES_SUPERCEDE;
		}
	}

	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_r, kz_restart);

SCMD(kz_lj, SCFL_JUMPSTATS | SCFL_MAP)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);

	Vector destPos;
	QAngle destAngles;
	if (g_pMappingApi->GetJumpstatArea(destPos, destAngles))
	{
		player->timerService->TimerStop();
		player->Teleport(&destPos, &destAngles, &vec3_origin);
	}
	else
	{
		player->languageService->PrintChat(true, false, "No Jumpstat Area Found", args->ArgS());
	}

	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_ljarea, kz_lj);
SCMD_LINK(kz_jsarea, kz_lj);

SCMD(kz_playercheck, SCFL_PLAYER)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	KZPlayer *targetPlayer = nullptr;
	if (args->ArgC() < 2)
	{
		targetPlayer = player;
	}
	else
	{
		for (i32 i = 0; i <= MAXPLAYERS; i++)
		{
			CBasePlayerController *controller = g_pKZPlayerManager->players[i]->GetController();

			if (!controller)
			{
				continue;
			}

			if (V_strstr(V_strlower((char *)controller->GetPlayerName()), V_strlower((char *)args->ArgS())))
			{
				targetPlayer = g_pKZPlayerManager->ToPlayer(i);
				break;
			}
		}
	}
	if (!targetPlayer)
	{
		player->languageService->PrintChat(true, false, "Error Message (Player Not Found)", args->ArgS());
		return MRES_SUPERCEDE;
	}
	player->languageService->PrintChat(
		true, false, targetPlayer->IsAuthenticated() ? "Player Authenticated (Steam)" : "Player Not Authenticated (Steam)", targetPlayer->GetName());
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_pc, kz_playercheck);

SCMD(jointeam, SCFL_HIDDEN)
{
	KZ::misc::JoinTeam(g_pKZPlayerManager->ToPlayer(controller), atoi(args->Arg(1)), true);
	return MRES_SUPERCEDE;
}

SCMD(switchhands, SCFL_HIDDEN)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	return MRES_IGNORED;
}

SCMD_LINK(switchhandsleft, switchhands);
SCMD_LINK(switchhandsright, switchhands);

static_function f64 CheckRestart()
{
	utils::ResetMapIfEmpty();
	return RESTART_CHECK_INTERVAL;
}

void KZ::misc::Init()
{
	KZOptionService::RegisterEventListener(&optionEventListener);
	KZ::misc::EnforceTimeLimit();
	mapRestartTimer = StartTimer(CheckRestart, RESTART_CHECK_INTERVAL, true, true);
	CConVarRef<int32> sv_infinite_ammo("sv_infinite_ammo");
	if (sv_infinite_ammo.IsValidRef() && sv_infinite_ammo.IsConVarDataAvailable())
	{
		sv_infinite_ammo.RemoveFlags(FCVAR_CHEAT);
	}
	CConVarRef<CUtlString> bot_stop("bot_stop");
	if (bot_stop.IsValidRef() && bot_stop.IsConVarDataAvailable())
	{
		bot_stop.RemoveFlags(FCVAR_CHEAT);
	}
}

// TODO: Fullupdate spectators on spec_mode/spec_next/spec_player/spec_prev

void KZ::misc::JoinTeam(KZPlayer *player, int newTeam, bool restorePos)
{
	int currentTeam = player->GetController()->GetTeam();

	// Don't use CS_TEAM_NONE
	if (newTeam == CS_TEAM_NONE)
	{
		newTeam = CS_TEAM_SPECTATOR;
	}

	if (newTeam == CS_TEAM_SPECTATOR && currentTeam != CS_TEAM_SPECTATOR)
	{
		if (currentTeam != CS_TEAM_NONE)
		{
			player->specService->SavePosition();
		}

		if (!player->timerService->GetPaused() && !player->timerService->CanPause())
		{
			player->timerService->TimerStop();
		}
		player->GetController()->ChangeTeam(CS_TEAM_SPECTATOR);
		player->quietService->SendFullUpdate();
		// TODO: put spectators of this player to freecam, and send them full updates
	}
	else if (newTeam == CS_TEAM_CT && currentTeam != CS_TEAM_CT || newTeam == CS_TEAM_T && currentTeam != CS_TEAM_T)
	{
		player->GetPlayerPawn()->CommitSuicide(false, true);
		player->GetController()->SwitchTeam(newTeam);
		player->GetController()->Respawn();
		if (restorePos && player->specService->HasSavedPosition())
		{
			player->specService->LoadPosition();
		}
		else
		{
			player->timerService->TimerStop();
			// Just joining a team alone can put you into weird invalid spawns.
			// Need to teleport the player to a valid one.
			Vector spawnOrigin {};
			QAngle spawnAngles {};
			if (utils::FindValidSpawn(spawnOrigin, spawnAngles))
			{
				auto pawn = player->GetPlayerPawn();
				pawn->Teleport(&spawnOrigin, &spawnAngles, &vec3_origin);
				// CS2 bug: m_flWaterJumpTime is not properly initialized upon player spawn.
				// If the player is teleported on the very first tick of movement and lose the ground flag,
				// the player might get teleported to a random place.
				pawn->m_pWaterServices()->m_flWaterJumpTime(0.0f);
			}
		}
		player->specService->ResetSavedPosition();
	}

	if (player->quietService->ShouldAutoHideWeapon())
	{
		player->quietService->HideCurrentWeapon(true);
	}
	player->tipService->OnPlayerJoinTeam(newTeam);
}

static_function void SanitizeMsg(const char *input, char *output, u32 size)
{
	int x = 0;
	for (int i = 0; input[i] != '\0'; i++)
	{
		if (x + 1 == size)
		{
			break;
		}

		int character = input[i];

		if (character > 0x10)
		{
			if (character == '\\')
			{
				output[x++] = character;
			}
			output[x++] = character;
		}
	}

	output[x] = '\0';
}

void KZ::misc::ProcessConCommand(ConCommandRef cmd, const CCommandContext &ctx, const CCommand &args)
{
	if (!GameEntitySystem())
	{
		return;
	}
	CPlayerSlot slot = ctx.GetPlayerSlot();

	CCSPlayerController *controller = (CCSPlayerController *)utils::GetController(slot);

	KZPlayer *player = NULL;

	if (!cmd.IsValidRef() || !controller || !(player = g_pKZPlayerManager->ToPlayer(controller)))
	{
		return;
	}
	const char *p {};
	const char *commandName = cmd.GetName();

	// Is it a chat message?
	if (!V_stricmp(commandName, "say") || !V_stricmp(commandName, "say_team"))
	{
		if (args.ArgC() < 2)
		{
			// no argument, happens when the player just types "say" or "say_team" in console
			return;
		}
		// 3 cases:
		// say ""
		// say_team ""
		// say /<insert text>
		i32 argLen = strlen(args.ArgS());
		p = args.ArgS();
		bool wrappedInQuotes = false;
		if (args.ArgS()[0] == '"' && args.ArgS()[argLen - 1] == '"')
		{
			argLen -= 2;
			wrappedInQuotes = true;
			p += 1;
		}
		if (argLen < 1 || args[1][0] == SCMD_CHAT_SILENT_TRIGGER)
		{
			// arg is too short!
			return;
		}

		CUtlString message;
		message.SetDirect(p, strlen(p) - wrappedInQuotes);
		auto name = player->GetName();
		auto msg = message.Get();

		if (player->IsAlive())
		{
			utils::SayChat(player->GetController(), "{lime}%s{default}: %s", name, msg);
			utils::PrintConsoleAll("%s: %s", name, msg);
			META_CONPRINTF("%s: %s\n", name, msg);
		}
		else
		{
			utils::SayChat(player->GetController(), "{grey}* {lime}%s{default}: %s", name, msg);
			utils::PrintConsoleAll("* %s: %s", name, msg);
			META_CONPRINTF("* %s: %s\n", name, msg);
		}
	}

	return;
}

void KZ::misc::OnRoundStart()
{
	CCSGameRules *gameRules = g_pKZUtils->GetGameRules();
	if (gameRules)
	{
		gameRules->m_bGameRestart(true);
		gameRules->m_iRoundWinStatus(1);
		// Make sure that the round time is synchronized with the global time.
		gameRules->m_fRoundStartTime().SetTime(0.0f);
		gameRules->m_flGameStartTime().SetTime(0.0f);
	}
}

static_global bool clipsDrawn = false;
static_global bool triggersDrawn = false;

static_function void ResetOverlays()
{
	g_pKZUtils->ClearOverlays();
	clipsDrawn = false;
	triggersDrawn = false;
}

void OnDebugColorCvarChanged(CConVar<Color> *ref, CSplitScreenSlot nSlot, const Color *pNewValue, const Color *pOldValue)
{
	ResetOverlays();
}

// clang-format off
CConVar<bool> kz_showplayerclips("kz_showplayerclips", FCVAR_NONE, "Draw player clips (listen server only)", false,
	[](CConVar<bool> *ref, CSplitScreenSlot nSlot, const bool *bNewValue, const bool *bOldValue)
	{
		ResetOverlays();
	}
);

CConVar<bool> kz_showtriggers("kz_showtriggers", FCVAR_NONE, "Draw triggers (listen server only)", false,
	[](CConVar<bool> *ref, CSplitScreenSlot nSlot, const bool *bNewValue, const bool *bOldValue)
	{
		ResetOverlays();
	}
);

CConVar<Color> kz_playerclip_color("kz_playerclip_color", FCVAR_NONE, "Color of player clips (rgba) drawn by kz_toggleplayerclips.", Color(0x80, 0, 0x80, 0xFF), OnDebugColorCvarChanged);
CConVar<Color> kz_trigger_teleport_color("kz_trigger_teleport_color", FCVAR_NONE, "Color of trigger_teleport (rgba) drawn by kz_showtriggers.", Color(255, 0, 255, 0x80), OnDebugColorCvarChanged);
CConVar<Color> kz_trigger_multiple_colors[KZTRIGGER_COUNT] = 
{
	{"kz_trigger_multiple_color", FCVAR_NONE, "Color of trigger_multiple (rgba) drawn by kz_showtriggers.", Color(255, 199, 105, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_modifiers_color", FCVAR_NONE, "Color of Mapping API's Modifiers trigger (rgba) drawn by kz_showtriggers.", Color(255, 255, 128, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_reset_checkpoints_color", FCVAR_NONE, "Color of Mapping API's Reset Checkpoints trigger (rgba) drawn by kz_showtriggers.", Color(255, 140, 204, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_single_bhop_reset_color", FCVAR_NONE, "Color of Mapping API's Single bhop reset trigger (rgba) drawn by kz_showtriggers.", Color(140, 255, 212, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_anti_bhop_color", FCVAR_NONE, "Color of Mapping API's Anti Bhop trigger (rgba) drawn by kz_showtriggers.", Color(255, 64, 64, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_start_zone_color", FCVAR_NONE, "Color of Mapping API's Start Zone trigger (rgba) drawn by kz_showtriggers.", Color(0, 255, 0, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_end_zone_color", FCVAR_NONE, "Color of Mapping API's End Zone trigger (rgba) drawn by kz_showtriggers.", Color(255, 0, 0, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_split_zone_color", FCVAR_NONE, "Color of Mapping API's Split Zone trigger (rgba) drawn by kz_showtriggers.", Color(0, 255, 228, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_checkpoint_zone_color", FCVAR_NONE, "Color of Mapping API's Checkpoint Zone trigger (rgba) drawn by kz_showtriggers.", Color(219, 255, 0, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_stage_zone_color", FCVAR_NONE, "Color of Mapping API's Stage Zone trigger (rgba) drawn by kz_showtriggers.", Color(255, 157, 0, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_general_teleport_color", FCVAR_NONE, "Color of Mapping API's General Teleport trigger (rgba) drawn by kz_showtriggers.", Color(230, 117, 255, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_multi_bhop_color", FCVAR_NONE, "Color of Mapping API's Multi Bhop trigger (rgba) drawn by kz_showtriggers.", Color(79, 31, 255, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_single_bhop_color", FCVAR_NONE, "Color of Mapping API's Single Bhop trigger (rgba) drawn by kz_showtriggers.", Color(31, 107, 255, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_sequential_bhop_color", FCVAR_NONE, "Color of Mapping API's Sequential Bhop trigger (rgba) drawn by kz_showtriggers.", Color(196, 84, 214, 0x80), OnDebugColorCvarChanged},
	{"kz_trigger_mappingapi_push_color", FCVAR_NONE, "Color of Mapping API's Push trigger (rgba) drawn by kz_showtriggers.", Color(155, 255, 0, 0x80), OnDebugColorCvarChanged},
};

// clang-format on

static_function void DrawClipMeshes(CPhysicsGameSystem *gs)
{
	if (clipsDrawn)
	{
		return;
	}
	FOR_EACH_MAP(gs->m_PhysicsSpawnGroups, i)
	{
		CPhysicsGameSystem::PhysicsSpawnGroups_t &group = gs->m_PhysicsSpawnGroups[i];
		CPhysAggregateInstance *instance = group.m_pLevelAggregateInstance;
		CPhysAggregateData *aggregateData = instance ? instance->aggregateData : nullptr;
		if (!aggregateData)
		{
			META_CONPRINTF("PhysicsSpawnGroup %i: No aggregate data found for instance %p\n", i, instance);
			continue;
		}
		FOR_EACH_VEC(aggregateData->m_Parts, i)
		{
			const VPhysXBodyPart_t *part = aggregateData->m_Parts[i];
			if (!part)
			{
				continue;
			}
			clipsDrawn = true;
			FOR_EACH_VEC(part->m_rnShape.m_hulls, j)
			{
				const RnHullDesc_t &hull = part->m_rnShape.m_hulls[j];
				const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[hull.m_nCollisionAttributeIndex];
				if (collisionAttr.HasInteractsAsLayer(LAYER_INDEX_CONTENTS_PLAYER_CLIP))
				{
					CTransform transform;
					transform.SetToIdentity();
					Ray_t ray;
					ray.Init(hull.m_Hull.m_Bounds.m_vMinBounds, hull.m_Hull.m_Bounds.m_vMaxBounds, hull.m_Hull.m_VertexPositions.Base(),
							 hull.m_Hull.m_VertexPositions.Count());
					g_pKZUtils->DebugDrawMesh(transform, ray, kz_playerclip_color.Get().r(), kz_playerclip_color.Get().g(),
											  kz_playerclip_color.Get().b(), kz_playerclip_color.Get().a(), true, false, -1.0f);
				}
			}
			FOR_EACH_VEC(part->m_rnShape.m_meshes, j)
			{
				const RnMeshDesc_t &mesh = part->m_rnShape.m_meshes[j];
				const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[mesh.m_nCollisionAttributeIndex];
				if (collisionAttr.HasInteractsAsLayer(LAYER_INDEX_CONTENTS_PLAYER_CLIP))
				{
					CTransform transform;
					transform.SetToIdentity();
					FOR_EACH_VEC(mesh.m_Mesh.m_Triangles, k)
					{
						const RnTriangle_t &triangle = mesh.m_Mesh.m_Triangles[k];
						g_pKZUtils->AddTriangleOverlay(mesh.m_Mesh.m_Vertices[triangle.m_nIndex[0]], mesh.m_Mesh.m_Vertices[triangle.m_nIndex[1]],
													   mesh.m_Mesh.m_Vertices[triangle.m_nIndex[2]], kz_playerclip_color.Get().r(),
													   kz_playerclip_color.Get().g(), kz_playerclip_color.Get().b(), kz_playerclip_color.Get().a(),
													   false, -1.0f);
					}
				}
			}
		}
	}
}

static_function void DrawTriggers()
{
	if (triggersDrawn || !GameEntitySystem())
	{
		return;
	}
	EntityInstanceIter_t iter;
	for (CEntityInstance *pEnt = iter.First(); pEnt; pEnt = iter.Next())
	{
		if (V_strstr(pEnt->GetClassname(), "trigger_"))
		{
			CBaseTrigger *pTrigger = static_cast<CBaseTrigger *>(pEnt);
			CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(pTrigger->m_CBodyComponent()->m_pSceneNode());
			CPhysAggregateInstance *pPhysInstance = pSkeleton ? (CPhysAggregateInstance *)pSkeleton->m_modelState().m_pVPhysicsAggregate() : nullptr;
			const KzTrigger *kzTrigger = KZ::mapapi::GetKzTrigger(pTrigger);
			Color triggerColor;
			if (KZ_STREQI(pEnt->GetClassname(), "trigger_teleport"))
			{
				triggerColor = kz_trigger_teleport_color.Get();
			}
			else if (kzTrigger)
			{
				triggerColor = kz_trigger_multiple_colors[kzTrigger->type].Get();
			}
			else
			{
				triggerColor = kz_trigger_multiple_colors[0].Get();
			}
			auto *aggregateData = pPhysInstance ? pPhysInstance->aggregateData : nullptr;
			if (!aggregateData)
			{
				META_CONPRINTF("Trigger %i: No aggregate data found for instance %p\n", pTrigger->entindex(), pPhysInstance);
				continue;
			}
			FOR_EACH_VEC(aggregateData->m_Parts, i)
			{
				const VPhysXBodyPart_t *part = aggregateData->m_Parts[i];
				if (!part)
				{
					continue;
				}
				triggersDrawn = true;
				FOR_EACH_VEC(part->m_rnShape.m_hulls, j)
				{
					const RnHullDesc_t &hull = part->m_rnShape.m_hulls[j];
					const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[hull.m_nCollisionAttributeIndex];

					CTransform transform;
					transform.SetToIdentity();
					transform.m_vPosition = pTrigger->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
					transform.m_orientation = Quaternion(pTrigger->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
					Ray_t ray;
					ray.Init(hull.m_Hull.m_Bounds.m_vMinBounds, hull.m_Hull.m_Bounds.m_vMaxBounds, hull.m_Hull.m_VertexPositions.Base(),
							 hull.m_Hull.m_Vertices.Count());
					g_pKZUtils->DebugDrawMesh(transform, ray, triggerColor.r(), triggerColor.g(), triggerColor.b(), triggerColor.a(), true, false,
											  -1.0f);
				}
				FOR_EACH_VEC(part->m_rnShape.m_meshes, j)
				{
					const RnMeshDesc_t &mesh = part->m_rnShape.m_meshes[j];
					const RnCollisionAttr_t &collisionAttr = aggregateData->m_CollisionAttributes[mesh.m_nCollisionAttributeIndex];
					CTransform transform;
					transform.SetToIdentity();
					transform.m_vPosition = pTrigger->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
					transform.m_orientation = Quaternion(pTrigger->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation());
					FOR_EACH_VEC(mesh.m_Mesh.m_Triangles, k)
					{
						const RnTriangle_t &triangle = mesh.m_Mesh.m_Triangles[k];
						Vector tri[3] = {utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[0]]),
										 utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[1]]),
										 utils::TransformPoint(transform, mesh.m_Mesh.m_Vertices[triangle.m_nIndex[2]])};
						g_pKZUtils->AddTriangleOverlay(tri[0], tri[1], tri[2], triggerColor.r(), triggerColor.g(), triggerColor.b(), triggerColor.a(),
													   false, -1.0f);
					}
				}
			}
		}
	}
}

void KZ::misc::OnPhysicsGameSystemFrameBoundary(void *pThis)
{
	static_persist CPhysicsGameSystem *physicsGameSystem = nullptr;
	// Map probably reloaded, mark clips as not drawn.
	if (pThis != physicsGameSystem)
	{
		physicsGameSystem = (CPhysicsGameSystem *)pThis;
		g_pKZUtils->ClearOverlays();
		clipsDrawn = false;
	}
	if (kz_showtriggers.Get())
	{
		DrawTriggers();
	}
	if (kz_showplayerclips.Get())
	{
		DrawClipMeshes(physicsGameSystem);
	}
}

void KZ::misc::OnServerActivate()
{
	KZ::misc::EnforceTimeLimit();
	g_pKZUtils->UpdateCurrentMapMD5();

	interfaces::pEngine->ServerCommand("exec cs2kz.cfg");
	KZ::misc::InitTimeLimit();

	// Restart round to ensure settings (e.g. mp_weapons_allow_map_placed) are applied
	interfaces::pEngine->ServerCommand("mp_restartgame 1");
	kz_showplayerclips.Set(false);
	kz_showtriggers.Set(false);
}

static_function void ComputeSweptAABB(const Vector &boxMins, const Vector &boxMaxs, const Vector &delta, Vector &outMins, Vector &outMaxs)
{
	// AABB at end position
	Vector endMins = boxMins + delta;
	Vector endMaxs = boxMaxs + delta;

	// Swept AABB is the min/max of both boxes
	outMins.x = Min(boxMins.x, endMins.x);
	outMins.y = Min(boxMins.y, endMins.y);
	outMins.z = Min(boxMins.z, endMins.z);
	outMaxs.x = Max(boxMaxs.x, endMaxs.x);
	outMaxs.y = Max(boxMaxs.y, endMaxs.y);
	outMaxs.z = Max(boxMaxs.z, endMaxs.z);
}

static_function void FindTrianglesInBox(const RnNode_t *node, CUtlVector<uint32> &triangles, const Vector &mins, const Vector &maxs,
										const CTransform *transform = nullptr)
{
	Vector nodeMins = transform ? utils::TransformPoint(*transform, node->m_vMin) : node->m_vMin;
	Vector nodeMaxs = transform ? utils::TransformPoint(*transform, node->m_vMax) : node->m_vMax;
	if (!QuickBoxIntersectTest(nodeMins, nodeMaxs, mins, maxs))
	{
		return;
	}

	if (node->IsLeaf())
	{
		for (u32 i = 0; i < node->m_nChildrenOrTriangleCount; i++)
		{
			triangles.AddToTail(node->m_nTriangleOffset + i);
		}
	}
	else
	{
		FindTrianglesInBox(node->GetChild1(), triangles, mins, maxs, transform);
		FindTrianglesInBox(node->GetChild2(), triangles, mins, maxs, transform);
	}
}

CConVar<bool> kz_phys_debug_hull("kz_phys_debug_hull", FCVAR_NONE, "", true);
CConVar<bool> kz_phys_debug_mesh("kz_phys_debug_mesh", FCVAR_NONE, "", true);
CConVar<bool> kz_phys_debug_mesh_nodes("kz_phys_debug_mesh_nodes", FCVAR_NONE, "", false);
CConVar<bool> kz_phys_debug_mesh_nodes_trace_only("kz_phys_debug_mesh_nodes_trace_only", FCVAR_NONE, "", true);
CConVar<bool> kz_phys_debug_mesh_triangles("kz_phys_debug_mesh_triangles", FCVAR_NONE, "", false);

SCMD(kz_testground)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	Vector origin;
	player->GetOrigin(&origin);
	Ray_t ray;
	ray.Init(Vector(-16.0f, -16.0f, 0.0f), Vector(16.0f, 16.0f, 72.0f));
	Vector extent = vec3_origin;
	extent.z = -20.0f;
	CUtlVectorFixedGrowable<PhysicsTrace_t, 128> results;
	CTraceFilterPlayerMovementCS filter(player->GetPlayerPawn());
	filter.m_bIterateEntities = true;
	CastBox.EnableDetour();
	g_pKZUtils->CastBoxMultiple(&results, &ray, &origin, &extent, &filter);
	CastBox.DisableDetour();
	META_CONPRINTF("Done!\n");
	FOR_EACH_VEC(results, i)
	{
		PhysicsTrace_t &result = results[i];
		CTransform transform;
		META_CONPRINTF("Hit Point: (%.3f, %.3f, %.3f)\n", result.m_vHitPoint.x, result.m_vHitPoint.y, result.m_vHitPoint.z);
		META_CONPRINTF("Hit Normal: (%.3f, %.3f, %.3f)\n", result.m_vHitNormal.x, result.m_vHitNormal.y, result.m_vHitNormal.z);
		META_CONPRINTF("Hit Offset: %.3f\n", result.m_flHitOffset);
		META_CONPRINTF("Fraction: %.3f\n", result.m_flFraction);
		META_CONPRINTF("Triangle: %d\n", result.m_nTriangle);
		META_CONPRINTF("StartInSolid: %s\n", result.m_bStartInSolid ? "true" : "false");
		if (g_pKZUtils->GetPhysicsBodyTransform(result.m_HitObject, transform))
		{
			CBaseEntity *ent = g_pKZUtils->PhysicsBodyToEntity(result.m_HitObject);
			const char *name = ent->GetClassname();
			i32 index = ent->entindex();
			Vector origin = ent->m_CBodyComponent()->m_pSceneNode()->m_vecAbsOrigin();
			QAngle rotation = ent->m_CBodyComponent()->m_pSceneNode()->m_angAbsRotation();
			META_CONPRINTF("Entity %s (index %i), origin %f %f %f, rotation %f %f %f\n", name, index, origin.x, origin.y, origin.z, rotation.x,
						   rotation.y, rotation.z);
		}
		PhysicsShapeType_t shapeType = result.m_HitShape->GetShapeType();

		META_CONPRINTF("Hit shape #%i = %llx ", i, result.m_HitShape);
		switch (shapeType)
		{
			// Never seen a sphere ever.
			case SHAPE_SPHERE:
			{
				META_CONPRINTF("(sphere)\n");
				break;
			}
			// The only capsule in CS2 is the player hitbox (?) but that isn't going to matter for player movement.
			case SHAPE_CAPSULE:
			{
				META_CONPRINTF("(capsule)\n");
				break;
			}
			case SHAPE_HULL:
			{
				META_CONPRINTF("(hull)\n");
				if (!kz_phys_debug_hull.Get())
				{
					break;
				}
				RnHull_t *hull = result.m_HitShape->GetHull();

				Vector centroid = utils::TransformPoint(transform, hull->m_vCentroid);
				META_CONPRINTF("  m_vCentroid: (%f, %f, %f)\n", centroid.x, centroid.y, centroid.z);
				Vector minBounds = utils::TransformPoint(transform, hull->m_Bounds.m_vMinBounds);
				Vector maxBounds = utils::TransformPoint(transform, hull->m_Bounds.m_vMaxBounds);
				META_CONPRINTF("  m_Bounds: Min(%f, %f, %f) Max(%f, %f, %f)\n", minBounds.x, minBounds.y, minBounds.z, maxBounds.x, maxBounds.y,
							   maxBounds.z);

				META_CONPRINTF("  m_Vertices: count = %d\n", hull->m_Vertices.Count());
				FOR_EACH_VEC(hull->m_Vertices, i)
				{
					META_CONPRINTF("    [%d] m_nEdge: %u\n", i, hull->m_Vertices[i].m_nEdge);
				}

				META_CONPRINTF("  m_VertexPositions: count = %d\n", hull->m_VertexPositions.Count());
				FOR_EACH_VEC(hull->m_VertexPositions, i)
				{
					Vector v = utils::TransformPoint(transform, hull->m_VertexPositions[i]);
					META_CONPRINTF("    [%d] (%f, %f, %f)\n", i, v.x, v.y, v.z);
				}

				META_CONPRINTF("  m_Edges: count = %d\n", hull->m_Edges.Count());
				FOR_EACH_VEC(hull->m_Edges, i)
				{
					const RnHalfEdge_t &e = hull->m_Edges[i];
					META_CONPRINTF("    [%d] Next: %u, Twin: %u, Origin: %u, Face: %u\n", i, e.m_nNext, e.m_nTwin, e.m_nOrigin, e.m_nFace);
				}

				META_CONPRINTF("  m_Faces: count = %d\n", hull->m_Faces.Count());
				FOR_EACH_VEC(hull->m_Faces, i)
				{
					META_CONPRINTF("    [%d] m_nEdge: %u\n", i, hull->m_Faces[i].m_nEdge);
				}

				META_CONPRINTF("  m_FacePlanes: count = %d\n", hull->m_FacePlanes.Count());
				FOR_EACH_VEC(hull->m_FacePlanes, i)
				{
					const RnPlane_t &p = hull->m_FacePlanes[i];
					Vector normal = utils::TransformPoint(transform, p.m_vNormal);
					META_CONPRINTF("    [%d] Normal: (%f, %f, %f), Offset: %f\n", i, normal.x, normal.y, normal.z, p.m_flOffset);
				}

				META_CONPRINTF("  m_nFlags: 0x%08X\n", hull->m_nFlags);
				break;
			}
			case SHAPE_MESH:
			{
				META_CONPRINTF("(mesh, tri %i)", result.m_nTriangle);
				if (!kz_phys_debug_mesh.Get())
				{
					break;
				}
				RnMesh_t *mesh = result.m_HitShape->GetMesh();
				Vector vMin = utils::TransformPoint(transform, mesh->m_vMin);
				Vector vMax = utils::TransformPoint(transform, mesh->m_vMax);
				META_CONPRINTF("  m_vMin: (%f, %f, %f)\n", vMin.x, vMin.y, vMin.z);
				META_CONPRINTF("  m_vMax: (%f, %f, %f)\n", vMax.x, vMax.y, vMax.z);

				META_CONPRINTF("  m_Triangles: count = %d\n", mesh->m_Triangles.Count());

				if (mesh->m_Nodes.Count() > 0)
				{
					RnNode_t &node = mesh->m_Nodes[i];
					if (kz_phys_debug_mesh_nodes.Get())
					{
						node.PrintDebug();
					}
					if (kz_phys_debug_mesh_nodes_trace_only.Get())
					{
						CUtlVector<u32> triangles;
						Vector sweptMins, sweptMaxs;
						ComputeSweptAABB(origin + Vector(-16.0f, -16.0f, 0.0f), origin + Vector(16.0f, 16.0f, 72.0f), extent, sweptMins, sweptMaxs);
						FindTrianglesInBox(&node, triangles, sweptMins, sweptMaxs, &transform);
						META_CONPRINTF("  Number of relevant triangles: %i\n", triangles.Count());
						FOR_EACH_VEC(triangles, i)
						{
							const RnTriangle_t &triangle = mesh->m_Triangles[triangles[i]];
							META_CONPRINTF("    Triangle [%d]: indices = (%d, %d, %d)\n", triangles[i], triangle.m_nIndex[0], triangle.m_nIndex[1],
										   triangle.m_nIndex[2]);
							Vector v0 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[0]]);
							Vector v1 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[1]]);
							Vector v2 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[2]]);
							META_CONPRINTF("      v0: (%f, %f, %f)\n", v0.x, v0.y, v0.z);
							META_CONPRINTF("      v1: (%f, %f, %f)\n", v1.x, v1.y, v1.z);
							META_CONPRINTF("      v2: (%f, %f, %f)\n", v2.x, v2.y, v2.z);
						}
					}
				}
				if (kz_phys_debug_mesh_triangles.Get())
				{
					FOR_EACH_VEC(mesh->m_Triangles, i)
					{
						const RnTriangle_t &triangle = mesh->m_Triangles[i];
						META_CONPRINTF("    Triangle [%d]: indices = (%d, %d, %d)\n", i, triangle.m_nIndex[0], triangle.m_nIndex[1],
									   triangle.m_nIndex[2]);
						Vector v0 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[0]]);
						Vector v1 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[1]]);
						Vector v2 = utils::TransformPoint(transform, mesh->m_Vertices[triangle.m_nIndex[2]]);
						META_CONPRINTF("      v0: (%f, %f, %f)\n", v0.x, v0.y, v0.z);
						META_CONPRINTF("      v1: (%f, %f, %f)\n", v1.x, v1.y, v1.z);
						META_CONPRINTF("      v2: (%f, %f, %f)\n", v2.x, v2.y, v2.z);
					}
				}
				break;
			}
			default:
			{
				break;
			}
		}
	}
	return MRES_SUPERCEDE;
}
