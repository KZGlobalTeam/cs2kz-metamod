#include "ctimer.h"

CUtlVector<CTimerBase *> g_PersistentTimers;
CUtlVector<CTimerBase *> g_NonPersistentTimers;

void ProcessTimers()
{
	for (int i = g_PersistentTimers.Count() - 1; i >= 0; i--)
	{
		auto timer = g_PersistentTimers[i];

		if (timer->lastExecute == -1)
			timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;

		if (timer->lastExecute + timer->interval <= g_pKZUtils->GetGlobals()->curtime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_PersistentTimers.Remove(i);
			}
			else
			{
				timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;
			}
		}
	}

	for (int i = g_NonPersistentTimers.Count() - 1; i >= 0; i--)
	{
		auto timer = g_NonPersistentTimers[i];

		if (timer->lastExecute == -1)
			timer->lastExecute = g_pKZUtils->GetGlobals()->curtime;

		if (timer->lastExecute + timer->interval <= g_pKZUtils->GetGlobals()->curtime)
		{
			if (!timer->Execute())
			{
				delete timer;
				g_NonPersistentTimers.Remove(i);
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


