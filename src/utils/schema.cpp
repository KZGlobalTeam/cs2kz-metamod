#include "common.h"

#include "schema.h"
#include "schemasystem/schemasystem.h"
#include "utils/interfaces.h"
// #include <unordered_map>
#include "tier1/utlmap.h"
#include "plat.h"
#include "sdk/entity/cbaseentity.h"

#include "tier0/memdbgon.h"
using SchemaKeyValueMap_t = std::map<uint32_t, SchemaKey>;
using SchemaTableMap_t = std::map<uint32_t, SchemaKeyValueMap_t>;

static bool IsFieldNetworked(SchemaClassFieldData_t &field)
{
	for (int i = 0; i < field.m_nStaticMetadataCount; i++)
	{
		static auto networkEnabled = hash_32_fnv1a_const("MNetworkEnable");
		if (networkEnabled == hash_32_fnv1a_const(field.m_pStaticMetadata[i].m_pszName))
		{
			return true;
		}
	}

	return false;
}

static bool InitSchemaFieldsForClass(SchemaTableMap_t &tableMap, const char *className, uint32_t classKey)
{
	CSchemaSystemTypeScope *pType = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pType)
	{
		return false;
	}

	SchemaClassInfoData_t *pClassInfo = pType->FindDeclaredClass(className).Get();

	if (!pClassInfo)
	{
		SchemaKeyValueMap_t map;
		tableMap.insert(std::make_pair(classKey, map));

		Warning("InitSchemaFieldsForClass(): '%s' was not found!\n", className);
		return false;
	}

	short fieldsSize = pClassInfo->m_nFieldCount;
	short dataNumFields = pClassInfo->m_pDataDescMap ? pClassInfo->m_pDataDescMap->dataNumFields : 0;
	SchemaClassFieldData_t *pFields = pClassInfo->m_pFields;
	SchemaKeyValueMap_t &keyValueMap = tableMap.insert(std::make_pair(classKey, SchemaKeyValueMap_t())).first->second;

	for (int i = 0; i < fieldsSize; ++i)
	{
		SchemaClassFieldData_t &field = pFields[i];

#ifdef CS2_SDK_ENABLE_SCHEMA_FIELD_OFFSET_LOGGING
		Msg("%s::%s found at -> 0x%X (networked = %i) - %llx\n", className, field.m_pszName, field.m_nSingleInheritanceOffset,
			IsFieldNetworked(field), &field);
#endif

		std::pair<uint32_t, SchemaKey> keyValuePair;
		keyValuePair.first = hash_32_fnv1a_const(field.m_pszName);
		keyValuePair.second.offset = field.m_nSingleInheritanceOffset;
		keyValuePair.second.networked = IsFieldNetworked(field);

		keyValueMap.insert(keyValuePair);
	}

	for (int i = 0; i < dataNumFields; ++i)
	{
		auto &field = pClassInfo->m_pDataDescMap->dataDesc[i];

		if (!field.fieldName || !field.fieldName[0] || field.fieldOffset < 0)
		{
			continue;
		}
		uint32_t hashKey = hash_32_fnv1a_const(field.fieldName);
		if (keyValueMap.find(hashKey) != keyValueMap.end())
		{
			continue;
		}

		std::pair<uint32_t, SchemaKey> newEntry;
		newEntry.first = hashKey;
		newEntry.second.offset = field.fieldOffset;
		newEntry.second.networked = false;
		keyValueMap.insert(newEntry);
	}
	return true;
}

int16_t schema::FindChainOffset(const char *className)
{
	CSchemaSystemTypeScope *pType = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pType)
	{
		return false;
	}

	SchemaClassInfoData_t *pClassInfo = pType->FindDeclaredClass(className).Get();

	do
	{
		SchemaClassFieldData_t *pFields = pClassInfo->m_pFields;
		short fieldsSize = pClassInfo->m_nFieldCount;
		for (int i = 0; i < fieldsSize; ++i)
		{
			SchemaClassFieldData_t &field = pFields[i];

			if (V_strcmp(field.m_pszName, "__m_pChainEntity") == 0)
			{
				return field.m_nSingleInheritanceOffset;
			}
		}
	} while ((pClassInfo = pClassInfo->m_pBaseClasses ? pClassInfo->m_pBaseClasses->m_pClass : nullptr) != nullptr);

	return 0;
}

SchemaKey schema::GetOffset(const char *className, uint32_t classKey, const char *memberName, uint32_t memberKey)
{
	static SchemaTableMap_t schemaTableMap;

	if (schemaTableMap.find(classKey) == schemaTableMap.end())
	{
		if (InitSchemaFieldsForClass(schemaTableMap, className, classKey))
		{
			return GetOffset(className, classKey, memberName, memberKey);
		}

		return {0, 0};
	}

	SchemaKeyValueMap_t tableMap = schemaTableMap[classKey];

	if (tableMap.find(memberKey) == tableMap.end())
	{
		Warning("schema::GetOffset(): '%s' was not found in '%s'!\n", memberName, className);
		return {0, 0};
	}

	return tableMap[memberKey];
}

void schema::NetworkStateChanged(int64 chainEntity, uint32 nLocalOffset, int32 nArrayIndex)
{
	CNetworkVarChainer *chainEnt = reinterpret_cast<CNetworkVarChainer *>(chainEntity);
	CEntityInstance *entity = chainEnt->GetObject();
	if (entity && !(entity->m_pEntity->m_flags & EF_IS_CONSTRUCTION_IN_PROGRESS))
	{
		NetworkStateChangedData data(nLocalOffset, nArrayIndex, chainEnt->m_PathIndex);
		entity->NetworkStateChanged(data);
	}
}
