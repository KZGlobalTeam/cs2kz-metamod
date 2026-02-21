/*
	Keeps track of course descriptors along with various types of triggers, applying effects to player when necessary.
*/

#include "kz/kz.h"
#include "kz/mode/kz_mode.h"
#include "kz/trigger/kz_trigger.h"
#include "kz/global/kz_global.h"
#include "movement/movement.h"
#include "kz_mappingapi.h"
#include "entity2/entitykeyvalues.h"
#include "sdk/entity/cbasetrigger.h"
#include "utils/ctimer.h"
#include "kz/db/kz_db.h"
#include "kz/language/kz_language.h"
#include "utils/simplecmds.h"
#include "utils/tables.h"
#include "UtlSortVector.h"

#include "tier0/memdbgon.h"

#define KEY_TRIGGER_TYPE         "timer_trigger_type"
#define KEY_IS_COURSE_DESCRIPTOR "timer_course_descriptor"

using namespace KZ::course;

class CourseLessFunc
{
public:
	bool Less(const KZCourseDescriptor *src1, const KZCourseDescriptor *src2, void *pCtx)
	{
		return src1->id < src2->id;
	}
};

enum
{
	MAPI_ERR_TOO_MANY_TRIGGERS = 1 << 0,
	MAPI_ERR_TOO_MANY_COURSES = 1 << 1,
};

static_global struct
{
	CUtlVectorFixed<KZCourseDescriptor, KZ_MAX_COURSE_COUNT> courseDescriptors;
	i32 mapApiVersion;
	bool apiVersionLoaded;
	bool fatalFailure;

	CUtlVectorFixed<KzTrigger, 2048> triggers;
	bool roundIsStarting;
	i32 errorFlags;
	i32 errorCount;
	char errors[32][256];

	bool hasJumpstatArea;
	Vector jumpstatAreaPos;
	QAngle jumpstatAreaAngles;
} g_mappingApi;

static_global CTimer<> *g_errorTimer;
static_global const char *g_errorPrefix = "{darkred} ERROR: ";
static_global const char *g_triggerNames[] = {"Disabled",   "Modifier",   "Reset Checkpoints", "Single Bhop Reset", "Antibhop",
											  "Start zone", "End zone",   "Split zone",        "Checkpoint zone",   "Stage zone",
											  "Teleport",   "Multi bhop", "Single bhop",       "Sequential bhop"};

static_function MappingInterface g_mappingInterface;

MappingInterface *g_pMappingApi = &g_mappingInterface;
static_global CUtlSortVector<KZCourseDescriptor *, CourseLessFunc> g_sortedCourses(KZ_MAX_COURSE_COUNT, KZ_MAX_COURSE_COUNT);

// TODO: add error check to make sure a course has at least 1 start zone and 1 end zone

static_function void Mapi_Error(const char *format, ...)
{
	i32 errorIndex = g_mappingApi.errorCount;
	if (errorIndex >= KZ_ARRAYSIZE(g_mappingApi.errors))
	{
		return;
	}
	else if (errorIndex == KZ_ARRAYSIZE(g_mappingApi.errors) - 1)
	{
		snprintf(g_mappingApi.errors[errorIndex], sizeof(g_mappingApi.errors[errorIndex]), "Too many errors to list!");
		return;
	}

	va_list args;
	va_start(args, format);
	vsnprintf(g_mappingApi.errors[errorIndex], sizeof(g_mappingApi.errors[errorIndex]), format, args);
	va_end(args);

	g_mappingApi.errorCount++;
}

static_function f64 Mapi_PrintErrors()
{
	if (g_mappingApi.errorFlags & MAPI_ERR_TOO_MANY_TRIGGERS)
	{
		utils::CPrintChatAll("%sToo many Mapping API triggers! Maximum is %i!", g_errorPrefix, g_mappingApi.triggers.Count());
	}
	if (g_mappingApi.errorFlags & MAPI_ERR_TOO_MANY_COURSES)
	{
		utils::CPrintChatAll("%sToo many Courses! Maximum is %i!", g_errorPrefix, g_mappingApi.courseDescriptors.Count());
	}
	for (i32 i = 0; i < g_mappingApi.errorCount; i++)
	{
		utils::CPrintChatAll("%s%s", g_errorPrefix, g_mappingApi.errors[i]);
	}

	return 60.0;
}

static_function bool Mapi_CreateCourse(i32 courseNumber = 1, const char *courseName = KZ_NO_MAPAPI_COURSE_NAME, i32 hammerId = -1,
									   const char *targetName = KZ_NO_MAPAPI_COURSE_DESCRIPTOR, bool disableCheckpoints = false)
{
	// Make sure we don't exceed this ridiculous value.
	// If we do, it is most likely that something went wrong, or it is caused by the mapper.
	if (g_mappingApi.courseDescriptors.Count() >= KZ_MAX_COURSE_COUNT)
	{
		assert(0);
		Mapi_Error("Failed to register course name '%s' (hammerId %i): Too many courses!", courseName, hammerId);
		return false;
	}

	auto &currentCourses = g_mappingApi.courseDescriptors;
	FOR_EACH_VEC(currentCourses, i)
	{
		if (currentCourses[i].hammerId == hammerId)
		{
			// This should only happen during start/end zone backwards compat where hammer IDs are KZ_NO_MAPAPI_VERSION, so this is not an error.
			return false;
		}
		if (KZ_STREQI(targetName, currentCourses[i].entityTargetname))
		{
			Mapi_Error("Course descriptor '%s' already existed! (registered by Hammer ID %i)", targetName, currentCourses[i].hammerId);
			return false;
		}
	}

	i32 index = g_mappingApi.courseDescriptors.AddToTail(
		{hammerId, targetName, disableCheckpoints, (u32)g_mappingApi.courseDescriptors.Count() + 1, courseNumber, courseName});
	g_sortedCourses.Insert(&g_mappingApi.courseDescriptors[index]);
	return true;
}

// Example keyvalues:
/*
	timer_anti_bhop_time: 0.2
	timer_teleport_relative: true
	timer_teleport_reorient_player: false
	timer_teleport_reset_speed: false
	timer_teleport_use_dest_angles: false
	timer_teleport_delay: 0
	timer_teleport_destination: landmark_teleport
	timer_zone_stage_number: 1
	timer_modifier_enable_slide: false
	timer_modifier_disable_jumpstats: false
	timer_modifier_disable_teleports: false
	timer_modifier_disable_checkpoints: false
	timer_modifier_disable_pause: false
	timer_trigger_type: 10
	wait: 1
	spawnflags: 4097
	StartDisabled: false
	useLocalOffset: false
	classname: trigger_multiple
	origin: 1792.000000 768.000000 -416.000000
	angles: 0.000000 0.000000 0.000000
	scales: 1.000000 1.000000 1.000000
	hammerUniqueId: 48
	model: maps\kz_mapping_api\entities\unnamed_48.vmdl
*/
static_function void Mapi_OnTriggerMultipleSpawn(const EntitySpawnInfo_t *info)
{
	const CEntityKeyValues *ekv = info->m_pKeyValues;
	i32 hammerId = ekv->GetInt("hammerUniqueId", -1);
	Vector origin = ekv->GetVector("origin");

	KzTriggerType type = (KzTriggerType)ekv->GetInt(KEY_TRIGGER_TYPE, KZTRIGGER_DISABLED);

	if (!g_mappingApi.roundIsStarting)
	{
		// Only allow triggers and zones that were spawned during the round start phase.
		return;
	}

	if (type < KZTRIGGER_DISABLED || type >= KZTRIGGER_COUNT)
	{
		assert(0);
		Mapi_Error("Trigger type %i is invalid and out of range (%i-%i) for trigger with Hammer ID %i, origin (%.0f %.0f %.0f)!", type,
				   KZTRIGGER_DISABLED, KZTRIGGER_COUNT - 1, hammerId, origin.x, origin.y, origin.z);
		return;
	}

	KzTrigger trigger = {};
	trigger.type = type;
	trigger.hammerId = hammerId;
	trigger.entity = info->m_pEntity->GetRefEHandle();

	switch (type)
	{
		case KZTRIGGER_MODIFIER:
		{
			trigger.modifier.disablePausing = ekv->GetBool("timer_modifier_disable_pause");
			trigger.modifier.disableCheckpoints = ekv->GetBool("timer_modifier_disable_checkpoints");
			trigger.modifier.disableTeleports = ekv->GetBool("timer_modifier_disable_teleports");
			trigger.modifier.disableJumpstats = ekv->GetBool("timer_modifier_disable_jumpstats");
			trigger.modifier.enableSlide = ekv->GetBool("timer_modifier_enable_slide");
			trigger.modifier.gravity = ekv->GetFloat("timer_modifier_gravity", 1);
			trigger.modifier.jumpFactor = ekv->GetFloat("timer_modifier_jump_impulse", 1.0f);
			trigger.modifier.forceDuck = ekv->GetBool("timer_modifier_force_duck");
			trigger.modifier.forceUnduck = ekv->GetBool("timer_modifier_force_unduck");
		}
		break;

		// NOTE: Nothing to do here
		case KZTRIGGER_RESET_CHECKPOINTS:
		case KZTRIGGER_SINGLE_BHOP_RESET:
			break;

		case KZTRIGGER_ANTI_BHOP:
		{
			trigger.antibhop.time = ekv->GetFloat("timer_anti_bhop_time");
			trigger.antibhop.time = MAX(trigger.antibhop.time, 0);
		}
		break;

		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		case KZTRIGGER_ZONE_STAGE:
		{
			const char *courseDescriptor = ekv->GetString("timer_zone_course_descriptor");

			if (!courseDescriptor || !courseDescriptor[0])
			{
				Mapi_Error("Course descriptor targetname of %s trigger is empty! Hammer ID %i, origin (%.0f %.0f %.0f)", g_triggerNames[type],
						   hammerId, origin.x, origin.y, origin.z);
				assert(0);
				return;
			}

			snprintf(trigger.zone.courseDescriptor, sizeof(trigger.zone.courseDescriptor), "%s", courseDescriptor);
			// TODO: code is a little repetitive...
			if (type == KZTRIGGER_ZONE_SPLIT)
			{
				trigger.zone.number = ekv->GetInt("timer_zone_split_number", INVALID_SPLIT_NUMBER);
				if (trigger.zone.number <= INVALID_SPLIT_NUMBER)
				{
					Mapi_Error("Split zone number \"%i\" is invalid! Hammer ID %i, origin (%.0f %.0f %.0f)", trigger.zone.number, hammerId, origin.x,
							   origin.y, origin.z);
					assert(0);
					return;
				}
			}
			else if (type == KZTRIGGER_ZONE_CHECKPOINT)
			{
				trigger.zone.number = ekv->GetInt("timer_zone_checkpoint_number", INVALID_CHECKPOINT_NUMBER);

				if (trigger.zone.number <= INVALID_CHECKPOINT_NUMBER)
				{
					Mapi_Error("Checkpoint zone number \"%i\" is invalid! Hammer ID %i, origin (%.0f %.0f %.0f)", trigger.zone.number, hammerId,
							   origin.x, origin.y, origin.z);
					assert(0);
					return;
				}
			}
			else if (type == KZTRIGGER_ZONE_STAGE)
			{
				trigger.zone.number = ekv->GetInt("timer_zone_stage_number", INVALID_STAGE_NUMBER);

				if (trigger.zone.number <= INVALID_STAGE_NUMBER)
				{
					Mapi_Error("Stage zone number \"%i\" is invalid! Hammer ID %i, origin (%.0f %.0f %.0f)", trigger.zone.number, hammerId, origin.x,
							   origin.y, origin.z);
					assert(0);
					return;
				}
			}
			else // Start/End zones
			{
				// Note: Triggers shouldn't be rotated most of the time anyway. If that ever happens for timer triggers, it's probably unintentional.
				QAngle angles = ekv->GetQAngle("angles");

				if (angles != vec3_angle)
				{
					Mapi_Error(
						"Warning: Unexpected rotation for timer trigger, some functionalities might not work properly! Hammer ID %i, origin (%.0f "
						"%.0f %.0f)",
						hammerId, origin.x, origin.y, origin.z);
				}
			}
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			const char *destination = ekv->GetString("timer_teleport_destination");
			V_snprintf(trigger.teleport.destination, sizeof(trigger.teleport.destination), "%s", destination);
			trigger.teleport.delay = ekv->GetFloat("timer_teleport_delay", 0);
			trigger.teleport.delay = MAX(trigger.teleport.delay, 0);
			if (KZ::mapapi::IsBhopTrigger(type))
			{
				trigger.teleport.delay = MAX(trigger.teleport.delay, 0.1);
			}
			trigger.teleport.useDestinationAngles = ekv->GetBool("timer_teleport_use_dest_angles");
			trigger.teleport.resetSpeed = ekv->GetBool("timer_teleport_reset_speed");
			trigger.teleport.reorientPlayer = ekv->GetBool("timer_teleport_reorient_player");
			trigger.teleport.relative = ekv->GetBool("timer_teleport_relative");
		}
		break;
		case KZTRIGGER_PUSH:
		{
			Vector impulse = ekv->GetVector("timer_push_amount");
			trigger.push.impulse[0] = impulse.x;
			trigger.push.impulse[1] = impulse.y;
			trigger.push.impulse[2] = impulse.z;
			trigger.push.setSpeed[0] = ekv->GetBool("timer_push_abs_speed_x");
			trigger.push.setSpeed[1] = ekv->GetBool("timer_push_abs_speed_y");
			trigger.push.setSpeed[2] = ekv->GetBool("timer_push_abs_speed_z");
			trigger.push.cancelOnTeleport = ekv->GetBool("timer_push_cancel_on_teleport");
			trigger.push.cooldown = ekv->GetFloat("timer_push_cooldown", 0.1f);
			trigger.push.delay = ekv->GetFloat("timer_push_delay", 0.0f);

			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_start_touch") ? KzMapPush::KZ_PUSH_START_TOUCH : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_touch") ? KzMapPush::KZ_PUSH_TOUCH : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_end_touch") ? KzMapPush::KZ_PUSH_END_TOUCH : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_jump_event") ? KzMapPush::KZ_PUSH_JUMP_EVENT : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_jump_button") ? KzMapPush::KZ_PUSH_JUMP_BUTTON : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_attack") ? KzMapPush::KZ_PUSH_ATTACK : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_attack2") ? KzMapPush::KZ_PUSH_ATTACK2 : 0;
			trigger.push.pushConditions |= ekv->GetBool("timer_push_condition_use") ? KzMapPush::KZ_PUSH_USE : 0;
		}
		break;
		case KZTRIGGER_DISABLED:
		{
			// Check for pre-mapping api triggers for backwards compatibility.
			if (g_mappingApi.mapApiVersion == KZ_NO_MAPAPI_VERSION)
			{
				if (info->m_pEntity->NameMatches("timer_startzone") || info->m_pEntity->NameMatches("timer_endzone"))
				{
					snprintf(trigger.zone.courseDescriptor, sizeof(trigger.zone.courseDescriptor), KZ_NO_MAPAPI_COURSE_DESCRIPTOR);
					trigger.type = info->m_pEntity->NameMatches("timer_startzone") ? KZTRIGGER_ZONE_START : KZTRIGGER_ZONE_END;
				}
			}
			break;
			// Otherwise these are just regular trigger_multiple.
		}
		default:
		{
			// technically impossible to happen, leave an assert here anyway for debug builds.
			assert(0);
			return;
		}
		break;
	}

	g_mappingApi.triggers.AddToTail(trigger);
}

static_function void Mapi_OnInfoTargetSpawn(const CEntityKeyValues *ekv)
{
	if (!ekv->GetBool(KEY_IS_COURSE_DESCRIPTOR))
	{
		return;
	}

	i32 hammerId = ekv->GetInt("hammerUniqueId", -1);
	Vector origin = ekv->GetVector("origin");

	i32 courseNumber = ekv->GetInt("timer_course_number", INVALID_COURSE_NUMBER);
	const char *courseName = ekv->GetString("timer_course_name");
	const char *targetName = ekv->GetString("targetname");
	constexpr static_persist const char *targetNamePrefix = "[PR#]";
	if (KZ_STREQLEN(targetName, targetNamePrefix, strlen(targetNamePrefix)))
	{
		targetName = targetName + strlen(targetNamePrefix);
	}

	if (courseNumber <= INVALID_COURSE_NUMBER)
	{
		Mapi_Error("Course number must be bigger than %i! Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)", INVALID_COURSE_NUMBER, hammerId,
				   origin.x, origin.y, origin.z);
		return;
	}

	if (!courseName[0])
	{
		Mapi_Error("Course name is empty! Course number %i. Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)", courseNumber, hammerId,
				   origin.x, origin.y, origin.z);
		return;
	}

	if (!targetName[0])
	{
		Mapi_Error("Course targetname is empty! Course name \"%s\". Course number %i. Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)",
				   courseName, courseNumber, hammerId, origin.x, origin.y, origin.z);
		return;
	}

	Mapi_CreateCourse(courseNumber, courseName, hammerId, targetName, ekv->GetBool("timer_course_disable_checkpoint"));
}

static_function KzTrigger *Mapi_FindKzTrigger(CBaseTrigger *trigger)
{
	if (!trigger->m_pEntity)
	{
		return nullptr;
	}

	CEntityHandle triggerHandle = trigger->GetRefEHandle();
	if (!trigger || !triggerHandle.IsValid() || trigger->m_pEntity->m_flags & EF_IS_INVALID_EHANDLE)
	{
		return nullptr;
	}

	FOR_EACH_VEC(g_mappingApi.triggers, i)
	{
		if (triggerHandle == g_mappingApi.triggers[i].entity)
		{
			return &g_mappingApi.triggers[i];
		}
	}

	return nullptr;
}

static_function KZCourseDescriptor *Mapi_FindCourse(const char *targetname)
{
	KZCourseDescriptor *result = nullptr;
	if (!targetname)
	{
		return result;
	}

	FOR_EACH_VEC(g_mappingApi.courseDescriptors, i)
	{
		if (KZ_STREQI(g_mappingApi.courseDescriptors[i].entityTargetname, targetname))
		{
			result = &g_mappingApi.courseDescriptors[i];
			break;
		}
	}

	return result;
}

static_function bool Mapi_SetStartPosition(const char *descriptorName, Vector origin, QAngle angles)
{
	KZCourseDescriptor *desc = Mapi_FindCourse(descriptorName);

	if (!desc)
	{
		return false;
	}
	desc->SetStartPosition(origin, angles);
	return true;
}

static_function void Mapi_OnInfoTeleportDestinationSpawn(const EntitySpawnInfo_t *info)
{
	const CEntityKeyValues *ekv = info->m_pKeyValues;
	const char *targetname = ekv->GetString("targetname");
	if (KZ_STREQI(targetname, "timer_start"))
	{
		if (g_mappingApi.mapApiVersion == KZ_NO_MAPAPI_VERSION)
		{
			Mapi_SetStartPosition(KZ_NO_MAPAPI_COURSE_DESCRIPTOR, ekv->GetVector("origin"), ekv->GetQAngle("angles"));
		}
		else if (g_mappingApi.mapApiVersion == KZ_MAPAPI_VERSION) // TODO do better check
		{
			const char *courseDescriptor = ekv->GetString("timer_zone_course_descriptor");
			i32 hammerId = ekv->GetInt("hammerUniqueId", -1);
			Vector origin = ekv->GetVector("origin");
			if (!courseDescriptor || !courseDescriptor[0])
			{
				Mapi_Error("Course descriptor targetname of timer_start info_teleport_destination is empty! Hammer ID %i, origin (%.0f %.0f %.0f)",
						   hammerId, origin.x, origin.y, origin.z);
				assert(0);
				return;
			}
			Mapi_SetStartPosition(courseDescriptor, origin, ekv->GetQAngle("angles"));
		}
	}
	else if (KZ_STREQI(targetname, "timer_jumpstat_area"))
	{
		g_mappingApi.hasJumpstatArea = true;
		g_mappingApi.jumpstatAreaPos = ekv->GetVector("origin");
		g_mappingApi.jumpstatAreaAngles = ekv->GetQAngle("angles");
	}
};

void KZ::mapapi::Init()
{
	g_mappingApi = {};

	g_errorTimer = g_errorTimer ? g_errorTimer : StartTimer(Mapi_PrintErrors, true);
}

void KZ::mapapi::OnCreateLoadingSpawnGroupHook(const CUtlVector<const CEntityKeyValues *> *pKeyValues)
{
	if (!pKeyValues)
	{
		return;
	}

	if (g_mappingApi.apiVersionLoaded)
	{
		return;
	}

	for (i32 i = 0; i < pKeyValues->Count(); i++)
	{
		auto ekv = (*pKeyValues)[i];

		if (!ekv)
		{
			continue;
		}
		const char *classname = ekv->GetString("classname");
		if (KZ_STREQI(classname, "worldspawn"))
		{
			// We only care about the first spawn group's worldspawn because the rest might use prefabs compiled outside of mapping API.
			g_mappingApi.apiVersionLoaded = true;
			g_mappingApi.mapApiVersion = ekv->GetInt("timer_mapping_api_version", KZ_NO_MAPAPI_VERSION);
			// NOTE(GameChaos): When a new mapping api version comes out, this will change
			//  for backwards compatibility.
			if (g_mappingApi.mapApiVersion == KZ_NO_MAPAPI_VERSION)
			{
				META_CONPRINTF("Warning: Map is not compiled with Mapping API. Reverting to default behavior.\n");

				// Manually create a KZ_NO_MAPAPI_COURSE_NAME course here because there shouldn't be any info_target_server_only around.
				Mapi_CreateCourse();
				break;
			}
			if (g_mappingApi.mapApiVersion != KZ_MAPAPI_VERSION)
			{
				Mapi_Error("FATAL. Mapping API version %i is invalid!", g_mappingApi.mapApiVersion);
				g_mappingApi.fatalFailure = true;
				return;
			}
			break;
		}
	}
	// Do a second pass for course descriptors.
	if (g_mappingApi.mapApiVersion != KZ_NO_MAPAPI_VERSION)
	{
		for (i32 i = 0; i < pKeyValues->Count(); i++)
		{
			auto ekv = (*pKeyValues)[i];

			if (!ekv)
			{
				continue;
			}
			const char *classname = ekv->GetString("classname");
			if (KZ_STREQI(classname, "info_target_server_only"))
			{
				Mapi_OnInfoTargetSpawn(ekv);
			}
		}
	}
}

void KZ::mapapi::OnSpawn(int count, const EntitySpawnInfo_t *info)
{
	if (!info || g_mappingApi.fatalFailure)
	{
		return;
	}

	for (i32 i = 0; i < count; i++)
	{
		auto ekv = info[i].m_pKeyValues;
#if 0
		// Debug print for all keyvalues
		FOR_EACH_ENTITYKEY(ekv, iter)
		{
			auto kv = ekv->GetKeyValue(iter);
			if (!kv)
			{
				continue;
			}
			CBufferStringGrowable<128> bufferStr;
			const char *key = ekv->GetEntityKeyId(iter).GetString();
			const char *value = kv->ToString(bufferStr);
			Msg("\t%s: %s\n", key, value);
		}
#endif

		if (!info[i].m_pEntity || !ekv || !info[i].m_pEntity->GetClassname())
		{
			continue;
		}
		const char *classname = info[i].m_pEntity->GetClassname();
		if (KZ_STREQI(classname, "trigger_multiple"))
		{
			Mapi_OnTriggerMultipleSpawn(&info[i]);
		}
	}

	// We need to pass the second time for the spawn points of courses.

	for (i32 i = 0; i < count; i++)
	{
		auto ekv = info[i].m_pKeyValues;

		if (!info[i].m_pEntity || !ekv || !info[i].m_pEntity->GetClassname())
		{
			continue;
		}
		const char *classname = info[i].m_pEntity->GetClassname();
		if (KZ_STREQI(classname, "info_teleport_destination"))
		{
			Mapi_OnInfoTeleportDestinationSpawn(&info[i]);
		}
	}

	if (g_mappingApi.fatalFailure)
	{
		g_mappingApi.triggers.RemoveAll();
		g_mappingApi.courseDescriptors.RemoveAll();
	}
}

void KZ::mapapi::OnRoundPreStart()
{
	g_mappingApi.triggers.RemoveAll();
	g_mappingApi.roundIsStarting = true;
}

void KZ::mapapi::OnRoundStart()
{
	g_mappingApi.roundIsStarting = false;
	FOR_EACH_VEC(g_mappingApi.courseDescriptors, courseInd)
	{
		// Find the number of split/checkpoint/stage zones that a course has
		//  and make sure that they all start from 1 and are consecutive by
		//  XORing the values with a consecutive 1...n sequence.
		//  https://florian.github.io/xor-trick/
		i32 splitXor = 0;
		i32 cpXor = 0;
		i32 stageXor = 0;
		i32 splitCount = 0;
		i32 cpCount = 0;
		i32 stageCount = 0;
		KZCourseDescriptor *courseDescriptor = &g_mappingApi.courseDescriptors[courseInd];
		FOR_EACH_VEC(g_mappingApi.triggers, i)
		{
			KzTrigger *trigger = &g_mappingApi.triggers[i];
			if (!KZ::mapapi::IsTimerTrigger(trigger->type))
			{
				continue;
			}

			if (!KZ_STREQ(trigger->zone.courseDescriptor, courseDescriptor->entityTargetname))
			{
				continue;
			}

			switch (trigger->type)
			{
				case KZTRIGGER_ZONE_SPLIT:
					splitXor ^= (++splitCount) ^ trigger->zone.number;
					break;
				case KZTRIGGER_ZONE_CHECKPOINT:
					cpXor ^= (++cpCount) ^ trigger->zone.number;
					break;
				case KZTRIGGER_ZONE_STAGE:
					stageXor ^= (++stageCount) ^ trigger->zone.number;
					break;
			}
		}

		bool invalid = false;
		if (splitXor != 0)
		{
			Mapi_Error("Course \"%s\" Split zones aren't consecutive or don't start at 1!", courseDescriptor->name);
			invalid = true;
		}

		if (cpXor != 0)
		{
			Mapi_Error("Course \"%s\" Checkpoint zones aren't consecutive or don't start at 1!", courseDescriptor->name);
			invalid = true;
		}

		if (stageXor != 0)
		{
			Mapi_Error("Course \"%s\" Stage zones aren't consecutive or don't start at 1!", courseDescriptor->name);
			invalid = true;
		}

		if (splitCount > KZ_MAX_SPLIT_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many split zones! Maximum is %i.", courseDescriptor->name, KZ_MAX_SPLIT_ZONES);
			invalid = true;
		}

		if (cpCount > KZ_MAX_CHECKPOINT_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many checkpoint zones! Maximum is %i.", courseDescriptor->name, KZ_MAX_CHECKPOINT_ZONES);
			invalid = true;
		}

		if (stageCount > KZ_MAX_STAGE_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many stage zones! Maximum is %i.", courseDescriptor->name, KZ_MAX_STAGE_ZONES);
			invalid = true;
		}

		if (invalid)
		{
			g_mappingApi.courseDescriptors.FastRemove(courseInd);
			courseInd--;
			break;
		}
		courseDescriptor->splitCount = splitCount;
		courseDescriptor->checkpointCount = cpCount;
		courseDescriptor->stageCount = stageCount;
	}
}

void KZ::mapapi::CheckEndTimerTrigger(CBaseTrigger *trigger)
{
	KzTrigger *kzTrigger = Mapi_FindKzTrigger(trigger);
	if (kzTrigger && kzTrigger->type == KZTRIGGER_ZONE_END)
	{
		KZCourseDescriptor *desc = Mapi_FindCourse(kzTrigger->zone.courseDescriptor);
		if (!desc)
		{
			return;
		}
		desc->hasEndPosition = utils::FindValidPositionForTrigger(trigger, desc->endPosition, desc->endAngles);
	}
}

const KzTrigger *KZ::mapapi::GetKzTrigger(CBaseTrigger *trigger)
{
	return Mapi_FindKzTrigger(trigger);
}

const KZCourseDescriptor *KZ::mapapi::GetCourseDescriptorFromTrigger(CBaseTrigger *trigger)
{
	KzTrigger *kzTrigger = Mapi_FindKzTrigger(trigger);
	if (!kzTrigger)
	{
		return nullptr;
	}
	return KZ::mapapi::GetCourseDescriptorFromTrigger(kzTrigger);
}

const KZCourseDescriptor *KZ::mapapi::GetCourseDescriptorFromTrigger(const KzTrigger *trigger)
{
	const KZCourseDescriptor *course = nullptr;
	switch (trigger->type)
	{
		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		case KZTRIGGER_ZONE_STAGE:
		{
			course = Mapi_FindCourse(trigger->zone.courseDescriptor);
			if (!course)
			{
				Mapi_Error("%s: Couldn't find course descriptor from name \"%s\"! Trigger's Hammer Id: %i", g_errorPrefix,
						   trigger->zone.courseDescriptor, trigger->hammerId);
			}
		}
		break;
	}
	return course;
}

bool MappingInterface::IsTriggerATimerZone(CBaseTrigger *trigger)
{
	KzTrigger *kzTrigger = Mapi_FindKzTrigger(trigger);
	if (!kzTrigger)
	{
		return false;
	}
	return KZ::mapapi::IsTimerTrigger(kzTrigger->type);
}

bool MappingInterface::GetJumpstatArea(Vector &pos, QAngle &angles)
{
	if (g_mappingApi.hasJumpstatArea)
	{
		pos = g_mappingApi.jumpstatAreaPos;
		angles = g_mappingApi.jumpstatAreaAngles;
	}

	return g_mappingApi.hasJumpstatArea;
}

void KZ::course::ClearCourses()
{
	g_sortedCourses.RemoveAll();
	KZTimerService::ClearRecordCache();
}

u32 KZ::course::GetCourseCount()
{
	return g_sortedCourses.Count();
}

const KZCourseDescriptor *KZ::course::GetCourseByCourseID(i32 id)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->id == id)
		{
			return g_sortedCourses[i];
		}
	}
	return nullptr;
}

const KZCourseDescriptor *KZ::course::GetCourseByLocalCourseID(u32 id)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->localDatabaseID == id)
		{
			return g_sortedCourses[i];
		}
	}
	return nullptr;
}

const KZCourseDescriptor *KZ::course::GetCourseByGlobalCourseID(u32 id)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->globalDatabaseID == id)
		{
			return g_sortedCourses[i];
		}
	}
	return nullptr;
}

const KZCourseDescriptor *KZ::course::GetCourse(const char *courseName, bool caseSensitive, bool matchPartial)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		const char *name = g_sortedCourses[i]->name;
		if (caseSensitive ? KZ_STREQ(name, courseName) : KZ_STREQI(name, courseName))
		{
			return g_sortedCourses[i];
		}
	}
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		const char *name = g_sortedCourses[i]->name;
		if (matchPartial)
		{
			if (caseSensitive ? V_strstr(name, courseName) : V_stristr(name, courseName))
			{
				return g_sortedCourses[i];
			}
		}
	}
	return nullptr;
}

const KZCourseDescriptor *KZ::course::GetCourse(u32 guid)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->guid == guid)
		{
			return g_sortedCourses[i];
		}
	}
	return nullptr;
}

const KZCourseDescriptor *KZ::course::GetFirstCourse()
{
	if (g_sortedCourses.Count() >= 1)
	{
		return g_sortedCourses[0];
	}
	return nullptr;
}

void KZ::course::SetupLocalCourses()
{
	if (KZDatabaseService::IsMapSetUp())
	{
		KZDatabaseService::SetupCourses(g_sortedCourses);
	}
}

bool KZ::course::UpdateCourseLocalID(const char *courseName, u32 databaseID)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->GetName() == courseName)
		{
			g_sortedCourses[i]->localDatabaseID = databaseID;
			return true;
		}
	}
	return false;
}

bool KZ::course::UpdateCourseGlobalID(const char *courseName, u32 globalID)
{
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		if (g_sortedCourses[i]->GetName() == courseName)
		{
			g_sortedCourses[i]->globalDatabaseID = globalID;
			return true;
		}
	}
	return false;
}

static void ListCourses(KZPlayer *player)
{
	if (player->timerService->GetCourse())
	{
		player->languageService->PrintChat(true, false, "Current Course", player->timerService->GetCourse()->name);
	}
	else
	{
		player->languageService->PrintChat(true, false, "No Current Course");
	}
	KZ::course::PrintCourses(player);
}

SCMD(kz_courses, SCFL_MAP)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	ListCourses(player);
	return MRES_SUPERCEDE;
}

SCMD(kz_course, SCFL_MAP)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	if (args->ArgC() < 2)
	{
		ListCourses(player);
	}
	else
	{
		KZ::misc::HandleTeleportToCourse(player, args);
	}
	return MRES_SUPERCEDE;
}

// TODO: Does this *really* belong here?
// clang-format off
#define COURSE_TABLE_NAME "Courses - Table Name"
static_global const char *columnKeysWithAPI[] = {
	"#.",
	"Course Info Header - Name",
	"Course Info Header - Mappers",
	"Course Info Header - State (Classic)",
	"Course Info Header - Tier (Classic)",
	"Course Info Header - State (Vanilla)",
	"Course Info Header - Tier (Vanilla)"
};
static_global const char *columnKeysWithoutAPI[] = {
	"#.",
	"Course Info Header - Name",
};
static_global const char *courseStateKeys[] = {
	"Course Info - Unranked",
	"Course Info - Pending",
	"Course Info - Ranked"
};
// clang-format on

static_function void PrintCoursesWithMap(KZPlayer *player, const std::vector<KZ::api::Map::Course> &courses)
{
	CUtlString headers[KZ_ARRAYSIZE(columnKeysWithAPI)];
	for (u32 col = 0; col < KZ_ARRAYSIZE(columnKeysWithAPI); col++)
	{
		headers[col] = player->languageService->PrepareMessage(columnKeysWithAPI[col]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(columnKeysWithAPI)> table(player->languageService->PrepareMessage(COURSE_TABLE_NAME).c_str(), headers);
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		KZCourseDescriptor *course = g_sortedCourses[i];
		const KZ::api::Map::Course *apiCourse = nullptr;
		for (auto &c : courses)
		{
			if (c.id == course->globalDatabaseID)
			{
				apiCourse = &c;
				break;
			}
		}
		std::string tierClassic {}, tierVanilla {}, stateClassic {}, stateVanilla {}, mappers {};
		if (apiCourse)
		{
			// Mappers
			for (u32 m = 0; m < apiCourse->mappers.size(); m++)
			{
				mappers += apiCourse->mappers[m].name;
				if (m < apiCourse->mappers.size() - 1)
				{
					mappers += ", ";
				}
			}
			// Tiers
			tierClassic = std::to_string((u8)apiCourse->filters.classic.nubTier) + "/" + std::to_string((u8)apiCourse->filters.classic.proTier);
			tierVanilla = std::to_string((u8)apiCourse->filters.vanilla.nubTier) + "/" + std::to_string((u8)apiCourse->filters.vanilla.proTier);
			// States
			stateClassic = player->languageService->PrepareMessage(courseStateKeys[(u8)apiCourse->filters.classic.state + 1]);
			stateVanilla = player->languageService->PrepareMessage(courseStateKeys[(u8)apiCourse->filters.vanilla.state + 1]);
		}
		else
		{
			META_CONPRINTF("Warning: Course ID %i not found in API map data!\n", course->globalDatabaseID);
		}
		// clang-format off
		table.SetRow(
			i,
			std::to_string(course->id).c_str(),
			course->name,
			mappers.c_str(),
			stateClassic.c_str(),
			tierClassic.c_str(),
			stateVanilla.c_str(),
			tierVanilla.c_str()
		);
		// clang-format on
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

static_function void PrintCoursesWithoutMap(KZPlayer *player)
{
	CUtlString headers[KZ_ARRAYSIZE(columnKeysWithoutAPI)];
	for (u32 col = 0; col < KZ_ARRAYSIZE(columnKeysWithoutAPI); col++)
	{
		headers[col] = player->languageService->PrepareMessage(columnKeysWithoutAPI[col]).c_str();
	}
	utils::Table<KZ_ARRAYSIZE(columnKeysWithoutAPI)> table(player->languageService->PrepareMessage(COURSE_TABLE_NAME).c_str(), headers);
	FOR_EACH_VEC(g_sortedCourses, i)
	{
		KZCourseDescriptor *course = g_sortedCourses[i];
		// clang-format off
		table.SetRow(
			i,
			std::to_string(course->id).c_str(),
			course->name
		);
		// clang-format on
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
	player->PrintConsole(false, false, table.GetTitle());
	player->PrintConsole(false, false, table.GetHeader());
	for (u32 i = 0; i < table.GetNumEntries(); i++)
	{
		player->PrintConsole(false, false, table.GetLine(i));
	}
	player->PrintConsole(false, false, table.GetSeparator("="));
}

void KZ::course::PrintCourses(KZPlayer *player)
{
	// If we have API map data, that means we have information about individual mappers for each course, along with tiers for both modes.
	// Otherwise, the only course information we have is the course ID and the course name.
	bool hasMap = false;
	std::vector<KZ::api::Map::Course> courses;
	hasMap = KZGlobalService::WithCurrentMap(
		[&](const std::optional<KZ::api::Map> &currentMap)
		{
			if (currentMap)
			{
				courses = currentMap->courses;
				return true;
			}
			return false;
		});
	if (hasMap)
	{
		PrintCoursesWithMap(player, courses);
	}
	else
	{
		PrintCoursesWithoutMap(player);
	}
}

static_function void PrintCurrentMapCoursesInfo(KZPlayer *player)
{
	if (!KZGlobalService::IsAvailable())
	{
		player->languageService->PrintChat(true, false, "Map Info - No API Connection");
	}

	bool hasMap = false;
	std::vector<KZ::api::Map::Course> courses;
	std::vector<KZ::api::PlayerInfo> mappers;
	std::string mapApprovedAt;
	KZ::api::Map::State mapState;
	std::string mapDescription;
	hasMap = KZGlobalService::WithCurrentMap(
		[&](const std::optional<KZ::api::Map> &currentMap)
		{
			if (currentMap)
			{
				courses = currentMap->courses;
				mappers = currentMap->mappers;
				mapState = currentMap->state;
				mapDescription = currentMap->description.value_or("");
				mapApprovedAt = currentMap->approvedAt;
				return true;
			}
			return false;
		});

	if (!hasMap)
	{
		player->languageService->PrintChat(true, false, "Map Info - No API Map Data");
	}
	player->languageService->PrintChat(true, false, "Map Info - Check Console");
	CUtlString mapName = g_pKZUtils->GetCurrentMapName();

	// Map name
	if (g_pKZUtils->GetCurrentMapWorkshopID())
	{
		char workshopID[16];
		V_snprintf(workshopID, sizeof(workshopID), " (%llu)", g_pKZUtils->GetCurrentMapWorkshopID());
		mapName += workshopID;
	}
	player->languageService->PrintConsole(false, false, "Map Info - Map Info Header", mapName.Get());

	if (hasMap)
	{
		// Mappers
		if (mappers.size() > 0)
		{
			std::string mappersStr;
			for (u32 i = 0; i < mappers.size(); i++)
			{
				mappersStr += mappers[i].name;
				if (i < mappers.size() - 1)
				{
					mappersStr += ", ";
				}
			}
			player->languageService->PrintConsole(false, false, "Map Info - Mappers", mappersStr.c_str());
		}
		// Map state
		std::string stateStr;
		switch (mapState)
		{
			case KZ::api::Map::State::Approved:
				stateStr = player->languageService->PrepareMessage("Map Info - Approved", mapApprovedAt.c_str());
				break;
			case KZ::api::Map::State::InTesting:
				stateStr = player->languageService->PrepareMessage("Map Info - In Testing");
				break;
			default:
				stateStr = player->languageService->PrepareMessage("Map Info - Invalid");
				break;
		}
		player->languageService->PrintConsole(false, false, "Map Info - State", stateStr.c_str());
		// Map description
		if (mapDescription.length() > 0)
		{
			player->languageService->PrintConsole(false, false, "Map Info - Description", mapDescription.c_str());
		}
	}
	// Courses
	KZ::course::PrintCourses(player);
}

SCMD(kz_mapinfo, SCFL_MAP | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PrintCurrentMapCoursesInfo(player);
	return MRES_SUPERCEDE;
}

SCMD_LINK(kz_mi, kz_mapinfo)

static_function void PrintCourseTier(KZPlayer *player, const CCommand *args)
{
	const KZCourseDescriptor *course = player->timerService->GetCourse();
	if (args->ArgC() < 2 && course == nullptr)
	{
		player->languageService->PrintChat(true, false, "No Current Course");
		KZ::course::PrintCourses(player);
		return;
	}
	const char *courseName = args->ArgS();
	if (!course)
	{
		if (utils::IsNumeric(courseName))
		{
			i32 courseID = atoi(courseName);
			course = KZ::course::GetCourseByCourseID(courseID);
		}
		else
		{
			course = KZ::course::GetCourse(courseName, false, true);
		}
	}
	if (!course)
	{
		player->languageService->PrintChat(true, false, "Course Data Unavailable", courseName);
		return;
	}
	if (!KZGlobalService::IsAvailable())
	{
		player->languageService->PrintChat(true, false, "Map Info - No API Connection (Short)");
		return;
	}
	bool hasMap = false;
	std::vector<KZ::api::Map::Course> courses;
	hasMap = KZGlobalService::WithCurrentMap(
		[&](const std::optional<KZ::api::Map> &currentMap)
		{
			if (currentMap)
			{
				courses = currentMap->courses;
				return true;
			}
			return false;
		});
	if (!hasMap)
	{
		player->languageService->PrintChat(true, false, "Map Info - No API Map Data (Short)");
		return;
	}
	const KZ::api::Map::Course *apiCourse = nullptr;
	for (auto &c : courses)
	{
		if (c.id == course->globalDatabaseID)
		{
			apiCourse = &c;
			break;
		}
	}
	// If the course isn't found in the API map data or the player's current mode isn't Classic or Vanilla, we can't show tier info.
	if (!apiCourse || (!KZ_STREQI(player->modeService->GetModeName(), "Classic") && (!KZ_STREQI(player->modeService->GetModeName(), "Vanilla"))))
	{
		player->languageService->PrintChat(true, false, "Course Data Unavailable", courseName);
		return;
	}
	u8 nubTier =
		u8(KZ_STREQI(player->modeService->GetModeName(), "Classic") ? apiCourse->filters.classic.nubTier : apiCourse->filters.vanilla.nubTier);
	u8 proTier =
		u8(KZ_STREQI(player->modeService->GetModeName(), "Classic") ? apiCourse->filters.classic.proTier : apiCourse->filters.vanilla.proTier);
	std::string state = KZ_STREQI(player->modeService->GetModeName(), "Classic")
							? player->languageService->PrepareMessage(courseStateKeys[(u8)apiCourse->filters.classic.state + 1])
							: player->languageService->PrepareMessage(courseStateKeys[(u8)apiCourse->filters.vanilla.state + 1]);
	std::string description = apiCourse->description ? ": " + apiCourse->description.value() : "";
	player->languageService->PrintChat(true, false, "Tier Info", course->name, nubTier, proTier, state.c_str(), description.c_str());
}

SCMD(kz_tier, SCFL_MAP | SCFL_GLOBAL)
{
	KZPlayer *player = g_pKZPlayerManager->ToPlayer(controller);
	PrintCourseTier(player, args);
	return MRES_SUPERCEDE;
}
