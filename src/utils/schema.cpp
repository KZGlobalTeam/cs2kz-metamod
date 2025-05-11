#include "common.h"

#include "schema.h"
#include "schemasystem/schemasystem.h"
#include "utils/interfaces.h"
// #include <unordered_map>
#include "tier1/utlmap.h"
#include "plat.h"
#include "sdk/entity/cbaseentity.h"

#include "tier0/memdbgon.h"
using SchemaKeyValueMap_t = CUtlMap<uint32_t, SchemaKey>;
using SchemaTableMap_t = CUtlMap<uint32_t, SchemaKeyValueMap_t *>;

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

static bool InitSchemaFieldsForClass(SchemaTableMap_t *tableMap, const char *className, uint32_t classKey)
{
	CSchemaSystemTypeScope *pType = g_pSchemaSystem->FindTypeScopeForModule(MODULE_PREFIX "server" MODULE_EXT);

	if (!pType)
	{
		return false;
	}

	SchemaClassInfoData_t *pClassInfo = pType->FindDeclaredClass(className).Get();

	if (!pClassInfo)
	{
		SchemaKeyValueMap_t *map = new SchemaKeyValueMap_t(0, 0, DefLessFunc(uint32_t));
		tableMap->Insert(classKey, map);

		Warning("InitSchemaFieldsForClass(): '%s' was not found!\n", className);
		return false;
	}

	short fieldsSize = pClassInfo->m_nFieldCount;
	short dataNumFields = pClassInfo->m_pDataDescMap ? pClassInfo->m_pDataDescMap->dataNumFields : 0;
	SchemaClassFieldData_t *pFields = pClassInfo->m_pFields;
	SchemaKeyValueMap_t *keyValueMap = new SchemaKeyValueMap_t(0, 0, DefLessFunc(uint32_t));
	keyValueMap->EnsureCapacity(fieldsSize + dataNumFields);
	tableMap->Insert(classKey, keyValueMap);

	for (int i = 0; i < dataNumFields; ++i)
	{
		auto &field = pClassInfo->m_pDataDescMap->dataDesc[i];

		if (!field.fieldName || !field.fieldName[0] || field.fieldOffset < 0)
		{
			continue;
		}
		keyValueMap->Insert(hash_32_fnv1a_const(field.fieldName), {field.fieldOffset, false});
	}

	for (int i = 0; i < fieldsSize; ++i)
	{
		SchemaClassFieldData_t &field = pFields[i];

#ifdef CS2_SDK_ENABLE_SCHEMA_FIELD_OFFSET_LOGGING
		Msg("%s::%s found at -> 0x%X - %llx\n", className, field.m_pszName, field.m_nSingleInheritanceOffset, &field);
#endif

		keyValueMap->Insert(hash_32_fnv1a_const(field.m_pszName), {field.m_nSingleInheritanceOffset, IsFieldNetworked(field)});
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
	static SchemaTableMap_t schemaTableMap(0, 0, DefLessFunc(uint32_t));
	int16_t tableMapIndex = schemaTableMap.Find(classKey);
	if (!schemaTableMap.IsValidIndex(tableMapIndex))
	{
		if (InitSchemaFieldsForClass(&schemaTableMap, className, classKey))
		{
			return GetOffset(className, classKey, memberName, memberKey);
		}

		return {0, 0};
	}

	SchemaKeyValueMap_t *tableMap = schemaTableMap[tableMapIndex];
	int16_t memberIndex = tableMap->Find(memberKey);
	if (!tableMap->IsValidIndex(memberIndex))
	{
		Warning("schema::GetOffset(): '%s' was not found in '%s'!\n", memberName, className);
		return {0, 0};
	}

	return tableMap->Element(memberIndex);
}

void schema::NetworkStateChanged(int64 chainEntity, uint32 nLocalOffset, int nArrayIndex)
{
	CNetworkVarChainer *chainEnt = reinterpret_cast<CNetworkVarChainer *>(chainEntity);
	CEntityInstance *entity = chainEnt->GetObject();
	if (entity && !(entity->m_pEntity->m_flags & EF_IS_CONSTRUCTION_IN_PROGRESS))
	{
		entity->NetworkStateChanged(nLocalOffset, nArrayIndex, chainEnt->m_PathIndex.m_Value);
	}
}
