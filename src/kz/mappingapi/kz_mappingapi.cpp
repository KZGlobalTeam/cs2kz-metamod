
#include "kz/kz.h"
#include "movement/movement.h"
#include "kz_mappingapi.h"
#include "entity2/entitykeyvalues.h"

#include "tier0/memdbgon.h"

#define KEY_TRIGGER_TYPE         "timer_trigger_type"
#define KEY_IS_COURSE_DESCRIPTOR "timer_course_descriptor"
#define INVALID_STAGE_NUMBER     -1

struct KzTrigger
{
	KzTriggerType type;
	CEntityHandle entity;

	union
	{
		Modifier modifier;
		Antibhop antibhop;
		Zone zone;
		StageZone stageZone;
		Teleport teleport;
	};
};

static_function struct
{
	i32 triggerCount;
	KzTrigger triggers[2048];
	i32 courseCount;
	KzCourseDescriptor courses[512];
} g_mappingApi;

static_function MappingInterface g_mappingInterface;
MappingInterface *g_pMappingApi = nullptr;

static_function KzTrigger *Mapi_NewTrigger(KzTriggerType type)
{
	// TODO: bounds checking
	assert(g_mappingApi.triggerCount < Q_ARRAYSIZE(g_mappingApi.triggers));
	KzTrigger *result = nullptr;
	if (g_mappingApi.triggerCount < Q_ARRAYSIZE(g_mappingApi.triggers))
	{
		result = &g_mappingApi.triggers[g_mappingApi.triggerCount++];
		*result = {};
		result->type = type;
	}
	return result;
}

static_function KzCourseDescriptor *Mapi_NewCourse()
{
	// TODO: bounds checking
	assert(g_mappingApi.courseCount < Q_ARRAYSIZE(g_mappingApi.courses));
	KzCourseDescriptor *result = nullptr;
	if (g_mappingApi.courseCount < Q_ARRAYSIZE(g_mappingApi.courses))
	{
		result = &g_mappingApi.courses[g_mappingApi.courseCount++];
		*result = {};
	}
	return result;
}

static_function void Mapi_OnTriggerMultipleSpawn(const EntitySpawnInfo_t *info)
{
	const CEntityKeyValues *ekv = info->m_pKeyValues;
	KzTriggerType type = (KzTriggerType)ekv->GetInt(KEY_TRIGGER_TYPE, KZTRIGGER_DISABLED);

	if (type <= KZTRIGGER_DISABLED || type >= KZTRIGGER_COUNT)
	{
		// TODO: print error!
		assert(0);
		return;
	}
	KzTrigger *trigger = Mapi_NewTrigger(type);

	if (!trigger)
	{
		// TODO: print error!
		assert(0);
		return;
	}

	trigger->entity = info->m_pEntity->GetRefEHandle();
	switch (type)
	{
		case KZTRIGGER_MODIFIER:
		{
			trigger->modifier.disablePausing = ekv->GetBool("timer_modifier_disable_pause");
			trigger->modifier.disableCheckpoints = ekv->GetBool("timer_modifier_disable_checkpoints");
			trigger->modifier.disableTeleports = ekv->GetBool("timer_modifier_disable_teleports");
			trigger->modifier.disableJumpstats = ekv->GetBool("timer_modifier_disable_jumpstats");
			trigger->modifier.enableSlide = ekv->GetBool("timer_modifier_enable_slide");
		}
		break;

		case KZTRIGGER_ANTI_BHOP:
		{
			trigger->antibhop.time = ekv->GetFloat("timer_anti_bhop_time");
		}
		break;

		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		case KZTRIGGER_ZONE_STAGE:
		{
			// TODO: find course descriptor entity!
			const char *courseDescriptor = ekv->GetString("timer_zone_course_descriptor");
			V_snprintf(trigger->zone.courseDescriptor, sizeof(trigger->zone.courseDescriptor), "%s", courseDescriptor);

			if (type == KZTRIGGER_ZONE_STAGE)
			{
				trigger->stageZone.stageNumber = ekv->GetInt("timer_zone_stage_number", INVALID_STAGE_NUMBER);
			}
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
		{
			const char *destination = ekv->GetString("timer_teleport_destination");
			V_snprintf(trigger->teleport.destination, sizeof(trigger->teleport.destination), "%s", destination);
			trigger->teleport.delay = ekv->GetFloat("timer_anti_bhop_time", -1.0f);
			trigger->teleport.useDestinationAngles = ekv->GetBool("timer_teleport_use_dest_angles");
			trigger->teleport.resetSpeed = ekv->GetBool("timer_teleport_reset_speed");
			trigger->teleport.reorientPlayer = ekv->GetBool("timer_teleport_reorient_player");
			trigger->teleport.relative = ekv->GetBool("timer_teleport_relative");
		}
		break;

		// case KZTRIGGER_RESET_CHECKPOINTS:
		// case KZTRIGGER_SINGLE_BHOP_RESET:
		default:
			break;
	}
}

static_function void Mapi_OnInfoTargetSpawn(const EntitySpawnInfo_t *info)
{
	const CEntityKeyValues *ekv = info->m_pKeyValues;

	if (!ekv->GetBool(KEY_IS_COURSE_DESCRIPTOR))
	{
		return;
	}

	KzCourseDescriptor *course = Mapi_NewCourse();
	if (!course)
	{
		// TODO: error
		return;
	}

	V_snprintf(course->entityTargetname, sizeof(course->entityTargetname), "%s", ekv->GetString("targetname"));
	V_snprintf(course->name, sizeof(course->name), "%s", ekv->GetString("timer_course_name"));

	course->disableCheckpoints = ekv->GetBool("timer_course_disable_checkpoint");
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

void Mappingapi_Initialize()
{
	g_pMappingApi = &g_mappingInterface;
	g_mappingApi.triggerCount = 0;
	g_mappingApi.courseCount = 0;
}

const KzCourseDescriptor *Mapi_FindCourse(const char *targetname)
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
				// TODO: print error!
				return;
			}
		}
		break;
	}

	switch (touched->type)
	{
		case KZTRIGGER_MODIFIER:
		case KZTRIGGER_RESET_CHECKPOINTS:
		case KZTRIGGER_SINGLE_BHOP_RESET:
		case KZTRIGGER_ANTI_BHOP:
			break;

		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_END:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		{
			player->ZoneStartTouch(course, touched->type);
		}
		break;

		case KZTRIGGER_ZONE_STAGE:
		{
			player->StageZoneStartTouch(course, touched->stageZone.stageNumber);
		}
		break;

		case KZTRIGGER_TELEPORT:
		case KZTRIGGER_MULTI_BHOP:
		case KZTRIGGER_SINGLE_BHOP:
		case KZTRIGGER_SEQUENTIAL_BHOP:
			break;
		default:
			break;
	}
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
				// TODO: print error!
				return;
			}
		}
		break;
	}

	switch (touched->type)
	{
		case KZTRIGGER_ZONE_START:
		case KZTRIGGER_ZONE_SPLIT:
		case KZTRIGGER_ZONE_CHECKPOINT:
		{
			player->ZoneEndTouch(course, touched->type);
		}
		break;
		
		case KZTRIGGER_ZONE_STAGE:
		{
			player->StageZoneEndTouch(course, touched->stageZone.stageNumber);
		}
		break;
		
		default:
			break;
	}
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
		}
	}
}
