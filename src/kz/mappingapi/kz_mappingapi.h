
#pragma once

#define KZ_MAPPING_INTERFACE "KZMappingInterface"

class MappingInterface
{
public:
	virtual bool IsTriggerATimerZone(CBaseTrigger *trigger);
	virtual void Initialize();
	virtual void OnSpawnPost(int count, const EntitySpawnInfo_t *info);
	virtual void OnTriggerMultipleStartTouchPost(KZPlayer *player, CBaseTrigger *trigger);
	virtual void OnTriggerMultipleEndTouchPost(KZPlayer *player, CBaseTrigger *trigger);
};

extern MappingInterface g_mappingInterface;
