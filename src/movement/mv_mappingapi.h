#pragma once

namespace mappingapi
{
	bool IsTriggerATimerZone(CBaseTrigger *trigger);
	void Initialize();
	void OnSpawnPost(int count, const EntitySpawnInfo_t *info);
	void OnTriggerMultipleStartTouchPost(KZPlayer *player, CBaseTrigger *trigger);
	void OnTriggerMultipleEndTouchPost(KZPlayer *player, CBaseTrigger *trigger);
} // namespace mappingapi
