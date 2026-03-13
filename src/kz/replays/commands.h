#ifndef KZ_REPLAYCOMMANDS_H
#define KZ_REPLAYCOMMANDS_H

#include "sdk/datatypes.h"

class KZPlayer;

namespace KZ::replaysystem::commands
{
	enum class RecordType
	{
		WR,
		WRPro,
		SR,
		SRPro,
		PB,
		PBPro,
		GPB,
		GPBPro,
		SPB,
		SPBPro
	};

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
	void ToggleLegsVisibility(KZPlayer *player);
	void LoadReplayForRecord(KZPlayer *player, RecordType type, const char *courseArg, const char *modeArg);
} // namespace KZ::replaysystem::commands

#endif // KZ_REPLAYCOMMANDS_H
