#include "src/common.h"
#include "utils/utils.h"
#include "utils/ctimer.h"
#include "kz/kz.h"
#include "utils/simplecmds.h"

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
#include "kz/profile/kz_profile.h"
#include "kz/pistol/kz_pistol.h"

#include "sdk/gamerules.h"
#include "sdk/physicsgamesystem.h"
#include "sdk/entity/cbasetrigger.h"
#include "sdk/cskeletoninstance.h"

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
		if (player->GetPlayerPawn())
		{
			player->GetPlayerPawn()->CommitSuicide(false, true);
		}
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

	// The pistol service doesn't need to do anything if the player doesn't join a playing team.
	if (newTeam >= CS_TEAM_T)
	{
		player->pistolService->OnPlayerJoinTeam();
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

		std::string coloredPrefix = player->profileService->GetPrefix(true);
		std::string prefix = player->profileService->GetPrefix(false);
		if (player->IsAlive())
		{
			utils::SayChat(player->GetController(), "%s {lime}%s{default}: %s", coloredPrefix.c_str(), name, msg);
			utils::PrintConsoleAll("%s %s: %s", prefix.c_str(), name, msg);
			META_CONPRINTF("%s %s: %s\n", prefix.c_str(), name, msg);
		}
		else
		{
			utils::SayChat(player->GetController(), "{grey}* %s {lime}%s{default}: %s", coloredPrefix.c_str(), name, msg);
			utils::PrintConsoleAll("* %s %s: %s", prefix.c_str(), name, msg);
			META_CONPRINTF("* %s %s: %s\n", prefix.c_str(), name, msg);
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
