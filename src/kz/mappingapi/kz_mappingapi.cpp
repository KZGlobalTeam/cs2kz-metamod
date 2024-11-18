/*
	Keeps track of course descriptors along with various types of triggers, applying effects to player when necessary.
*/

#include "kz/kz.h"
#include "kz/course/kz_course.h"
#include "kz/mode/kz_mode.h"
#include "kz/trigger/kz_trigger.h"
#include "movement/movement.h"
#include "kz_mappingapi.h"
#include "entity2/entitykeyvalues.h"
#include "sdk/entity/cbasetrigger.h"
#include "utils/ctimer.h"

#include "tier0/memdbgon.h"

#define KEY_TRIGGER_TYPE         "timer_trigger_type"
#define KEY_IS_COURSE_DESCRIPTOR "timer_course_descriptor"

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

// TODO: add error check to make sure a course has at least 1 start zone and 1 end zone

static_function void Mapi_Error(const char *format, ...)
{
	i32 errorIndex = g_mappingApi.errorCount;
	if (errorIndex >= Q_ARRAYSIZE(g_mappingApi.errors))
	{
		return;
	}
	else if (errorIndex == Q_ARRAYSIZE(g_mappingApi.errors) - 1)
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

	// Attempt to register a new course using course number and name.
	KZCourse *course = KZ::course::InsertCourse(courseNumber, courseName);
	if (!course)
	{
		Mapi_Error("Failed to register course name '%s' (hammerId %i)", courseName, hammerId);
		return false;
	}
	// Check if the course already has a descriptor. This should not happen.
	if (course->descriptor)
	{
		// Courses made with hammerId -1 is from pre-mapping API, which is fine.
		if (course->descriptor->hammerId == -1)
		{
			return false;
		}
		assert(0);
		Mapi_Error("Course name '%s' already had a descriptor!", courseName);
		return false;
	}
	i32 index = g_mappingApi.courseDescriptors.AddToTail({course, hammerId, targetName, disableCheckpoints});
	course->descriptor = &g_mappingApi.courseDescriptors[index];
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
			Mapi_Error("Course \"%s\" Split zones aren't consecutive or don't start at 1!", courseDescriptor->course->name);
			invalid = true;
		}

		if (cpXor != 0)
		{
			Mapi_Error("Course \"%s\" Checkpoint zones aren't consecutive or don't start at 1!", courseDescriptor->course->name);
			invalid = true;
		}

		if (stageXor != 0)
		{
			Mapi_Error("Course \"%s\" Stage zones aren't consecutive or don't start at 1!", courseDescriptor->course->name);
			invalid = true;
		}

		if (splitCount > KZ_MAX_SPLIT_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many split zones! Maximum is %i.", courseDescriptor->course->name, KZ_MAX_SPLIT_ZONES);
			invalid = true;
		}

		if (cpCount > KZ_MAX_CHECKPOINT_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many checkpoint zones! Maximum is %i.", courseDescriptor->course->name, KZ_MAX_CHECKPOINT_ZONES);
			invalid = true;
		}

		if (stageCount > KZ_MAX_STAGE_ZONES)
		{
			Mapi_Error("Course \"%s\" Too many stage zones! Maximum is %i.", courseDescriptor->course->name, KZ_MAX_STAGE_ZONES);
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
