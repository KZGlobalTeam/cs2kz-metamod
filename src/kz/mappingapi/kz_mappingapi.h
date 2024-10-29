
#pragma once

#define KZ_MAPPING_INTERFACE "KZMappingInterface"

#define KZ_NO_MAPAPI_VERSION           -1
#define KZ_NO_MAPAPI_COURSE_DESCRIPTOR "Default"
#define KZ_NO_MAPAPI_COURSE_NAME       "Main"

#define KZ_MAPAPI_VERSION 2

#define KZ_MAX_SPLIT_ZONES      100
#define KZ_MAX_CHECKPOINT_ZONES 100
#define KZ_MAX_STAGE_ZONES      100

#define INVALID_SPLIT_NUMBER      0
#define INVALID_CHECKPOINT_NUMBER 0
#define INVALID_STAGE_NUMBER      0
#define INVALID_COURSE_NUMBER     0

struct KZCourse;
class KZPlayer;

enum KzTriggerType
{
	KZTRIGGER_DISABLED = 0,
	KZTRIGGER_MODIFIER,
	KZTRIGGER_RESET_CHECKPOINTS,
	KZTRIGGER_SINGLE_BHOP_RESET,
	KZTRIGGER_ANTI_BHOP,

	KZTRIGGER_ZONE_START,
	KZTRIGGER_ZONE_END,
	KZTRIGGER_ZONE_SPLIT,
	KZTRIGGER_ZONE_CHECKPOINT,
	KZTRIGGER_ZONE_STAGE,

	KZTRIGGER_TELEPORT,
	KZTRIGGER_MULTI_BHOP,
	KZTRIGGER_SINGLE_BHOP,
	KZTRIGGER_SEQUENTIAL_BHOP,
	KZTRIGGER_COUNT,
};

// KZTRIGGER_MODIFIER
struct KzMapModifier
{
	bool disablePausing;
	bool disableCheckpoints;
	bool disableTeleports;
	bool disableJumpstats;
	bool enableSlide;
};

// KZTRIGGER_ANTI_BHOP
struct KzMapAntibhop
{
	f32 time;
};

// KZTRIGGER_ZONE_*
struct KzMapZone
{
	char courseDescriptor[128];
	i32 number; // not used on start/end zones
};

// KZTRIGGER_TELEPORT/_MULTI_BHOP/_SINGLE_BHOP/_SEQUENTIAL_BHOP
struct KzMapTeleport
{
	char destination[128];
	f32 delay;
	bool useDestinationAngles;
	bool resetSpeed;
	bool reorientPlayer;
	bool relative;
};

struct KZCourseDescriptor
{
	KZCourseDescriptor(KZCourse *course, i32 hammerId = -1, const char *targetName = "", bool disableCheckpoints = false)
		: course(course), hammerId(hammerId), disableCheckpoints(disableCheckpoints)
	{
		V_snprintf(entityTargetname, sizeof(entityTargetname), "%s", targetName);
	}

	KZCourseDescriptor() = default;

	KZCourse *course = nullptr;
	char entityTargetname[128] {};
	i32 hammerId = -1;
	bool disableCheckpoints = false;

	bool hasStartPosition = false;
	Vector startPosition;
	QAngle startAngles;

	void SetStartPosition(Vector origin, QAngle angles)
	{
		hasStartPosition = true;
		startPosition = origin;
		startAngles = angles;
	}

	bool hasEndPosition = false;
	Vector endPosition;
	QAngle endAngles;

	i32 splitCount {};
	i32 checkpointCount {};
	i32 stageCount {};
};

struct KzTrigger
{
	KzTriggerType type;
	CEntityHandle entity;
	i32 hammerId;

	union
	{
		KzMapModifier modifier;
		KzMapAntibhop antibhop;
		KzMapZone zone;
		KzMapTeleport teleport;
	};
};

namespace KZ::mapapi
{
	// These namespace'd functions are called when relevant game events happen, and are somewhat in order.
	void Init();
	void OnCreateLoadingSpawnGroupHook(const CUtlVector<const CEntityKeyValues *> *pKeyValues);
	void OnRoundPrestart();
	void OnSpawn(int count, const EntitySpawnInfo_t *info);
	void OnRoundStart();

	void CheckEndTimerTrigger(CBaseTrigger *trigger);
	// This is const, unlike the trigger returned from Mapi_FindKzTrigger.
	const KzTrigger *GetKzTrigger(CBaseTrigger *trigger);

	const KZCourseDescriptor *GetCourseDescriptorFromTrigger(CBaseTrigger *trigger);
	const KZCourseDescriptor *GetCourseDescriptorFromTrigger(const KzTrigger *trigger);

	inline bool IsBhopTrigger(KzTriggerType triggerType)
	{
		return triggerType == KZTRIGGER_MULTI_BHOP || triggerType == KZTRIGGER_SINGLE_BHOP || triggerType == KZTRIGGER_SEQUENTIAL_BHOP;
	}

	inline bool IsTimerTrigger(KzTriggerType triggerType)
	{
		static_assert(KZTRIGGER_ZONE_START == 5 && KZTRIGGER_ZONE_STAGE == 9,
					  "Don't forget to change this function when changing the KzTriggerType enum!!!");
		return triggerType >= KZTRIGGER_ZONE_START && triggerType <= KZTRIGGER_ZONE_STAGE;
	}
} // namespace KZ::mapapi

// Exposed interface to modes.
class MappingInterface
{
public:
	virtual bool IsTriggerATimerZone(CBaseTrigger *trigger);
};

extern MappingInterface *g_pMappingApi;
