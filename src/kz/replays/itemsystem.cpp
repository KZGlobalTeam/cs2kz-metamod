#include "common.h"
#include "kz_replaysystem.h"
#include "sdk/entity/cbaseplayerweapon.h"
#include "sdk/cskeletoninstance.h"
#include "sdk/entity/ccsplayerpawn.h"

static_global std::unordered_map<u16, std::string> itemAttributes;
static_global std::unordered_map<u32, bool> paintKitUsesLegacyModel;

// Maps item definition index to weapon name and gear slot.

struct ItemData
{
	std::string name {};
	gear_slot_t gearSlot {};
};

static_global std::unordered_map<u16, ItemData> weaponItemMapping;

static_function gear_slot_t GetGearSlot(const char *name)
{
	if (KZ_STREQI(name, "primary"))
	{
		return gear_slot_t::GEAR_SLOT_RIFLE;
	}
	if (KZ_STREQI(name, "secondary"))
	{
		return gear_slot_t::GEAR_SLOT_PISTOL;
	}
	if (KZ_STREQI(name, "knife"))
	{
		return gear_slot_t::GEAR_SLOT_KNIFE;
	}
	if (KZ_STREQI(name, "grenade"))
	{
		return gear_slot_t::GEAR_SLOT_GRENADES;
	}
	if (KZ_STREQI(name, "item"))
	{
		return gear_slot_t::GEAR_SLOT_C4;
	}
	if (KZ_STREQI(name, "boost"))
	{
		return gear_slot_t::GEAR_SLOT_BOOSTS;
	}
	return gear_slot_t::GEAR_SLOT_INVALID;
}

void KZ::replaysystem::item::InitItemAttributes()
{
	KeyValues *kv = new KeyValues("items_game");
	kv->LoadFromFile(g_pFullFileSystem, "scripts/items/items_game.txt", "GAME");
	if (!kv)
	{
		Warning("[KZ] Failed to load items_game.txt for item attributes!\n");
		delete kv;
		return;
	}
	KeyValues *subKey = kv->FindKey("attributes");
	if (!subKey)
	{
		Warning("[KZ] Failed to find attributes key in items_game.txt!\n");
		delete kv;
		return;
	}
	for (KeyValues *sub = subKey->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey())
	{
		if (!sub->GetString("name")[0])
		{
			continue;
		}
		u16 id = atoi(sub->GetName());
		if (id == 0)
		{
			continue;
		}
		itemAttributes[id] = sub->GetString("name");
	}
	subKey = kv->FindKey("paint_kits");
	if (!subKey)
	{
		Warning("[KZ] Failed to find paint_kits key in items_game.txt!\n");
		delete kv;
		return;
	}
	for (KeyValues *sub = subKey->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey())
	{
		u32 id = atoi(sub->GetName());
		if (id == 0)
		{
			continue;
		}
		paintKitUsesLegacyModel[id] = sub->GetInt("use_legacy_model", 0) != 0;
	}
	subKey = kv->FindKey("items");
	KeyValues *prefabs = kv->FindKey("prefabs");
	if (!subKey)
	{
		Warning("[KZ] Failed to find items key in items_game.txt!\n");
		delete kv;
		return;
	}
	for (KeyValues *sub = subKey->GetFirstSubKey(); sub != NULL; sub = sub->GetNextKey())
	{
		if (!sub->GetString("name")[0])
		{
			continue;
		}
		u16 id = atoi(sub->GetName());
		if (id == 0)
		{
			continue;
		}
		if (!V_strstr(sub->GetString("name"), "weapon_"))
		{
			continue;
		}
		weaponItemMapping[id] = {sub->GetString("name"), gear_slot_t::GEAR_SLOT_INVALID};
		// Try to get "item_gear_slot" from the item, if not found, find it in the prefab.
		// Prefabs can be nested so we need to take that into account as well.
		const char *gearSlotStr = sub->GetString("item_gear_slot", "");
		KeyValues *current = sub;
		while (!gearSlotStr[0])
		{
			const char *parentPrefabName = current->GetString("prefab", "");
			if (!parentPrefabName[0])
			{
				break;
			}
			KeyValues *prefab = prefabs->FindKey(parentPrefabName);
			if (!prefab)
			{
				break;
			}
			gearSlotStr = prefab->GetString("item_gear_slot", "");
			current = prefab;
		}
		weaponItemMapping[id].gearSlot = GetGearSlot(gearSlotStr);
	}
	delete kv;
}

std::string KZ::replaysystem::item::GetItemAttributeName(u16 id)
{
	return itemAttributes[id];
}

std::string KZ::replaysystem::item::GetWeaponName(u16 id)
{
	std::string name = weaponItemMapping[id].name;
	if (V_strstr(name.c_str(), "weapon_knife_") || name == "weapon_bayonet")
	{
		return "weapon_knife";
	}
	return name;
}

gear_slot_t KZ::replaysystem::item::GetWeaponGearSlot(u16 id)
{
	return weaponItemMapping[id].gearSlot;
}

bool KZ::replaysystem::item::DoesPaintKitUseLegacyModel(float paintKit)
{
	return paintKitUsesLegacyModel[static_cast<u32>(paintKit)];
}

void KZ::replaysystem::item::ApplyItemAttributesToWeapon(CBasePlayerWeapon &weapon, const EconInfo &info)
{
	// Knives need subclass change to properly apply attributes.
	bool isKnife = false;
	if (KZ_STREQI(weapon.GetClassname(), "weapon_knife") || KZ_STREQI(weapon.GetClassname(), "weapon_knife_t"))
	{
		// Hacky way to change subclass, but it doesn't rely on fragile signatures.
		isKnife = true;
		char command[64];
		V_snprintf(command, sizeof(command), "subclass_change %i %i", info.mainInfo.itemDef, weapon.entindex());
		CCommandContext ctx(CT_FIRST_SPLITSCREEN_CLIENT, CPlayerSlot(0));
		CCommand cmd;
		cmd.Tokenize(command);
		g_pCVar->FindConCommand("subclass_change").Dispatch(ctx, cmd);
	}

	// Apply skin attributes
	CAttributeContainer &attrManager = weapon.m_AttributeManager();
	CEconItemView &item = attrManager.m_Item();
	CAttributeList &attributeList = item.m_NetworkedDynamicAttributes();
	item.m_iItemDefinitionIndex(info.mainInfo.itemDef);
	item.m_iEntityQuality(info.mainInfo.quality);
	item.m_iEntityLevel(info.mainInfo.level);
	item.m_iAccountID(info.mainInfo.accountID);
	item.m_iItemID(info.mainInfo.itemID);
	item.m_iItemIDHigh(info.mainInfo.itemID >> 32);
	item.m_iItemIDLow((u32)info.mainInfo.itemID & 0xFFFFFFFF);
	item.m_iInventoryPosition(info.mainInfo.inventoryPosition);
	strncpy(item.m_szCustomName(), info.mainInfo.customName, sizeof(info.mainInfo.customName));
	strncpy(item.m_szCustomNameOverride(), info.mainInfo.customNameOverride, sizeof(info.mainInfo.customNameOverride));

	for (int i = 0; i < info.mainInfo.numAttributes; i++)
	{
		int id = info.attributes[i].defIndex;
		float value = info.attributes[i].value;

		// Compatibility for paint kits on legacy models
		if (id == 6)
		{
			CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(weapon.m_CBodyComponent()->m_pSceneNode());
			bool usesLegacyModel = KZ::replaysystem::item::DoesPaintKitUseLegacyModel(value);
			if (!isKnife)
			{
				pSkeleton->m_modelState().m_MeshGroupMask(usesLegacyModel ? 2 : 1);
				pSkeleton->m_modelState().m_nBodyGroupChoices()->Element(0) = usesLegacyModel ? 1 : 0;
				pSkeleton->m_modelState().m_nBodyGroupChoices.NetworkStateChanged();
			}
			else
			{
				pSkeleton->m_modelState().m_MeshGroupMask(-1);
			}
		}
		g_pKZUtils->SetOrAddAttributeValueByName(&item.m_AttributeList(), KZ::replaysystem::item::GetItemAttributeName(id).c_str(), value);
		g_pKZUtils->SetOrAddAttributeValueByName(&attributeList, KZ::replaysystem::item::GetItemAttributeName(id).c_str(), value);
	}
}

void KZ::replaysystem::item::ApplyGloveAttributesToPawn(CCSPlayerPawn *pawn, const EconInfo &info)
{
	CEconItemView &item = pawn->m_EconGloves();
	item.m_iItemDefinitionIndex(info.mainInfo.itemDef);
	item.m_iEntityQuality(info.mainInfo.quality);
	item.m_iEntityLevel(info.mainInfo.level);
	item.m_iAccountID(info.mainInfo.accountID);
	item.m_iItemID(info.mainInfo.itemID);
	item.m_iItemIDHigh(info.mainInfo.itemID >> 32);
	item.m_iItemIDLow((u32)info.mainInfo.itemID & 0xFFFFFFFF);
	item.m_iInventoryPosition(info.mainInfo.inventoryPosition);
	item.m_bInitialized = true;
	strncpy(item.m_szCustomName(), info.mainInfo.customName, sizeof(info.mainInfo.customName));
	strncpy(item.m_szCustomNameOverride(), info.mainInfo.customNameOverride, sizeof(info.mainInfo.customNameOverride));
	CAttributeList &attributeList = item.m_NetworkedDynamicAttributes();
	for (int i = 0; i < info.mainInfo.numAttributes; i++)
	{
		int id = info.attributes[i].defIndex;
		float value = info.attributes[i].value;
		g_pKZUtils->SetOrAddAttributeValueByName(&item.m_AttributeList(), KZ::replaysystem::item::GetItemAttributeName(id).c_str(), value);
		g_pKZUtils->SetOrAddAttributeValueByName(&attributeList, KZ::replaysystem::item::GetItemAttributeName(id).c_str(), value);
	}
}

void KZ::replaysystem::item::ApplyModelAttributesToPawn(CCSPlayerPawn *pawn, const char *modelName)
{
	CSkeletonInstance *pSkeleton = static_cast<CSkeletonInstance *>(pawn->m_CBodyComponent()->m_pSceneNode());
	g_pKZUtils->SetModel(pawn, modelName);
	u64 mask = pSkeleton->m_modelState().m_MeshGroupMask() + 1;
	pSkeleton->m_modelState().m_MeshGroupMask(mask);
}
