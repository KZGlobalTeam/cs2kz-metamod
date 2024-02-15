
#include "movement.h"
#include "mv_mappingapi.h"
// #include "utils/utils.h"
#include "entity2/entitykeyvalues.h"

#include "tier0/memdbgon.h"

#define TRIGGER_TYPE_KEY "timer_trigger_type"

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

internal class
{
public:
	void OnMappingTriggerSpawn(MvTriggerType type, const CEntityKeyValues *ekv)
	{
		
	}
private:
	
} g_mappingApi;

void mappingapi::OnSpawn(int nCount, const EntitySpawnInfo_t *pInfo)
{
	if (!pInfo)
	{
		return;
	}
	
	for (i32 i = 0; i < nCount; i++)
	{
		if (!pInfo[i].m_pEntity || !pInfo->m_pKeyValues)
		{
			continue;
		}
		const char *classname = pInfo[i].m_pEntity->m_designerName.String();
		// Msg("classname: %s\n", classname);
		if (!classname || stricmp(classname, "trigger_multiple") != 0)
		{
			continue;
		}
		
		auto ekv = pInfo->m_pKeyValues;
		FOR_EACH_ENTITYKEY(ekv, iter)
		{
			auto kv = ekv->GetKeyValue(iter);
			if (!kv)
			{
				continue;
			}
			CBufferStringGrowable<128> bufferStr;
			// Msg("\t%s: %s\n", ekv->GetEntityKeyId(iter).GetString(), value);
			const char *key = ekv->GetEntityKeyId(iter).GetString();
			const char *value = kv->ToString(bufferStr);
			
			if (strncmp(key, TRIGGER_TYPE_KEY, sizeof(TRIGGER_TYPE_KEY)) != 0)
			{
				continue;
			}
			
			MvTriggerType type = (MvTriggerType)V_StringToInt32(value, MVTRIGGER_DISABLED);
			if (type > MVTRIGGER_DISABLED && type < MVTRIGGER_COUNT)
			{
				g_mappingApi.OnMappingTriggerSpawn(type, pInfo->m_pKeyValues);
			}
		}
	}
}
