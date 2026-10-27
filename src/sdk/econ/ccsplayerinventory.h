#pragma once
#include "utils/schema.h"
#include "utlmap.h"
#include "utils/virtual.h"
#include "gcsdk/soid.h"

class CEconItemView;

typedef uint64 itemid_t;

struct CCSPlayerInventory
{
	void **__vtable;
	int m_nTargetRecipe;
	GCSDK::SOID_t m_OwnerID;

	struct
	{
		CUtlVector<CEconItemView *> m_vecInventoryItems;
		CUtlOrderedMap<itemid_t, CEconItemView *> m_mapItemIDToItemView;
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
