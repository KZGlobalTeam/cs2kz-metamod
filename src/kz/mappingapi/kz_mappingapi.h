
#pragma once

#define KZ_MAPPING_INTERFACE      "KZMappingInterface"
#define KZ_MAX_COURSE_NAME_LENGTH 65

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
};

// KZTRIGGER_ZONE_STAGE
struct KzMapStageZone
{
	char courseDescriptor[128];
	i32 stageNumber;
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

struct KzCourseDescriptor
{
	char entityTargetname[128];
	char name[KZ_MAX_COURSE_NAME_LENGTH];
	i32 number;
	i32 hammerId;
	bool disableCheckpoints;
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
		KzMapStageZone stageZone;
		KzMapTeleport teleport;
	};
};

class KZPlayer;

class MappingInterface
{
public:
	virtual bool IsTriggerATimerZone(CBaseTrigger *trigger);
	virtual void OnSpawnPost(int count, const EntitySpawnInfo_t *info);
	virtual void OnTriggerMultipleStartTouchPost(KZPlayer *player, CBaseTrigger *trigger);
	virtual void OnTriggerMultipleEndTouchPost(KZPlayer *player, CBaseTrigger *trigger);
};

void Mappingapi_Initialize();

extern MappingInterface *g_pMappingApi;
