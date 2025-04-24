#pragma once
#include "utlvector.h"

#define DECLARE_CLASS_EVENT_LISTENER(listener) \
\
private: \
	static CUtlVector<listener *> eventListeners; \
\
public: \
	static bool RegisterEventListener(listener *eventListener); \
	static bool UnregisterEventListener(listener *eventListener);

// clang-format off
#define IMPLEMENT_CLASS_EVENT_LISTENER(class, listener) \
	CUtlVector<listener *> class::eventListeners; \
	bool class::RegisterEventListener(listener *eventListener) \
	{ \
		if (eventListeners.Find(eventListener) >= 0) \
		{ \
			return false; \
		} \
		eventListeners.AddToTail(eventListener); \
		return true; \
	} \
	bool class::UnregisterEventListener(listener *eventListener) \
	{ \
		return eventListeners.FindAndRemove(eventListener); \
	}
// clang-format on
