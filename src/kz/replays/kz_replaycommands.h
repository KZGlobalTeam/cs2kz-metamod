#ifndef KZ_REPLAYCOMMANDS_H
#define KZ_REPLAYCOMMANDS_H

#include "sdk/datatypes.h"

class KZPlayer;

namespace KZ::replaysystem::commands
{
	// Navigation functions
	void NavigateReplay(KZPlayer *player, u32 targetTick);

	// Command handlers
	void HandleReplayCommand(KZPlayer *player, const char *uuid);
	void HandleLoadProgressCommand(KZPlayer *player);
	void HandleCancelLoadCommand(KZPlayer *player);
	void HandleGotoCommand(KZPlayer *player, const char *input);
	void HandleGotoTickCommand(KZPlayer *player, const char *input);
	void HandleInfoCommand(KZPlayer *player);
	void HandlePauseCommand(KZPlayer *player);
} // namespace KZ::replaysystem::commands

#endif // KZ_REPLAYCOMMANDS_H
