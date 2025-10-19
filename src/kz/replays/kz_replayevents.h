#ifndef KZ_REPLAYEVENTS_H
#define KZ_REPLAYEVENTS_H

#include "sdk/datatypes.h"
#include "kz_replaydata.h"

class KZPlayer;

namespace KZ::replaysystem::events
{
	// Event processing functions
	void CheckEvents(KZPlayer &player);
	void CheckJumps(KZPlayer &player);

	// Event reprocessing for navigation
	void ReprocessEventsUpToTick(data::ReplayPlayback *replay, u32 targetTick);

	// Specific event handlers
	void HandleTimerEvent(KZPlayer &player, const RpEvent *event, data::ReplayPlayback *replay);
	void HandleCheckpointEvent(const RpEvent *event, data::ReplayPlayback *replay);
	void HandleModeChangeEvent(KZPlayer &player, const RpEvent *event);
	void HandleStyleChangeEvent(KZPlayer &player, const RpEvent *event);
	void HandleTeleportEvent(KZPlayer &player, const RpEvent *event);
} // namespace KZ::replaysystem::events

#endif // KZ_REPLAYEVENTS_H
