#include "ctimer.h"

CUtlVector<CTimerBase *> g_PersistentTimers;
CUtlVector<CTimerBase *> g_NonPersistentTimers;

void ProcessTimers()
{
	for (int i = g_PersistentTimers.Count() - 1; i != g_PersistentTimers.InvalidIndex();)
	{
		auto timer = g_PersistentTimers[i];

		int prevIndex = i;
		i--;

		if (timer->lastExecute == -1)
			timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;

		if (timer->lastExecute + timer->interval <= g_pKZUtils->GetGlobals()->curtime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_PersistentTimers.Remove(prevIndex);
			}
			else
			{
				timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;
			}
		}
	}

	for (int i = g_NonPersistentTimers.Count() - 1; i != g_NonPersistentTimers.InvalidIndex();)
	{
		auto timer = g_NonPersistentTimers[i];

		int prevIndex = i;
		i--;

		if (timer->lastExecute == -1)
			timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;

		if (timer->lastExecute + timer->interval <= g_pKZUtils->GetGlobals()->curtime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_NonPersistentTimers.Remove(prevIndex);
			}
			else
			{
				timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;
			}
		}
	}
}

void RemoveNonPersistentTimers()
{
	g_NonPersistentTimers.PurgeAndDeleteElements();
}


