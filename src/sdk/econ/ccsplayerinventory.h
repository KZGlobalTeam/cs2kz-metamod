#pragma once
#include "utils/schema.h"
#include "utlmap.h"
#include "utils/virtual.h"

class CEconItemView;

typedef uint64 itemid_t;

struct SOID_t
{
	uint64 m_id; // CSteamID
	uint32 m_type;
	uint32 m_padding;
};

struct CCSPlayerInventory
{
	void **__vtable;
	int m_nTargetRecipe;
	SOID_t m_OwnerID;

	struct
	{
		CUtlVector<CEconItemView *> m_vecInventoryItems;
		CUtlMap<itemid_t, CEconItemView *, int, CDefLess<itemid_t>> m_mapItemIDToItemView;
	} m_Items;

	int m_iPendingRequests;
	bool m_bGotItemsFromSteam;
	bool m_bHasTestItems;
	bool m_bIsListeningToSOCache;
	void *m_pSOCache;

	CUtlVector<void *> m_vecListeners;

	int m_nActiveQuestID;

	struct
	{
		itemid_t itemID;
		uint16 definitionIndex;
	} m_loadoutItems[4][57];
};
