
#include "kz/kz.h"
#include "movement/movement.h"
#include "kz_mappingapi.h"
#include "entity2/entitykeyvalues.h"
#include "sdk/entity/cbasetrigger.h"
#include "utils/ctimer.h"

#include "tier0/memdbgon.h"

#define KEY_TRIGGER_TYPE          "timer_trigger_type"
#define KEY_IS_COURSE_DESCRIPTOR  "timer_course_descriptor"
#define INVALID_SPLIT_NUMBER      0
#define INVALID_CHECKPOINT_NUMBER 0
#define INVALID_STAGE_NUMBER      0
#define INVALID_COURSE_NUMBER     0

enum
{
	MAPI_ERR_TOO_MANY_TRIGGERS = 1 << 0,
	MAPI_ERR_TOO_MANY_COURSES = 1 << 1,
};

static_global struct
{
	i32 triggerCount;
	KzTrigger triggers[2048];
	i32 courseCount;
	KzCourseDescriptor courses[512];

	bool roundIsStarting;
	i32 errorFlags;
	i32 errorCount;
	char errors[32][256];
} g_mappingApi;

static_global CTimer<> *g_errorTimer;
static_global const char *g_errorPrefix = "{darkred} ERROR: ";
static_global const char *g_triggerNames[] = {"Disabled",   "Modifier",   "Reset Checkpoints", "Single Bhop Reset", "Antibhop",
											  "Start zone", "End zone",   "Split zone",        "Checkpoint zone",   "Stage zone",
											  "Teleport",   "Multi bhop", "Single bhop",       "Sequential bhop"};

static_function MappingInterface g_mappingInterface;
MappingInterface *g_pMappingApi = nullptr;

// TODO: add error check to make sure a course has at least 1 start zone and 1 end zone

static_function void Mapi_Error(const char *format, ...)
{
	i32 errorIndex = g_mappingApi.errorCount;
	if (errorIndex >= Q_ARRAYSIZE(g_mappingApi.errors[0]))
	{
		return;
	}
	else if (errorIndex == Q_ARRAYSIZE(g_mappingApi.errors[0]) - 1)
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
		utils::CPrintChatAll("%sToo many Mapping API triggers! Maximum is %i!", g_errorPrefix, Q_ARRAYSIZE(g_mappingApi.triggers));
	}
	if (g_mappingApi.errorFlags & MAPI_ERR_TOO_MANY_COURSES)
	{
		utils::CPrintChatAll("%sToo many Courses! Maximum is %i!", g_errorPrefix, Q_ARRAYSIZE(g_mappingApi.courses));
	}
	for (i32 i = 0; i < g_mappingApi.errorCount; i++)
	{
		utils::CPrintChatAll("%s%s", g_errorPrefix, g_mappingApi.errors[i]);
	}

	return 60.0;
}

static_function bool Mapi_AddTrigger(KzTrigger trigger)
{
	if (g_mappingApi.triggerCount >= Q_ARRAYSIZE(g_mappingApi.triggers))
	{
		assert(0);
		g_mappingApi.errorFlags |= MAPI_ERR_TOO_MANY_TRIGGERS;
		return false;
	}

	g_mappingApi.triggers[g_mappingApi.triggerCount++] = trigger;
	return true;
}

static_function void Mapi_AddCourse(KzCourseDescriptor course)
{
	assert(g_mappingApi.courseCount < Q_ARRAYSIZE(g_mappingApi.courses));
	if (g_mappingApi.courseCount >= Q_ARRAYSIZE(g_mappingApi.courses))
	{
		assert(0);
		g_mappingApi.errorFlags |= MAPI_ERR_TOO_MANY_COURSES;
		return;
	}

	g_mappingApi.courses[g_mappingApi.courseCount++] = course;
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

	const CEntityKeyValues *ekv = info->m_pKeyValues;
	i32 hammerId = ekv->GetInt("hammerUniqueId", -1);
	Vector origin = ekv->GetVector("origin");

	KzTriggerType type = (KzTriggerType)ekv->GetInt(KEY_TRIGGER_TYPE, KZTRIGGER_DISABLED);

	if (type < KZTRIGGER_DISABLED || type >= KZTRIGGER_COUNT)
	{
		assert(0);
		Mapi_Error("Trigger type %i is invalid and out of range (%i-%i) for trigger with Hammer ID %i, origin (%.0f %.0f %.0f)!", type,
				   KZTRIGGER_DISABLED, KZTRIGGER_COUNT - 1, hammerId, origin.x, origin.y, origin.z);
		return;
	}

	if (!g_mappingApi.roundIsStarting)
	{
		// Only allow triggers and zones that were spawned during the round start phase.
		Mapi_Error("Trigger %s spawned after the map was loaded, the trigger won't be loaded! Hammer ID %i, origin (%.0f %.0f %.0f)",
				   g_triggerNames[type], hammerId, origin.x, origin.y, origin.z);
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
		}
		break;

		// NOTE: Nothing to do here
		case KZTRIGGER_RESET_CHECKPOINTS:
		case KZTRIGGER_SINGLE_BHOP_RESET:
			break;

		case KZTRIGGER_ANTI_BHOP:
		{
			// TODO: min value for antibhop time (bigger or equal to 0)
			trigger.antibhop.time = ekv->GetFloat("timer_anti_bhop_time");
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
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			const char *destination = ekv->GetString("timer_teleport_destination");
			V_snprintf(trigger.teleport.destination, sizeof(trigger.teleport.destination), "%s", destination);
			// TODO: min/max values for delay
			trigger.teleport.delay = ekv->GetFloat("timer_teleport_delay", -1.0f);
			trigger.teleport.useDestinationAngles = ekv->GetBool("timer_teleport_use_dest_angles");
			trigger.teleport.resetSpeed = ekv->GetBool("timer_teleport_reset_speed");
			trigger.teleport.reorientPlayer = ekv->GetBool("timer_teleport_reorient_player");
			trigger.teleport.relative = ekv->GetBool("timer_teleport_relative");
		}
		break;

		default:
		{
			// technically impossible to happen, leave an assert here anyway for debug builds.
			assert(0);
			return;
		}
		break;
	}

	Mapi_AddTrigger(trigger);
}

static_function void Mapi_OnInfoTargetSpawn(const EntitySpawnInfo_t *info)
{
	const CEntityKeyValues *ekv = info->m_pKeyValues;

	if (!ekv->GetBool(KEY_IS_COURSE_DESCRIPTOR))
	{
		return;
	}

	i32 hammerId = ekv->GetInt("hammerUniqueId", -1);
	Vector origin = ekv->GetVector("origin");
	if (!g_mappingApi.roundIsStarting)
	{
		// Only allow courses that were spawned during the round start phase.
		Mapi_Error("Course spawned after the map was loaded, the course will be ignored! Hammer ID %i, origin (%.0f %.0f %.0f)", hammerId, origin.x,
				   origin.y, origin.z);
		return;
	}

	KzCourseDescriptor course = {};
	course.number = ekv->GetInt("timer_course_number", INVALID_COURSE_NUMBER);
	course.hammerId = hammerId;

	// TODO: make sure course descriptor names are unique!

	if (course.number <= INVALID_COURSE_NUMBER)
	{
		Mapi_Error("Course number must be bigger than %i! Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)", INVALID_COURSE_NUMBER,
				   course.hammerId, origin.x, origin.y, origin.z);
		return;
	}

	V_snprintf(course.name, sizeof(course.name), "%s", ekv->GetString("timer_course_name"));
	if (!course.name[0])
	{
		Mapi_Error("Course name is empty! Course number %i. Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)", course.number, course.hammerId,
				   origin.x, origin.y, origin.z);
		return;
	}

	V_snprintf(course.entityTargetname, sizeof(course.entityTargetname), "%s", ekv->GetString("targetname"));
	if (!course.entityTargetname[0])
	{
		Mapi_Error("Course targetname is empty! Course name \"%s\". Course number %i. Course descriptor Hammer ID %i, origin (%.0f %.0f %.0f)",
				   course.name, course.number, course.hammerId, origin.x, origin.y, origin.z);
		return;
	}

	course.disableCheckpoints = ekv->GetBool("timer_course_disable_checkpoint");

	Mapi_AddCourse(course);
}

static_function KzTrigger *Mapi_FindKzTrigger(CBaseTrigger *trigger)
{
	KzTrigger *result = nullptr;
	if (!trigger->m_pEntity)
	{
		return result;
	}

	CEntityHandle triggerHandle = trigger->GetRefEHandle();
	if (!trigger || !triggerHandle.IsValid() || trigger->m_pEntity->m_flags & EF_IS_INVALID_EHANDLE)
	{
		return result;
	}

	for (i32 i = 0; i < g_mappingApi.triggerCount; i++)
	{
		if (triggerHandle == g_mappingApi.triggers[i].entity)
		{
			result = &g_mappingApi.triggers[i];
			break;
		}
	}

	return result;
}

static_function const KzCourseDescriptor *Mapi_FindCourse(const char *targetname)
{
	KzCourseDescriptor *result = nullptr;
	if (!targetname)
	{
		return result;
	}

	for (i32 i = 0; i < g_mappingApi.courseCount; i++)
	{
		if (V_stricmp(g_mappingApi.courses[i].entityTargetname, targetname) == 0)
		{
			result = &g_mappingApi.courses[i];
			break;
		}
	}

	return result;
}

bool MappingInterface::IsTriggerATimerZone(CBaseTrigger *trigger)
{
	KzTrigger *mvTrigger = Mapi_FindKzTrigger(trigger);
	if (!mvTrigger)
	{
		return false;
	}
	static_assert(KZTRIGGER_ZONE_START == 5 && KZTRIGGER_ZONE_STAGE == 9,
				  "Don't forget to change this function when changing the KzTriggerType enum!!!");
	return mvTrigger->type >= KZTRIGGER_ZONE_START && mvTrigger->type <= KZTRIGGER_ZONE_STAGE;
}

bool MappingInterface::IsBhopTrigger(KzTriggerType triggerType)
{
	bool result = triggerType == KZTRIGGER_MULTI_BHOP || triggerType == KZTRIGGER_SINGLE_BHOP || triggerType == KZTRIGGER_SEQUENTIAL_BHOP;
	return result;
}

void Mappingapi_RoundPrestart()
{
	g_mappingApi = {};
	g_pMappingApi = &g_mappingInterface;
	g_mappingApi.roundIsStarting = true;

	g_errorTimer = StartTimer(Mapi_PrintErrors, true);
}

void Mappingapi_RoundStart()
{
	g_mappingApi.roundIsStarting = false;
	for (i32 courseInd = 0; courseInd < g_mappingApi.courseCount; courseInd++)
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
		KzCourseDescriptor *course = &g_mappingApi.courses[courseInd];
		for (i32 i = 0; i < g_mappingApi.triggerCount; i++)
		{
			KzTrigger *trigger = &g_mappingApi.triggers[i];
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
		if (splitXor != 0)
		{
			Mapi_Error("Course \"%s\" Split zones aren't consecutive or don't start at 1!", course->name);
		}
		if (cpXor != 0)
		{
			Mapi_Error("Course \"%s\" Checkpoint zones aren't consecutive or don't start at 1!", course->name);
		}
		if (stageXor != 0)
		{
			Mapi_Error("Course \"%s\" Stage zones aren't consecutive or don't start at 1!", course->name);
		}
		// remove course if split/cp/stage zones aren't consecutive or don't start at 1
		if (splitXor != 0 || cpXor != 0 || stageXor != 0)
		{
			// TODO: change to cutlvectorfixed
			if (g_mappingApi.courseCount > 1)
			{
				g_mappingApi.courses[courseInd] = g_mappingApi.courses[g_mappingApi.courseCount - 1];
			}
			courseInd--;
			g_mappingApi.courseCount--;
			break;
		}
		course->splitCount = splitCount;
		course->checkpointCount = cpCount;
		course->stageCount = stageCount;
	}
}

void MappingInterface::OnTriggerMultipleStartTouchPost(KZPlayer *player, CBaseTrigger *trigger)
{
	KzTrigger *touched = Mapi_FindKzTrigger(trigger);
	if (!touched)
	{
		return;
	}

	const KzCourseDescriptor *course = nullptr;
	switch (touched->type)
	{
		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		case KZTRIGGER_ZONE_STAGE:
		{
			course = Mapi_FindCourse(touched->zone.courseDescriptor);
			if (!course)
			{
				utils::CPrintChatAll("%strigger_multiple StartTouch: Couldn't find course descriptor from name \"%s\"! Trigger's Hammer Id: %i",
									 g_errorPrefix, touched->zone.courseDescriptor, touched->hammerId);
				return;
			}
		}
		break;
	}

	player->MappingApiTriggerStartTouch(touched, course);
}

void MappingInterface::OnTriggerMultipleEndTouchPost(KZPlayer *player, CBaseTrigger *trigger)
{
	KzTrigger *touched = Mapi_FindKzTrigger(trigger);
	if (!touched)
	{
		return;
	}

	const KzCourseDescriptor *course = nullptr;
	switch (touched->type)
	{
		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		case KZTRIGGER_ZONE_STAGE:
		{
			course = Mapi_FindCourse(touched->zone.courseDescriptor);
			if (!course)
			{
				utils::CPrintChatAll("%strigger_multiple EndTouch: Couldn't find course descriptor from name \"%s\"! Trigger's Hammer Id: %i",
									 g_errorPrefix, touched->zone.courseDescriptor, touched->hammerId);
				return;
			}
		}
		break;
	}

	player->MappingApiTriggerEndTouch(touched, course);
}

void MappingInterface::OnSpawnPost(int count, const EntitySpawnInfo_t *info)
{
	if (!info)
	{
		return;
	}

	for (i32 i = 0; i < count; i++)
	{
		auto ekv = info[i].m_pKeyValues;
		if (!info[i].m_pEntity || !ekv || !info[i].m_pEntity->GetClassname())
		{
			continue;
		}
		const char *classname = info[i].m_pEntity->GetClassname();
		if (V_stricmp(classname, "trigger_multiple") == 0)
		{
			Mapi_OnTriggerMultipleSpawn(&info[i]);
		}
		else if (V_stricmp(classname, "info_target_server_only") == 0)
		{
			Mapi_OnInfoTargetSpawn(&info[i]);
			Msg("info target\n");
		}
	}
}
