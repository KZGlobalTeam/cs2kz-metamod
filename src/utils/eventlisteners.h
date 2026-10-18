#pragma once
#include <algorithm>
#include <vector>

#define DECLARE_CLASS_EVENT_LISTENER(listener) \
\
private: \
	static std::vector<listener *> eventListeners; \
\
public: \
	static bool RegisterEventListener(listener *eventListener); \
	static bool UnregisterEventListener(listener *eventListener);

// clang-format off
#define IMPLEMENT_CLASS_EVENT_LISTENER(class, listener) \
	std::vector<listener *> class::eventListeners; \
	bool class::RegisterEventListener(listener *eventListener) \
	{ \
		if (std::find(eventListeners.begin(), eventListeners.end(), eventListener) != eventListeners.end()) \
		{ \
			return false; \
		} \
		eventListeners.push_back(eventListener); \
		return true; \
	} \
	bool class::UnregisterEventListener(listener *eventListener) \
	{ \
		auto it = std::find(eventListeners.begin(), eventListeners.end(), eventListener); \
		if (it == eventListeners.end()) \
		{ \
			return false; \
		} \
		eventListeners.erase(it); \
		return true; \
	}
// clang-format on
