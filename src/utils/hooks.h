#include "common.h"
#include "interfaces.h"
#include "igameevents.h"
#include "iserver.h"

namespace hooks
{
	inline CUtlVector<int> entityTouchHooks;

	void Initialize();
	void Cleanup();
	void HookEntities();
	void LoadAddons();
}

extern IMultiAddonManager *g_IMultiAddonManager;
