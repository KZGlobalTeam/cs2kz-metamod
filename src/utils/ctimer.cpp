#include "ctimer.h"

CUtlVector<CTimerBase*> g_timers;

void RemoveTimers()
{
	g_timers.PurgeAndDeleteElements();
}

void RemoveMapTimers()
{
	for (int i = g_timers.Count()-1; i != g_timers.InvalidIndex();)
	{
		int prevIndex = i;
		i--;

		if (g_timers[prevIndex]->m_bPreserveMapChange)
			continue;

		delete g_timers[prevIndex];
		g_timers.Remove(prevIndex);
	}
}
