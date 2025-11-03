#ifndef KZ_REPLAYCOMMANDS_H
#define KZ_REPLAYCOMMANDS_H

#include "sdk/datatypes.h"

class KZPlayer;

namespace KZ::replaysystem::commands
{
	// Navigation functions
	void NavigateReplay(KZPlayer *player, u32 targetTick);

	// Command handlers
	void LoadReplay(KZPlayer *player, const char *uuid);
	void CheckReplayLoadProgress(KZPlayer *player);
	void CancelReplayLoad(KZPlayer *player);
	void JumpToReplayTime(KZPlayer *player, const char *input);
	void JumpToReplayTick(KZPlayer *player, const char *input);
	void GetReplayInfo(KZPlayer *player);
	void ToggleReplayPause(KZPlayer *player);
	void ListReplays(KZPlayer *player, const char *input);
} // namespace KZ::replaysystem::commands

#endif // KZ_REPLAYCOMMANDS_H
