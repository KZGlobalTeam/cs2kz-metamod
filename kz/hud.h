#pragma once

#include "common.h"
#include "playermanager.h"

extern CPlayerManager *g_pPlayerManager;

namespace KZ
{
	namespace hud
	{
		void OnProcessUsercmds_Post(CPlayerSlot &slot, bf_read *buf, int numcmds, bool ignore, bool paused);
	}
};


