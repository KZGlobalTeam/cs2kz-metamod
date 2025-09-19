#pragma once
#include "sdk/entity/cbaseplayerweapon.h"
#include "utils.h"

struct EconInfo
{
	struct MainInfo
	{
		i32 itemDef {};
		i32 quality {};
		i32 level = 1;
		i32 accountID {};
		i64 itemID {};
		i32 inventoryPosition {};
		char customName[161] {};
		char customNameOverride[161] {};
		int numAttributes {};
	} mainInfo;

	// TODO: dynamic array
	struct
	{
		i32 defIndex;
		f32 value;
	} attributes[32];

	EconInfo() = default;

	EconInfo(CBasePlayerWeapon *weapon)
	{
		if (!weapon)
		{
			return;
		}
		CAttributeContainer &attrManager = weapon->m_AttributeManager();
		CEconItemView &item = attrManager.m_Item();
		mainInfo.itemDef = item.m_iItemDefinitionIndex();
		mainInfo.quality = item.m_iEntityQuality();
		mainInfo.level = item.m_iEntityLevel();
		mainInfo.accountID = item.m_iAccountID();
		mainInfo.itemID = item.m_iItemID();
		mainInfo.inventoryPosition = item.m_iInventoryPosition();
		strncpy(mainInfo.customName, item.m_szCustomName(), sizeof(mainInfo.customName));
		strncpy(mainInfo.customNameOverride, item.m_szCustomNameOverride(), sizeof(mainInfo.customNameOverride));
		CAttributeList &attributeList = item.m_NetworkedDynamicAttributes();
		mainInfo.numAttributes = attributeList.m_Attributes->Count();
		FOR_EACH_VEC(*attributeList.m_Attributes(), i)
		{
			CEconItemAttribute &attr = (*attributeList.m_Attributes())[i];
			attributes[i] = {attr.m_iAttributeDefinitionIndex(), attr.m_flValue()};
		}
	}

	bool operator==(const EconInfo &other) const
	{
		if (mainInfo.itemDef != other.mainInfo.itemDef || mainInfo.quality != other.mainInfo.quality || mainInfo.level != other.mainInfo.level
			|| mainInfo.accountID != other.mainInfo.accountID || mainInfo.itemID != other.mainInfo.itemID
			|| mainInfo.inventoryPosition != other.mainInfo.inventoryPosition)
		{
			return false;
		}
		if (strncmp(mainInfo.customName, other.mainInfo.customName, sizeof(mainInfo.customName)) != 0
			|| strncmp(mainInfo.customNameOverride, other.mainInfo.customNameOverride, sizeof(mainInfo.customNameOverride)) != 0)
		{
			return false;
		}
		if (mainInfo.numAttributes != other.mainInfo.numAttributes)
		{
			return false;
		}
		for (size_t i = 0; i < mainInfo.numAttributes; i++)
		{
			if (attributes[i].defIndex != other.attributes[i].defIndex || attributes[i].value != other.attributes[i].value)
			{
				return false;
			}
		}
		return true;
	}

	bool operator!=(const EconInfo &other) const
	{
		return !(*this == other);
	}

	bool operator==(CBasePlayerWeapon *weapon)
	{
		if (!weapon)
		{
			return *this == EconInfo();
		}
		CAttributeContainer &attrManager = weapon->m_AttributeManager();
		CEconItemView &item = attrManager.m_Item();
		if (mainInfo.itemDef != item.m_iItemDefinitionIndex())
		{
			return false;
		}
		if (mainInfo.quality != item.m_iEntityQuality())
		{
			return false;
		}
		if (mainInfo.level != item.m_iEntityLevel())
		{
			return false;
		}
		if (mainInfo.accountID != item.m_iAccountID())
		{
			return false;
		}
		if (mainInfo.itemID != item.m_iItemID())
		{
			return false;
		}
		if (mainInfo.inventoryPosition != item.m_iInventoryPosition())
		{
			return false;
		}
		if (!KZ_STREQI(mainInfo.customName, item.m_szCustomName()))
		{
			return false;
		}
		if (!KZ_STREQI(mainInfo.customNameOverride, item.m_szCustomNameOverride()))
		{
			return false;
		}
		if (mainInfo.numAttributes != item.m_NetworkedDynamicAttributes().m_Attributes->Count())
		{
			return false;
		}
		for (size_t i = 0; i < mainInfo.numAttributes; i++)
		{
			if (attributes[i].defIndex != item.m_NetworkedDynamicAttributes().m_Attributes->Element(i).m_iAttributeDefinitionIndex()
				|| attributes[i].value != item.m_NetworkedDynamicAttributes().m_Attributes->Element(i).m_flValue())
			{
				return false;
			}
		}
		return true;
	}
};
