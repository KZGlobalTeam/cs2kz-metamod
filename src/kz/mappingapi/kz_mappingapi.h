
#pragma once

#define KZ_MAPPING_INTERFACE "KZMappingInterface"

#define KZ_NO_MAPAPI_VERSION           0
#define KZ_NO_MAPAPI_COURSE_DESCRIPTOR "Default"
#define KZ_NO_MAPAPI_COURSE_NAME       "Main"

#define KZ_MAPAPI_VERSION 2

#define KZ_MAX_SPLIT_ZONES        100
#define KZ_MAX_CHECKPOINT_ZONES   100
#define KZ_MAX_STAGE_ZONES        100
#define KZ_MAX_COURSE_COUNT       128
#define KZ_MAX_COURSE_NAME_LENGTH 65

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

	KZTRIGGER_PUSH,
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
	f32 gravity;
	f32 jumpFactor;
	bool forceDuck;
	bool forceUnduck;
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

// KZTRIGGER_PUSH
struct KzMapPush
{
	// Cannot use Vector here as it is not a POD type.
	f32 impulse[3];

	enum KzMapPushCondition : u32
	{
		KZ_PUSH_START_TOUCH = 1,
		KZ_PUSH_TOUCH = 2,
		KZ_PUSH_END_TOUCH = 4,
		KZ_PUSH_JUMP_EVENT = 8,
		KZ_PUSH_JUMP_BUTTON = 16,
		KZ_PUSH_ATTACK = 32,
		KZ_PUSH_ATTACK2 = 64,
		KZ_PUSH_USE = 128,
	};

	u32 pushConditions;
	bool setSpeed[3];
	bool cancelOnTeleport;
	f32 cooldown;
	f32 delay;
};

struct KZCourseDescriptor

{
	KZCourseDescriptor(i32 hammerId = -1, const char *targetName = "", bool disableCheckpoints = false, u32 guid = 0,
					   i32 courseID = INVALID_COURSE_NUMBER, const char *courseName = "")
		: hammerId(hammerId), disableCheckpoints(disableCheckpoints), guid(guid), id(courseID)
	{
		V_snprintf(entityTargetname, sizeof(entityTargetname), "%s", targetName);
		V_snprintf(name, sizeof(name), "%s", courseName);
	}

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

	// Shared identifiers
	u32 guid {};

	// Mapper assigned course ID.
	i32 id;
	// Mapper assigned course name.
	char name[KZ_MAX_COURSE_NAME_LENGTH] {};

	bool HasMatchingIdentifiers(i32 id, const char *name) const
	{
		return this->id == id && (!V_stricmp(this->name, name));
	}

	CUtlString GetName() const
	{
		return name;
	}

	// ID used for local database.
	u32 localDatabaseID {};

	// ID used for global database.
	u32 globalDatabaseID {};
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
		KzMapPush push;
	};
};

namespace KZ::mapapi
{
	// These namespace'd functions are called when relevant game events happen, and are somewhat in order.
	void Init();
	void OnCreateLoadingSpawnGroupHook(const CUtlVector<const CEntityKeyValues *> *pKeyValues);
	void OnSpawn(int count, const EntitySpawnInfo_t *info);
	void OnRoundPreStart();
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

	inline bool IsPushTrigger(KzTriggerType triggerType)
	{
		return triggerType == KZTRIGGER_PUSH;
	}
} // namespace KZ::mapapi

// Exposed interface to modes.
class MappingInterface
{
public:
	virtual bool IsTriggerATimerZone(CBaseTrigger *trigger);
	bool GetJumpstatArea(Vector &pos, QAngle &angles);
};

extern MappingInterface *g_pMappingApi;

namespace KZ::course
{
	// Clear the list of current courses.
	void ClearCourses();

	// Get the number of courses on this map.
	u32 GetCourseCount();

	// Get a course's information given its map-defined course id.
	const KZCourseDescriptor *GetCourseByCourseID(i32 id);

	// Get a course's information given its local course id.
	const KZCourseDescriptor *GetCourseByLocalCourseID(u32 id);

	// Get a course's information given its global course id.
	const KZCourseDescriptor *GetCourseByGlobalCourseID(u32 id);

	// Get a course's information given its name.
	const KZCourseDescriptor *GetCourse(const char *courseName, bool caseSensitive = true);

	// Get a course's information given its GUID.
	const KZCourseDescriptor *GetCourse(u32 guid);

	// Get the first course's information sorted by map-defined ID.
	const KZCourseDescriptor *GetFirstCourse();

	// Setup all the courses to the local database.
	void SetupLocalCourses();

	// Update the course's database ID given its name.
	bool UpdateCourseLocalID(const char *courseName, u32 databaseID);

	// Update the course's global ID given its map-defined name and ID.
	bool UpdateCourseGlobalID(const char *courseName, u32 globalID);

}; // namespace KZ::course
