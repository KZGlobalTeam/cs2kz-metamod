
#pragma once

#define KZ_MAPPING_INTERFACE      "KZMappingInterface"
#define KZ_MAX_COURSE_NAME_LENGTH 128

struct KzCourseDescriptor
{
	char entityTargetname[128];
	char name[KZ_MAX_COURSE_NAME_LENGTH];
	bool disableCheckpoints;
};

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
