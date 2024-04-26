
#include "movement.h"
#include "mv_mappingapi.h"
#include "entity2/entitykeyvalues.h"

#include "tier0/memdbgon.h"

#define KEY_TRIGGER_TYPE         "timer_trigger_type"
#define KEY_IS_COURSE_DESCRIPTOR "timer_course_descriptor"
#define INVALID_STAGE_NUMBER     -1

enum MvTriggerType
{
	MVTRIGGER_DISABLED = 0,
	MVTRIGGER_MODIFIER,
	MVTRIGGER_RESET_CHECKPOINTS,
	MVTRIGGER_SINGLE_BHOP_RESET,
	MVTRIGGER_ANTI_BHOP,
	MVTRIGGER_ZONE_START,
	MVTRIGGER_ZONE_END,
	MVTRIGGER_ZONE_SPLIT,
	MVTRIGGER_ZONE_CHECKPOINT,
	MVTRIGGER_ZONE_STAGE,
	MVTRIGGER_TELEPORT,
	MVTRIGGER_MULTI_BHOP,
	MVTRIGGER_SINGLE_BHOP,
	MVTRIGGER_SEQUENTIAL_BHOP,
	MVTRIGGER_COUNT,
};

struct MvTrigger
{
	MvTriggerType type;

	// MVTRIGGER_MODIFIER
	struct
	{
		bool disablePausing;
		bool disableCheckpoints;
		bool disableTeleports;
		bool disableJumpstats;
		bool enableSlide;
	} modifier;

	// MVTRIGGER_ANTI_BHOP
	struct
	{
		f32 time;
	} antibhop;

	// MVTRIGGER_ZONE_* (including MVTRIGGER_ZONE_STAGE)
	struct
	{
		char courseDescriptor[128];
	} zone;

	// MVTRIGGER_ZONE_STAGE
	struct
	{
		i32 number;
	} stageZone;

	struct
	{
		char destination[128];
		f32 delay;
		bool useDestinationAngles;
		bool resetSpeed;
		bool reorientPlayer;
		bool relative;
	} teleport;
};

struct MvCourseDescriptor
{
	char name[128];
	bool disableCheckpoints;
};

internal struct
{
	i32 triggerCount;
	MvTrigger triggers[2048];
	i32 courseCount;
	MvCourseDescriptor courses[512];
} g_mappingApi;

internal MvTrigger *Mapi_NewTrigger(MvTriggerType type)
{
	// TODO: bounds checking
	assert(g_mappingApi.triggerCount < Q_ARRAYSIZE(g_mappingApi.triggers));
	MvTrigger *result = nullptr;
	if (g_mappingApi.triggerCount < Q_ARRAYSIZE(g_mappingApi.triggers))
	{
		result = &g_mappingApi.triggers[g_mappingApi.triggerCount++];
		*result = {};
		result->type = type;
	}
	return result;
}

internal MvCourseDescriptor *Mapi_NewCourse()
{
	// TODO: bounds checking
	assert(g_mappingApi.courseCount < Q_ARRAYSIZE(g_mappingApi.courses));
	MvCourseDescriptor *result = nullptr;
	if (g_mappingApi.courseCount < Q_ARRAYSIZE(g_mappingApi.courses))
	{
		result = &g_mappingApi.courses[g_mappingApi.courseCount++];
		*result = {};
	}
	return result;
}

internal void Mapi_OnTriggerMultipleSpawn(const CEntityKeyValues *ekv)
{
	MvTriggerType type = (MvTriggerType)ekv->GetInt(KEY_TRIGGER_TYPE, MVTRIGGER_DISABLED);

	if (type <= MVTRIGGER_DISABLED || type >= MVTRIGGER_COUNT)
	{
		// TODO: print error!
		assert(0);
		return;
	}
	MvTrigger *trigger = Mapi_NewTrigger(type);

	if (!trigger)
	{
		// TODO: print error!
		assert(0);
		return;
	}

	switch (type)
	{
		case MVTRIGGER_MODIFIER:
		{
			trigger->modifier.disablePausing = ekv->GetBool("timer_modifier_disable_pause");
			trigger->modifier.disableCheckpoints = ekv->GetBool("timer_modifier_disable_checkpoints");
			trigger->modifier.disableTeleports = ekv->GetBool("timer_modifier_disable_teleports");
			trigger->modifier.disableJumpstats = ekv->GetBool("timer_modifier_disable_jumpstats");
			trigger->modifier.enableSlide = ekv->GetBool("timer_modifier_enable_slide");
		}
		break;

		case MVTRIGGER_ANTI_BHOP:
		{
			trigger->antibhop.time = ekv->GetFloat("timer_anti_bhop_time");
		}
		break;

		case MVTRIGGER_ZONE_START:
		case MVTRIGGER_ZONE_END:
		case MVTRIGGER_ZONE_SPLIT:
		case MVTRIGGER_ZONE_CHECKPOINT:
		case MVTRIGGER_ZONE_STAGE:
		{
			// TODO: find course descriptor entity!
			const char *courseDescriptor = ekv->GetString("timer_zone_course_descriptor");
			V_snprintf(trigger->zone.courseDescriptor, sizeof(trigger->zone.courseDescriptor), "%s", courseDescriptor);

			if (type == MVTRIGGER_ZONE_STAGE)
			{
				trigger->stageZone.number = ekv->GetInt("timer_zone_stage_number", INVALID_STAGE_NUMBER);
			}
		}
		break;

		case MVTRIGGER_TELEPORT:
		case MVTRIGGER_MULTI_BHOP:
		case MVTRIGGER_SINGLE_BHOP:
		case MVTRIGGER_SEQUENTIAL_BHOP:
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

		// case MVTRIGGER_RESET_CHECKPOINTS:
		// case MVTRIGGER_SINGLE_BHOP_RESET:
		default:
			break;
	}

#if 0
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
}

internal void Mapi_OnInfoTargetSpawn(const CEntityKeyValues *ekv)
{
#if 1
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

	if (!ekv->GetBool(KEY_IS_COURSE_DESCRIPTOR))
	{
		return;
	}

	MvCourseDescriptor *course = Mapi_NewCourse();
	if (!course)
	{
		// TODO: error
		return;
	}

	const char *name = ekv->GetString("timer_course_name");
	V_snprintf(course->name, sizeof(course->name), "%s", name);
	course->disableCheckpoints = ekv->GetBool("timer_course_disable_checkpoint");
}

void mappingapi::Initialize()
{
	g_mappingApi.triggerCount = 0;
	g_mappingApi.courseCount = 0;
}

void mappingapi::OnSpawnPost(int count, const EntitySpawnInfo_t *info)
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
		Msg("spawned classname %s\n", classname);
		if (V_stricmp(classname, "trigger_multiple") == 0)
		{
			Mapi_OnTriggerMultipleSpawn(ekv);
		}
		else if (V_stricmp(classname, "info_target_server_only") == 0)
		{
			Mapi_OnInfoTargetSpawn(ekv);
		}
	}
}
