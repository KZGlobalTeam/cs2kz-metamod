#ifndef KZ_REPLAYBOT_H
#define KZ_REPLAYBOT_H

#include "sdk/datatypes.h"
#include "kz_replay.h"

class CCSPlayerController;
class KZPlayer;

namespace KZ::replaysystem::bot
{
	// Bot lifecycle management
	void SpawnBot();
	void KickBot();
	void MakeBotAlive();
	void MoveBotToSpec();

	// Bot state management
	CCSPlayerController *GetBot();
	bool IsValidBot(CCSPlayerController *controller);
	KZPlayer *GetBotPlayer();

	// Bot setup and configuration
	void InitializeBotForReplay(const ReplayHeader &header);

	// Bot spectator handling
	void SpectateBot(KZPlayer *spectator);
} // namespace KZ::replaysystem::bot

#endif // KZ_REPLAYBOT_H
